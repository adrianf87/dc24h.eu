#include "dc24h/accounts/registration.hpp"
#include "registration_internal.hpp"

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#include <algorithm>
#include <barrier>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <new>
#include <source_location>
#include <thread>

namespace {
thread_local int fail_after = -1;
thread_local bool random_failure = false, kdf_failure = false, deterministic = false, fake_kdf = false;
thread_local bool scrubbed = true;
}
[[gnu::noinline]] void* operator new(std::size_t n) {
    if (fail_after == 0) { fail_after = -1; throw std::bad_alloc(); }
    if (fail_after > 0) { --fail_after; }
    if (auto* p = std::malloc(n ? n : 1)) { return p; }
    throw std::bad_alloc();
}
[[gnu::noinline]] void* operator new[](std::size_t n) { return ::operator new(n); }
[[gnu::noinline]] void operator delete(void* p) noexcept { std::free(p); }
[[gnu::noinline]] void operator delete[](void* p) noexcept { std::free(p); }
[[gnu::noinline]] void operator delete(void* p,std::size_t) noexcept { std::free(p); }
[[gnu::noinline]] void operator delete[](void* p,std::size_t) noexcept { std::free(p); }
extern "C" int __real_RAND_bytes(unsigned char*,int);
extern "C" int __wrap_RAND_bytes(unsigned char* bytes,int count) {
    if (random_failure) { return 0; }
    if (!deterministic) { return __real_RAND_bytes(bytes,count); }
    for (int i = 0; i < count; ++i) { bytes[i] = static_cast<unsigned char>(i); }
    return 1;
}
extern "C" int __real_EVP_PBE_scrypt(const char*,std::size_t,const unsigned char*,std::size_t,
    std::uint64_t,std::uint64_t,std::uint64_t,std::uint64_t,unsigned char*,std::size_t);
