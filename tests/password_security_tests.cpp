#include "dc24h/accounts/password_security.hpp"
#include "dc24h/accounts/registration.hpp"
#include "registration_internal.hpp"

#include <openssl/crypto.h>
#include <openssl/evp.h>

#include <algorithm>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <new>
#include <source_location>
#include <thread>
#include <utility>

namespace {
thread_local int fail_after = -1;
thread_local bool kdf_failure = false, fake_kdf = false, scrubbed = true, parameters_ok = true;
thread_local unsigned int kdf_calls = 0, comparison_calls = 0, erased_verifiers = 0;
}
[[gnu::noinline]] void* operator new(std::size_t size) {
    if (fail_after == 0) { fail_after = -1; throw std::bad_alloc(); }
    if (fail_after > 0) { --fail_after; }
    if (auto* pointer = std::malloc(size ? size : 1)) { return pointer; }
    throw std::bad_alloc();
}
[[gnu::noinline]] void* operator new[](std::size_t size) { return ::operator new(size); }
[[gnu::noinline]] void operator delete(void* pointer) noexcept { std::free(pointer); }
[[gnu::noinline]] void operator delete[](void* pointer) noexcept { std::free(pointer); }
[[gnu::noinline]] void operator delete(void* pointer,std::size_t) noexcept { std::free(pointer); }
[[gnu::noinline]] void operator delete[](void* pointer,std::size_t) noexcept { std::free(pointer); }
extern "C" int __real_EVP_PBE_scrypt(const char*,std::size_t,const unsigned char*,std::size_t,
    std::uint64_t,std::uint64_t,std::uint64_t,std::uint64_t,unsigned char*,std::size_t);
