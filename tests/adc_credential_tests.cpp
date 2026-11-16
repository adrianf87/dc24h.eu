#include "dc24h/accounts/adc_authentication.hpp"
#include "dc24h/accounts/identity_mapping.hpp"
#include "dc24h/accounts/password_security.hpp"
#include "dc24h/accounts/registration.hpp"
#include <openssl/crypto.h>
#include <openssl/rand.h>
#include <sys/stat.h>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <source_location>

namespace { bool random_failure = false, scrubbed = true; }
extern "C" int __real_RAND_bytes(unsigned char*,int);
extern "C" int __wrap_RAND_bytes(unsigned char* data,int size) { return random_failure ? 0 : __real_RAND_bytes(data,size); }
extern "C" void __real_OPENSSL_cleanse(void*,std::size_t);
extern "C" void __wrap_OPENSSL_cleanse(void* data,std::size_t size) {
    __real_OPENSSL_cleanse(data,size); const auto* bytes = static_cast<const unsigned char*>(data);
    scrubbed = scrubbed && std::all_of(bytes,bytes+size,[](auto value) { return value == 0; });
}
namespace {
using namespace dc24h::accounts;
using dc24h::database::Connection;
using dc24h::database::Options;
constexpr std::string_view account_password = "Account ownership password 42!";
constexpr std::string_view adc_password = "ADC wire password 42!";
void require(bool value,std::source_location where = std::source_location::current()) {
    if (!value) { throw std::runtime_error("ADC credential assertion at line "+std::to_string(where.line())); }
}
template<class F> void rejects(AdcCredentialErrorKind kind,F&& action,std::source_location where = std::source_location::current()) {
    try { action(); }
    catch (const AdcCredentialError& error) {
        require(error.kind() == kind,where);
        require(std::string_view(error.what()).find(adc_password) == std::string_view::npos,where); return;
    }
    require(false,where);
}
std::string cid(unsigned seed) {
    dc24h::adc::TigerDigest pid{};
    for (std::size_t i = 0; i < pid.size(); ++i) { pid[i] = static_cast<unsigned char>(i+seed); }
    return dc24h::adc::tiger_base32(*dc24h::adc::tiger192(pid));
}
StoredAccount create(Connection& c,std::string name,std::string nickname,unsigned seed,bool active = true) {
    auto account = RegistrationService(c).create_account(name,account_password,42);
    if (active) {
        auto changed = account.account.with_access(Role::registered,Status::active,42);
        account = AccountRepository(c).update(account,std::get<Account>(changed));
    }
    (void)IdentityRepository(c).bind(account.account.id(),*IdentityKey::create(nickname,cid(seed)),42);
    return account;
}
void integration(const Options& options,const std::string& key_path) {
    Connection c(options); const auto key = AdcSecretKey::load(key_path); AdcCredentialService service(c,*key);
    const auto one = create(c,"AuthAccountOne","RegisteredADC",7);
    const auto disabled = create(c,"AuthAccountDisabled","DisabledADC",8,false);
    const auto absent = create(c,"AuthAccountUnenrolled","MissingADC",9);
    service.enroll(one.account.id(),account_password,adc_password,42);
    service.enroll(disabled.account.id(),account_password,adc_password,42);
    auto protected_value = c.scalar("SELECT secret FROM dc24h_account_adc_credentials WHERE account_id=?",{std::to_string(one.account.id().value())});
    require(protected_value && protected_value->find(adc_password) == std::string::npos && protected_value->find(account_password) == std::string::npos);
    const auto other = c.scalar("SELECT secret FROM dc24h_account_adc_credentials WHERE account_id=?",{std::to_string(disabled.account.id().value())});
    require(other && *other != *protected_value);
    require(PasswordSecurityService(c).verify(one.account.id(),account_password) == PasswordVerification::matched);
    require(PasswordSecurityService(c).verify(one.account.id(),adc_password) == PasswordVerification::mismatch);
    require(AccountRepository(c).find_by_id(one.account.id()) == one);
    rejects(AdcCredentialErrorKind::conflict,[&] { service.enroll(one.account.id(),account_password,adc_password,42); });
    rejects(AdcCredentialErrorKind::proof,[&] { service.enroll(absent.account.id(),"Wrong ownership password!",adc_password,42); });
    random_failure = true;
    rejects(AdcCredentialErrorKind::crypto,[&] { service.enroll(absent.account.id(),account_password,adc_password,42); });
    random_failure = false;
    require(c.scalar("SELECT COUNT(*) FROM dc24h_account_adc_credentials WHERE account_id=?",{std::to_string(absent.account.id().value())}) == "0");
    { auto transaction = c.transaction();
      try { service.enroll(absent.account.id(),account_password,adc_password,42); require(false); }
      catch (const dc24h::database::Error& error) { require(error.kind() == dc24h::database::ErrorKind::transaction); }
      transaction.rollback(); }
    const auto source = AdcAuthenticationDirectory::load(c,key);
    { dc24h::adc::PasswordChallenge challenge;
      require(std::holds_alternative<dc24h::adc::PasswordAuthentication>(source->prepare("RegisteredADC",cid(7),dc24h::users::Transport::adcs,challenge))); }
    for (const auto& pair : {std::pair{"RegisteredADC",cid(8)},std::pair{"OtherNick",cid(7)},std::pair{"DisabledADC",cid(8)},std::pair{"MissingADC",cid(9)}}) {
        dc24h::adc::PasswordChallenge challenge;
        require(std::holds_alternative<dc24h::adc::DeniedAuthentication>(source->prepare(pair.first,pair.second,dc24h::users::Transport::adcs,challenge)));
    }
    { dc24h::adc::PasswordChallenge challenge;
      require(std::holds_alternative<dc24h::adc::DeniedAuthentication>(source->prepare("RegisteredADC",cid(7),dc24h::users::Transport::adc,challenge))); }
    { dc24h::adc::PasswordChallenge challenge;
      require(std::holds_alternative<dc24h::adc::GuestAuthentication>(source->prepare("Guest",cid(100),dc24h::users::Transport::adc,challenge))); }
    require(source->allows_identity("RegisteredADC",cid(7),one.account.id().value()));
    require(!source->allows_identity("Changed",cid(7),one.account.id().value()));
    require(!source->allows_identity("RegisteredADC",cid(7),{}));
    rejects(AdcCredentialErrorKind::invalid_record,[&] { (void)AdcAuthenticationDirectory::load(c,key,1); });
    // Nonce, ciphertext and tag tampering all fail authentication before serving clients.
    for (const auto position : {std::size_t{9},std::size_t{22},protected_value->size()-1}) {
        auto changed = *protected_value; changed[position] ^= 1;
        c.execute("UPDATE dc24h_account_adc_credentials SET secret=? WHERE account_id=?",{changed,std::to_string(one.account.id().value())});
        rejects(AdcCredentialErrorKind::crypto,[&] { (void)AdcAuthenticationDirectory::load(c,key); });
        require(!c.in_transaction());
    }
    c.execute("UPDATE dc24h_account_adc_credentials SET secret=?,created_at=43 WHERE account_id=?",{*protected_value,std::to_string(one.account.id().value())});
    rejects(AdcCredentialErrorKind::crypto,[&] { (void)AdcAuthenticationDirectory::load(c,key); });
    c.execute("UPDATE dc24h_account_adc_credentials SET secret=?,created_at=42 WHERE account_id=?",{*other,std::to_string(one.account.id().value())});
    rejects(AdcCredentialErrorKind::crypto,[&] { (void)AdcAuthenticationDirectory::load(c,key); });
    c.execute("UPDATE dc24h_account_adc_credentials SET secret=? WHERE account_id=?",{*protected_value,std::to_string(one.account.id().value())});
    const auto wrong_path = key_path+".wrong";
    { std::ofstream file(wrong_path); file << std::string(64,'f') << '\n'; }
    require(::chmod(wrong_path.c_str(),0600) == 0); const auto wrong = AdcSecretKey::load(wrong_path);
    rejects(AdcCredentialErrorKind::crypto,[&] { (void)AdcAuthenticationDirectory::load(c,wrong); });
    require(::chmod(key_path.c_str(),0644) == 0);
    bool rejected = false; try { (void)AdcSecretKey::load(key_path); } catch (const std::runtime_error&) { rejected = true; }
    require(::chmod(key_path.c_str(),0600) == 0); require(rejected);
    auto limited_options = options; limited_options.user = "adc_runtime";
    { Connection limited(limited_options); require(AdcAuthenticationDirectory::load(limited,key) != nullptr); }
    require(AccountRepository(c).find_by_id(one.account.id()) == one && scrubbed);
}
}
int main(int argc,char* argv[]) {
    try {
        if (argc != 4) { throw std::runtime_error("Invalid ADC credential test arguments"); }
        Options options; options.enabled = true; options.socket = argv[1]; options.password_file = argv[2];
        integration(options,argv[3]); std::cout << "ADC credential checks passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
