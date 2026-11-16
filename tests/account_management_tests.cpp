#include "dc24h/accounts/registration.hpp"
#include "dc24h/accounts/management.hpp"
#include "dc24h/accounts/adc_authentication.hpp"
#include "dc24h/accounts/identity_mapping.hpp"
#include "dc24h/accounts/password_security.hpp"
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
#include <functional>

namespace {
thread_local int fail_after = -1;
thread_local bool random_failure = false, kdf_failure = false, deterministic = false, fake_kdf = false;
thread_local bool scrubbed = true;
thread_local std::function<void()> after_kdf;
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
    int result = 1;
    if (fake_kdf) { std::fill(out,out+size,0x42); }
    else { result = __real_EVP_PBE_scrypt(password,length,salt,salt_length,n,r,p,memory,out,size); }
    if (result && after_kdf) { auto callback = std::move(after_kdf); after_kdf = {}; callback(); }
    return result;
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
constexpr std::string_view original = "Management original password 42!";
constexpr std::string_view replacement = "Management replacement password 42!";
constexpr std::string_view reset = "Management reset password 42!";
constexpr std::string_view adc_original = "Management old ADC password 42!";
void require(bool value, std::source_location where = std::source_location::current()) {
    if (!value) { throw std::runtime_error("Account management assertion at line "+std::to_string(where.line())); }
}
template<class F> void rejects(ManagementErrorKind kind,F&& action,std::source_location where = std::source_location::current()) {
    try { action(); }
    catch (const ManagementError& error) {
        require(error.kind() == kind,where);
        for (auto secret : {original,replacement,reset,adc_original}) { require(std::string_view(error.what()).find(secret) == std::string_view::npos,where); }
        return;
    }
    require(false,where);
}
StoredAccount create(Connection& c,std::string name,Role role = Role::registered,Status status = Status::active) {
    auto stored = RegistrationService(c).create_account(name,original,10);
    return AccountRepository(c).update(stored,std::get<Account>(stored.account.with_access(role,status,10)));
}
std::string cid(unsigned seed) {
    dc24h::adc::TigerDigest pid{};
    for (std::size_t i=0;i<pid.size();++i) { pid[i]=static_cast<unsigned char>(i+seed); }
    return dc24h::adc::tiger_base32(*dc24h::adc::tiger192(pid));
}
void enroll(Connection& c,const AdcSecretKey& key,const StoredAccount& account,std::string_view proof,
    std::string_view nickname,unsigned seed,Timestamp time,std::string_view password = adc_original) {
    IdentityRepository identities(c);
    if (!identities.find_by_account(account.account.id())) {
        (void)identities.bind(account.account.id(),*IdentityKey::create(nickname,cid(seed)),time);
    }
    AdcCredentialService(c,key).enroll(account.account.id(),proof,password,time);
}
std::string verifier(Connection& c,const StoredAccount& account) {
    return *c.scalar("SELECT verifier FROM dc24h_account_credentials WHERE account_id=?",{std::to_string(account.account.id().value())});
}
std::string adc_count(Connection& c,const StoredAccount& account) {
    return *c.scalar("SELECT COUNT(*) FROM dc24h_account_adc_credentials WHERE account_id=?",{std::to_string(account.account.id().value())});
}
void denied_directory(Connection& c,std::shared_ptr<const AdcSecretKey> key,std::string_view nick,unsigned seed) {
    dc24h::adc::PasswordChallenge challenge;
    const auto directory=AdcAuthenticationDirectory::load(c,std::move(key));
    require(std::holds_alternative<dc24h::adc::DeniedAuthentication>(directory->prepare(nick,cid(seed),dc24h::users::Transport::adcs,challenge)));
}
void allocation_failures(const Options& options,std::shared_ptr<const AdcSecretKey> key) {
    Connection observer(options); fake_kdf=true;
    const auto expected=create(observer,"ManagementAllocation");
    enroll(observer,*key,expected,original,"ManagementAllocationADC",51,10);
    const auto before=verifier(observer,expected);
    int failures=0; bool success=false;
    for (int index=0;index<1536 && !success;++index) {
        Connection c(options); AccountManagementService service(c); fail_after=index;
        try { (void)service.change_password(expected,original,replacement,20); fail_after=-1;success=true; }
        catch (const std::bad_alloc&) { fail_after=-1;++failures; }
        require(!c.in_transaction());
        const auto current=AccountRepository(observer).find_by_id(expected.account.id());
        require(current && current->revision==expected.revision+(success ? 1U : 0U));
        if (!success) { require(*current==expected && verifier(observer,expected)==before); }
        require(adc_count(observer,expected)==(success ? "0" : "1"));
    }
    fake_kdf=false; require(success && failures>100 && scrubbed);
}
void integration(const Options& options,const std::string& key_path) {
    Connection c(options); AccountRepository repository(c); AccountManagementService service(c);
    const auto key=AdcSecretKey::load(key_path);
    const auto admin=create(c,"ManagementAdmin",Role::administrator);
    const auto moderator=create(c,"ManagementModerator",Role::moderator);
    const auto other_admin=create(c,"ManagementOtherAdmin",Role::administrator);
    const auto disabled_admin=create(c,"ManagementDisabledAdmin",Role::administrator,Status::disabled);
    auto user=create(c,"ManageŻółć'\\;--");
    user=repository.update(user,std::get<Account>(user.account.with_last_login(10)));
    enroll(c,*key,user,original,"ManagementADC",31,10);
    const auto before=verifier(c,user);
    rejects(ManagementErrorKind::unauthorized,[&]{ (void)service.change_password(user,"Wrong management password!",replacement,20); });
    rejects(ManagementErrorKind::invalid_password,[&]{ (void)service.change_password(user,original,"short",20); });
    rejects(ManagementErrorKind::invalid_password,[&]{ (void)service.change_password(user,original,original,20); });
    rejects(ManagementErrorKind::invalid_input,[&]{ (void)service.change_password(user,original,replacement,9); });
    for (auto fault : {0,1}) {
        random_failure=fault==0;kdf_failure=fault==1;
        rejects(ManagementErrorKind::crypto,[&]{ (void)service.change_password(user,original,replacement,20); });
        random_failure=kdf_failure=false;
        require(repository.find_by_id(user.account.id())==user && verifier(c,user)==before && adc_count(c,user)=="1");
    }
    const auto changed=service.change_password(user,original,replacement,20);
    require(changed.account.status()==Status::active && changed.account.role()==user.account.role() && changed.revision==user.revision+1);
    require(changed.account.name()==user.account.name() && changed.account.times()==Times{10,20,{10}});
    require(verifier(c,changed)!=before && adc_count(c,changed)=="0");
    require(PasswordSecurityService(c).verify(changed.account.id(),original)==PasswordVerification::mismatch);
    require(PasswordSecurityService(c).verify(changed.account.id(),replacement)==PasswordVerification::matched);
    denied_directory(c,key,"ManagementADC",31);
    rejects(ManagementErrorKind::stale,[&]{ (void)service.change_password(user,original,reset,20); });
    for (const auto& actor : {moderator,disabled_admin}) {
        rejects(ManagementErrorKind::unauthorized,[&]{ (void)service.disable_account(actor,original,changed,30); });
    }
    rejects(ManagementErrorKind::unauthorized,[&]{ (void)service.reset_password(admin,"Wrong administrator password!",changed,reset,30); });
    rejects(ManagementErrorKind::unauthorized,[&]{ (void)service.disable_account(admin,original,admin,30); });
    rejects(ManagementErrorKind::unauthorized,[&]{ (void)service.reset_password(admin,original,other_admin,reset,30); });
    const auto forged=StoredAccount{std::get<Account>(moderator.account.with_access(Role::administrator,Status::active,10)),moderator.revision};
    rejects(ManagementErrorKind::stale,[&]{ (void)service.disable_account(forged,original,changed,30); });
    enroll(c,*key,changed,replacement,"ManagementADC",31,20);
    const auto disabled=service.disable_account(admin,original,changed,30);
    require(disabled.account.status()==Status::disabled && disabled.revision==changed.revision+1 && adc_count(c,disabled)=="0");
    require(PasswordSecurityService(c).verify(disabled.account.id(),replacement)==PasswordVerification::matched);
    rejects(ManagementErrorKind::unauthorized,[&]{ (void)service.change_password(disabled,replacement,reset,40); });
    denied_directory(c,key,"ManagementADC",31);
    const auto reset_account=service.reset_password(admin,original,disabled,reset,40);
    require(reset_account.account.status()==Status::disabled && reset_account.account.role()==Role::registered && reset_account.revision==disabled.revision+1);
    require(PasswordSecurityService(c).verify(reset_account.account.id(),replacement)==PasswordVerification::mismatch);
    require(PasswordSecurityService(c).verify(reset_account.account.id(),reset)==PasswordVerification::matched);
    require(IdentityRepository(c).find_by_account(user.account.id()).has_value());
    require(reset_account.account.times().last_login_at==10);
    const auto metadata=repository.insert({"ManagementMissingCredential",Role::registered,Status::disabled,{10,10,{}}});
    const auto repaired=service.reset_password(admin,original,metadata,reset,40);
    require(PasswordSecurityService(c).verify(repaired.account.id(),reset)==PasswordVerification::matched);
    // Denied DELETE after account/verifier writes must roll the entire operation back.
    const auto rollback=create(c,"ManagementRollback"); enroll(c,*key,rollback,original,"ManagementRollbackADC",32,10);
    const auto rollback_hash=verifier(c,rollback);
    auto limited_options=options;limited_options.user="management_limited";
    { Connection limited(limited_options);
      try { (void)AccountManagementService(limited).reset_password(admin,original,rollback,reset,20); require(false); }
      catch (const dc24h::database::Error& error) { require(!error.outcome_unknown()); }
      require(!limited.in_transaction()); }
    require(repository.find_by_id(rollback.account.id())==rollback && verifier(c,rollback)==rollback_hash && adc_count(c,rollback)=="1");
    { auto tx=c.transaction();
      try { (void)service.change_password(rollback,original,replacement,20); require(false); }
      catch (const dc24h::database::Error& error) { require(error.kind()==dc24h::database::ErrorKind::transaction); }
      tx.rollback(); }
    // Credential-only concurrent changes cannot slip past the pre-lock proof.
    const auto race=create(c,"ManagementProofRace"); Connection writer(options);
    after_kdf=[&]{ writer.execute("UPDATE dc24h_account_credentials SET verifier=CONCAT(LEFT(verifier,LENGTH(verifier)-1),IF(RIGHT(verifier,1)='a','b','a')) WHERE account_id=?",{std::to_string(race.account.id().value())}); };
    rejects(ManagementErrorKind::stale,[&]{ (void)service.change_password(race,original,replacement,20); });
    require(repository.find_by_id(race.account.id())==race && !c.in_transaction());
    // An enrollment that proved an old password must not restore ADC access
    // after account management has changed that password and its revision.
    const auto enrollment_race=create(c,"ManagementEnrollmentRace");
    (void)IdentityRepository(c).bind(enrollment_race.account.id(),*IdentityKey::create("ManagementEnrollmentADC",cid(33)),10);
    after_kdf=[&]{ (void)AccountManagementService(writer).change_password(enrollment_race,original,replacement,20); };
    try { AdcCredentialService(c,*key).enroll(enrollment_race.account.id(),original,adc_original,15); require(false); }
    catch (const AdcCredentialError& error) { require(error.kind()==AdcCredentialErrorKind::conflict); }
    require(adc_count(c,enrollment_race)=="0" && !c.in_transaction());
    require(PasswordSecurityService(c).verify(enrollment_race.account.id(),replacement)==PasswordVerification::matched);
    const auto enrollment_proof=create(c,"ManagementEnrollmentProof");
    (void)IdentityRepository(c).bind(enrollment_proof.account.id(),*IdentityKey::create("ManagementEnrollmentProofADC",cid(34)),10);
    after_kdf=[&]{ writer.execute("UPDATE dc24h_account_credentials SET verifier=CONCAT(LEFT(verifier,LENGTH(verifier)-1),IF(RIGHT(verifier,1)='a','b','a')) WHERE account_id=?",{std::to_string(enrollment_proof.account.id().value())}); };
    try { AdcCredentialService(c,*key).enroll(enrollment_proof.account.id(),original,adc_original,15); require(false); }
    catch (const AdcCredentialError& error) { require(error.kind()==AdcCredentialErrorKind::proof); }
    require(adc_count(c,enrollment_proof)=="0" && repository.find_by_id(enrollment_proof.account.id())==enrollment_proof);
    // A demoted actor cannot complete an already-started administrative request.
    const auto actor=create(c,"ManagementDemotion",Role::administrator);
    after_kdf=[&]{ (void)AccountRepository(writer).update(actor,std::get<Account>(actor.account.with_access(Role::registered,Status::active,20))); };
    rejects(ManagementErrorKind::stale,[&]{ (void)service.disable_account(actor,original,rollback,30); });
    require(repository.find_by_id(rollback.account.id())==rollback && adc_count(c,rollback)=="1");
    // Two requests prove the same actor/target snapshots before row locking.
    std::barrier gate(2); int wins[2]{},stales[2]{};std::exception_ptr errors[2];
    const auto concurrent=create(c,"ManagementConcurrent");
    const auto run=[&](int index) {
        try { Connection db(options); after_kdf=[&]{ gate.arrive_and_wait(); };
          try { (void)AccountManagementService(db).reset_password(admin,original,concurrent,reset,20);wins[index]=1; }
          catch (const ManagementError& error) { require(error.kind()==ManagementErrorKind::stale);stales[index]=1; }
        } catch (...) { errors[index]=std::current_exception();gate.arrive_and_drop(); }
    };
    std::jthread first(run,0),second(run,1);first.join();second.join();
    for (auto error : errors) { if (error) { std::rethrow_exception(error); } }
    require(wins[0]+wins[1]==1 && stales[0]+stales[1]==1);
    // Broken credential schema is refused before any mutation.
    c.execute("ALTER TABLE dc24h_account_adc_credentials DROP FOREIGN KEY dc24h_adc_credentials_account");
    rejects(ManagementErrorKind::schema,[&]{ (void)service.disable_account(admin,original,rollback,30); });
    c.execute("ALTER TABLE dc24h_account_adc_credentials ADD CONSTRAINT dc24h_adc_credentials_account FOREIGN KEY(account_id) REFERENCES dc24h_accounts(id) ON UPDATE RESTRICT ON DELETE RESTRICT");
    require(repository.find_by_id(rollback.account.id())==rollback);
    allocation_failures(options,key);
    // The real ncdc follow-up uses a reset account after explicit reactivation
    // and enrollment of a fresh, separate ADC password.
    const auto fixture=create(c,"ManagementNcdc");enroll(c,*key,fixture,original,"RegisteredADC",7,10);
    const auto fresh=service.reset_password(admin,original,fixture,reset,20);
    denied_directory(c,key,"RegisteredADC",7);
    const auto activated=repository.update(fresh,std::get<Account>(fresh.account.with_access(Role::registered,Status::active,30)));
    enroll(c,*key,activated,reset,"RegisteredADC",7,30,"ADC wire password 42!");
    require(scrubbed && !c.in_transaction());
}
}
int main(int argc,char* argv[]) {
    try {
        if (argc!=4) { throw std::runtime_error("Invalid account management test arguments"); }
        Options options;options.enabled=true;options.socket=argv[1];options.password_file=argv[2];
        integration(options,argv[3]);std::cout << "Account management, proof, atomicity and rollback checks passed\n";
    } catch (const std::exception& error) { fail_after=-1;std::cerr << error.what() << '\n';return 1; }
}