extern "C" int __wrap_EVP_PBE_scrypt(const char* password,std::size_t length,const unsigned char* salt,std::size_t salt_length,
    std::uint64_t n,std::uint64_t r,std::uint64_t p,std::uint64_t memory,unsigned char* output,std::size_t size) {
    ++kdf_calls;
    parameters_ok = parameters_ok && n == 131072 && r == 8 && p == 1 && memory == 256ULL*1024*1024 && salt_length == 32 && size == 32;
    if (kdf_failure) { std::fill(output,output+size,0x7f); return 0; }
    if (fake_kdf) { std::fill(output,output+size,0x42); return 1; }
    return __real_EVP_PBE_scrypt(password,length,salt,salt_length,n,r,p,memory,output,size);
}
extern "C" int __real_CRYPTO_memcmp(const void*,const void*,std::size_t);
extern "C" int __wrap_CRYPTO_memcmp(const void* left,const void* right,std::size_t size) {
    ++comparison_calls;
    parameters_ok = parameters_ok && size == 32;
    return __real_CRYPTO_memcmp(left,right,size);
}
extern "C" void __real_OPENSSL_cleanse(void*,std::size_t);
extern "C" void __wrap_OPENSSL_cleanse(void* pointer,std::size_t size) {
    __real_OPENSSL_cleanse(pointer,size);
    const auto* bytes = static_cast<const unsigned char*>(pointer);
    scrubbed = scrubbed && std::all_of(bytes,bytes+size,[](auto value) { return value == 0; });
    if (size == dc24h::accounts::detail::verifier_bytes) { ++erased_verifiers; }
}
namespace {
using namespace dc24h::accounts;
using dc24h::database::Connection;
using dc24h::database::Options;
constexpr std::string_view password = "Registration test password 42!";
void require(bool ok,std::source_location where = std::source_location::current()) {
    if (!ok) { throw std::runtime_error("Password security assertion at line "+std::to_string(where.line())); }
}
template<class F> void rejects(PasswordSecurityErrorKind kind,F&& action,
    std::source_location where = std::source_location::current()) {
    try { action(); }
    catch (const PasswordSecurityError& error) {
        require(error.kind() == kind,where);
        require(std::string_view(error.what()).find(password) == std::string_view::npos,where);
        require(std::string_view(error.what()).find("$dc24h-scrypt$") == std::string_view::npos,where);
        return;
    }
    require(false,where);
}
std::string fixture() {
    return std::string(detail::verifier_prefix)+
        "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f$"
        "192bc7f7e9372268cb84aebfde477de2d20fd86c1ccf076d981d1b55fc799aed";
}
void unit() {
    // Independently computed hashlib.scrypt fixture, not a hash/verify round trip.
    const auto record = fixture();
    require(detail::verify_password(password,record));
    require(!detail::verify_password("Registration test password 43!",record));
    const auto compared = comparison_calls;
    require(compared == 2);
    for (std::size_t i = 0; i < record.size(); ++i) {
        auto malformed = record; malformed[i] = '!';
        const auto calls = kdf_calls;
        rejects(PasswordSecurityErrorKind::invalid_record,[&] { (void)detail::verify_password(password,malformed); });
        require(kdf_calls == calls && comparison_calls == compared);
    }
    for (const auto& bad : {std::string{},record.substr(1),record+'x',std::string(4096,'x'),
                           std::string(detail::verifier_prefix)+std::string(129,'F')}) {
        const auto calls = kdf_calls;
        rejects(PasswordSecurityErrorKind::invalid_record,[&] { (void)detail::verify_password(password,bad); });
        require(kdf_calls == calls);
    }
    auto unsafe_cost = record; unsafe_cost.replace(unsafe_cost.find("ln=17"),5,"ln=99");
    const auto calls = kdf_calls;
    rejects(PasswordSecurityErrorKind::invalid_record,[&] { (void)detail::verify_password(password,unsafe_cost); });
    require(kdf_calls == calls);
    auto nul = record; nul.back() = '\0';
    rejects(PasswordSecurityErrorKind::invalid_record,[&] { (void)detail::verify_password(password,nul); });
    kdf_failure = true;
    try { (void)detail::verify_password(password,record); require(false); }
    catch (const RegistrationError& error) { require(error.kind() == RegistrationErrorKind::crypto); }
    kdf_failure = false;
    require(comparison_calls == compared && scrubbed && parameters_ok);
    // Password bytes, whitespace and Unicode composition remain significant.
    for (const auto& pair : {
        std::pair{std::string("  A long password 42!  "),std::string("A long password 42!")},
        std::pair{std::string("A long e\xcc\x81 password!"),std::string("A long é password!")},
        std::pair{std::string(1024,'x'),std::string(1023,'x')},
        std::pair{std::string(15,' '),std::string(15,'x')}}) {
        detail::EncodedPassword encoded;
        detail::hash_password(pair.first,encoded);
        const std::string_view view(reinterpret_cast<const char*>(encoded.bytes.data()),encoded.bytes.size());
        require(detail::verify_password(pair.first,view));
        require(!detail::verify_password(pair.second,view));
    }
    require(scrubbed && parameters_ok);
}
void allocation_failures(const Options& options,AccountId account) {
    Connection observer(options);
    // Only fault sweeps replace KDF work; normal unit/database tests use real scrypt.
    // Fake KDF writes 0x42, whose lowercase hex is 42 repeated.
    auto fixed = std::string(detail::verifier_prefix)+std::string(64,'0')+'$';
    for (std::size_t i = 0; i < 32; ++i) { fixed += "42"; }
    observer.execute("UPDATE dc24h_account_credentials SET verifier=? WHERE account_id=?",{fixed,std::to_string(account.value())});
    int failures = 0; bool success = false;
    for (int index = 0; index < 768 && !success; ++index) {
        Connection c(options); PasswordSecurityService service(c);
        fake_kdf = true; fail_after = index;
        try {
            const auto result = service.verify(account,password);
            fail_after = -1; require(result == PasswordVerification::matched); success = true;
        } catch (const std::bad_alloc&) { fail_after = -1; ++failures; }
        fake_kdf = false;
        require(!c.in_transaction());
        require(observer.scalar("SELECT verifier FROM dc24h_account_credentials WHERE account_id=?",{std::to_string(account.value())}) == fixed);
    }
    require(success && failures > 30 && scrubbed && parameters_ok);
}
void integration(const Options& options) {
    Connection c(options); RegistrationService registration(c); AccountRepository accounts(c); PasswordSecurityService service(c);
    const auto one = registration.create_account("PasswordProtected",password,42);
    const auto missing = accounts.insert({"PasswordMetadataOnly"});
    auto checked = service.verify(one.account.id(),password);
    require(checked == PasswordVerification::matched);
    require(service.verify(one.account.id(),"Registration test password 43!") == PasswordVerification::mismatch);
    const auto before = kdf_calls;
    require(service.verify(missing.account.id(),password) == PasswordVerification::unavailable);
    require(service.verify(*AccountId::from_value(9223372036854775807ULL),password) == PasswordVerification::unavailable);
    require(kdf_calls == before && accounts.find_by_id(one.account.id()) == one && !c.in_transaction());
    for (const auto& bad : {std::string{},std::string(14,'x'),std::string(1025,'x'),
                           std::string(15,'x')+'\0',std::string(15,'x')+'\xff'}) {
        rejects(PasswordSecurityErrorKind::invalid_password,[&] { (void)service.verify(one.account.id(),bad); });
    }
    require(kdf_calls == before);
    { auto transaction = c.transaction();
      try { (void)service.verify(one.account.id(),password); require(false); }
      catch (const dc24h::database::Error& error) { require(error.kind() == dc24h::database::ErrorKind::transaction); }
      transaction.rollback(); }
    std::exception_ptr thread_error;
    std::jthread thread([&] {
        try { (void)service.verify(one.account.id(),password); require(false); }
        catch (const dc24h::database::Error& error) {
            if (error.kind() != dc24h::database::ErrorKind::wrong_thread) { thread_error = std::current_exception(); }
        } catch (...) { thread_error = std::current_exception(); }
    });
    thread.join(); if (thread_error) { std::rethrow_exception(thread_error); }
    const auto erased = erased_verifiers;
    kdf_failure = true;
    rejects(PasswordSecurityErrorKind::crypto,[&] { (void)service.verify(one.account.id(),password); });
    kdf_failure = false;
    require(erased_verifiers > erased && scrubbed);
    const auto original = c.scalar("SELECT verifier FROM dc24h_account_credentials WHERE account_id=?",{std::to_string(one.account.id().value())});
    require(original.has_value());
    auto malformed = *original; malformed[0] = '!';
    c.execute("UPDATE dc24h_account_credentials SET verifier=? WHERE account_id=?",{malformed,std::to_string(one.account.id().value())});
    const auto calls = kdf_calls;
    rejects(PasswordSecurityErrorKind::invalid_record,[&] { (void)service.verify(one.account.id(),password); });
    require(kdf_calls == calls && erased_verifiers > erased+1);
    c.execute("UPDATE dc24h_account_credentials SET verifier=? WHERE account_id=?",{*original,std::to_string(one.account.id().value())});
    c.execute("ALTER TABLE dc24h_account_credentials DROP FOREIGN KEY dc24h_credentials_account");
    rejects(PasswordSecurityErrorKind::schema,[&] { (void)service.verify(one.account.id(),password); });
    c.execute("ALTER TABLE dc24h_account_credentials ADD CONSTRAINT dc24h_credentials_account FOREIGN KEY(account_id) REFERENCES dc24h_accounts(id) ON DELETE RESTRICT ON UPDATE RESTRICT");
    auto limited_options = options; limited_options.user = "password_limited";
    { Connection limited(limited_options); PasswordSecurityService reader(limited);
      require(reader.verify(one.account.id(),password) == PasswordVerification::matched); }
    limited_options.user = "password_no_credentials";
    { Connection limited(limited_options); PasswordSecurityService reader(limited);
      rejects(PasswordSecurityErrorKind::schema,[&] { (void)reader.verify(one.account.id(),password); }); }
    const auto allocation = registration.create_account("PasswordAllocation",password,42);
    allocation_failures(options,allocation.account.id());
    require(accounts.find_by_id(one.account.id()) == one && scrubbed && parameters_ok);
}
void read_only(const Options& options) {
    Connection c(options); AccountRepository accounts(c); PasswordSecurityService service(c);
    const auto account = accounts.find_by_name(*AccountName::create("PasswordProtected")); require(account.has_value());
    require(service.verify(account->account.id(),password) == PasswordVerification::matched);
    require(accounts.find_by_id(account->account.id()) == account && !c.in_transaction());
}
}
int main(int argc,char* argv[]) {
    try {
        if (argc == 1) { unit(); }
        else if (argc == 3 || argc == 4) {
            Options options; options.enabled = true; options.socket = argv[1]; options.password_file = argv[2];
            if (argc == 4) { options.user = "runtime"; read_only(options); }
            else { integration(options); }
        } else { throw std::runtime_error("Invalid password security test arguments"); }
        std::cout << "Password security checks passed\n";
    } catch (const std::exception& error) { fail_after = -1; std::cerr << error.what() << '\n'; return 1; }
}