extern "C" int __wrap_EVP_PBE_scrypt(const char* password,std::size_t length,const unsigned char* salt,std::size_t salt_length,
    std::uint64_t n,std::uint64_t r,std::uint64_t p,std::uint64_t memory,unsigned char* out,std::size_t size) {
    if (kdf_failure) { return 0; }
    if (fake_kdf) { std::fill(out,out+size,0x42); return 1; }
    return __real_EVP_PBE_scrypt(password,length,salt,salt_length,n,r,p,memory,out,size);
}
extern "C" void __real_OPENSSL_cleanse(void*,std::size_t);
extern "C" void __wrap_OPENSSL_cleanse(void* pointer,std::size_t size) {
    __real_OPENSSL_cleanse(pointer,size);
    const auto* bytes = static_cast<const unsigned char*>(pointer);
    scrubbed = scrubbed && std::all_of(bytes,bytes+size,[](auto value) { return value == 0; });
}
namespace {
using namespace dc24h::accounts;
using dc24h::database::Connection;
using dc24h::database::Options;
constexpr std::string_view password = "Registration test password 42!";
void require(bool ok,std::source_location location = std::source_location::current()) {
    if (!ok) { throw std::runtime_error("Registration assertion at line "+std::to_string(location.line())); }
}
template<class F> void rejects(RegistrationErrorKind kind,F&& action,
    std::source_location location = std::source_location::current()) {
    try { action(); } catch (const RegistrationError& error) { require(error.kind() == kind,location); return; }
    require(false,location);
}
void unit() {
    detail::validate_password(std::string(15,' '));
    detail::validate_password(std::string(1024,'x'));
    detail::validate_password("żółćżółćżółćżół"); // 15 Unicode scalar values.
    for (const auto& text : {std::string{},std::string(14,'x'),std::string(1025,'x'),
        std::string(15,'x')+'\0',std::string(15,'x')+'\n',std::string(15,'x')+'\x7f',
        std::string(15,'x')+'\xff',std::string("żółćżółćżółćżó")}) {
        rejects(RegistrationErrorKind::invalid_password,[&] { detail::validate_password(text); });
    }
    detail::EncodedPassword encoded;
    deterministic = true;
    detail::hash_password(password,encoded);
    const std::string_view value(reinterpret_cast<const char*>(encoded.bytes.data()),encoded.bytes.size());
    require(value.starts_with(detail::verifier_prefix));
    // Independent hashlib.scrypt fixture with salt bytes 0..31 and fixed production cost.
    require(value.ends_with("192bc7f7e9372268cb84aebfde477de2d20fd86c1ccf076d981d1b55fc799aed"));
    random_failure = true;
    rejects(RegistrationErrorKind::crypto,[&] { detail::hash_password(password,encoded); });
    require(std::all_of(encoded.bytes.begin(),encoded.bytes.end(),[](auto x) { return x == 0; }));
    random_failure = false; kdf_failure = true;
    rejects(RegistrationErrorKind::crypto,[&] { detail::hash_password(password,encoded); });
    kdf_failure = false; deterministic = false;
    require(scrubbed);
}
void allocation_failures(const Options& options) {
    Connection observer(options); AccountRepository reader(observer);
    int failures = 0; bool success = false;
    // Fake KDF only for exhaustive C++ allocation faults. Real KDF is tested
    // separately; production exposes no injection or reduced cost setting.
    for (int index = 0; index < 768 && !success; ++index) {
        Connection c(options); RegistrationService registration(c);
        fake_kdf = true; fail_after = index;
        try { (void)registration.create_account("AllocationRegistration",password,1); fail_after = -1; success = true; }
        catch (const std::bad_alloc&) { fail_after = -1; ++failures; }
        fake_kdf = false;
        require(!c.in_transaction());
        const auto count = observer.scalar("SELECT COUNT(*) FROM dc24h_accounts WHERE name='AllocationRegistration'");
        const auto credentials = observer.scalar("SELECT COUNT(*) FROM dc24h_account_credentials c JOIN dc24h_accounts a ON a.id=c.account_id WHERE a.name='AllocationRegistration'");
        require(count == (success ? "1" : "0") && credentials == count);
    }
    require(success && failures > 50 && scrubbed);
}
void integration(const Options& options) {
    Connection c(options); RegistrationService registration(c); AccountRepository repository(c);
    const auto one = registration.create_account("RegisteredOne",password,42);
    const auto two = registration.create_account("RegisteredTwo",password,42);
    require(one.account.role() == Role::registered && one.account.status() == Status::disabled);
    require(one.account.times() == Times{42,42,{}} && one.revision == 1 && one.account.id() != two.account.id());
    require(repository.find_by_id(one.account.id()) == one);
    const auto first = c.scalar("SELECT verifier FROM dc24h_account_credentials WHERE account_id=?",{std::to_string(one.account.id().value())});
    const auto second = c.scalar("SELECT verifier FROM dc24h_account_credentials WHERE account_id=?",{std::to_string(two.account.id().value())});
    require(first && second && *first != *second && first->find(password) == std::string::npos);
    const auto metadata = repository.insert({"ExistingAdmin",Role::administrator,Status::active,{1,1,{}}});
    try { (void)registration.create_account("ExistingAdmin",password,2); require(false); }
    catch (const RepositoryError& error) { require(error.kind() == RepositoryErrorKind::duplicate); }
    require(repository.find_by_id(metadata.account.id()) == metadata);
    require(c.scalar("SELECT COUNT(*) FROM dc24h_account_credentials WHERE account_id=?",{std::to_string(metadata.account.id().value())}) == "0");
    rejects(RegistrationErrorKind::invalid_name,[&] { (void)registration.create_account("bad name",password,1); });
    rejects(RegistrationErrorKind::invalid_password,[&] { (void)registration.create_account("InvalidPassword","short",1); });
    for (const auto time : {Timestamp{-1},max_timestamp+1}) {
        rejects(RegistrationErrorKind::invalid_time,[&] { (void)registration.create_account("InvalidTime",password,time); });
    }
    for (int fault = 0; fault < 2; ++fault) {
        random_failure = fault == 0; kdf_failure = fault == 1;
        rejects(RegistrationErrorKind::crypto,[&] { (void)registration.create_account("FailedCrypto",password,1); });
        random_failure = kdf_failure = false;
        require(!repository.find_by_name(*AccountName::create("FailedCrypto")) && !c.in_transaction());
    }
    auto limited_options = options; limited_options.user = "registration_limited";
    { Connection limited(limited_options); RegistrationService service(limited);
      try { (void)service.create_account("CredentialDenied",password,1); require(false); }
      catch (const dc24h::database::Error& error) { require(!error.outcome_unknown()); }
      require(!limited.in_transaction()); }
    require(!repository.find_by_name(*AccountName::create("CredentialDenied")));
    { auto transaction = c.transaction();
      try { (void)registration.create_account("Nested",password,1); require(false); }
      catch (const dc24h::database::Error& error) { require(error.kind() == dc24h::database::ErrorKind::transaction); }
      transaction.rollback(); }
    // Two real KDFs and independent sessions contend on the same exact name.
    std::barrier gate(2); int wins[2]{}, duplicates[2]{}; std::exception_ptr errors[2];
    auto race = [&](int index) {
        try {
            Connection db(options); RegistrationService service(db); gate.arrive_and_wait();
            try { (void)service.create_account("ConcurrentRegistration",password,1); wins[index] = 1; }
            catch (const RepositoryError& error) { require(error.kind() == RepositoryErrorKind::duplicate); duplicates[index] = 1; }
        } catch (...) { errors[index] = std::current_exception(); gate.arrive_and_drop(); }
    };
    std::jthread a(race,0),b(race,1); a.join(); b.join();
    for (auto error : errors) { if (error) { std::rethrow_exception(error); } }
    require(wins[0]+wins[1] == 1 && duplicates[0]+duplicates[1] == 1);
    require(c.scalar("SELECT COUNT(*) FROM dc24h_account_credentials c JOIN dc24h_accounts a ON c.account_id=a.id WHERE a.name='ConcurrentRegistration'") == "1");
    c.execute("ALTER TABLE dc24h_account_credentials DROP FOREIGN KEY dc24h_credentials_account");
    rejects(RegistrationErrorKind::schema,[&] { (void)registration.create_account("BadSchema",password,1); });
    c.execute("ALTER TABLE dc24h_account_credentials ADD CONSTRAINT dc24h_credentials_account FOREIGN KEY(account_id) REFERENCES dc24h_accounts(id) ON DELETE RESTRICT ON UPDATE RESTRICT");
    require(!repository.find_by_name(*AccountName::create("BadSchema")));
    (void)registration.create_account("Żółć'\\;--",password,max_timestamp);
    allocation_failures(options);
    require(scrubbed);
}
void readonly(const Options& options) {
    Connection c(options); RegistrationService service(c); AccountRepository repository(c);
    require(repository.find_by_name(*AccountName::create("RegisteredOne")).has_value());
    try { (void)service.create_account("ReadonlyRegistration",password,1); require(false); }
    catch (const dc24h::database::Error& error) { require(!error.outcome_unknown()); }
    require(!c.in_transaction() && !repository.find_by_name(*AccountName::create("ReadonlyRegistration")));
}
}
int main(int argc,char* argv[]) {
    try {
        if (argc == 1) { unit(); }
        else {
            if (argc != 3 && argc != 4) { throw std::runtime_error("Invalid registration test arguments"); }
            Options options; options.enabled = true; options.socket = argv[1]; options.password_file = argv[2];
            if (argc == 4) { options.user = "runtime"; readonly(options); }
            else { integration(options); }
        }
        std::cout << "Registration checks passed\n";
    } catch (const std::exception& error) { fail_after = -1; std::cerr << error.what() << '\n'; return 1; }
}
