#include "dc24h/bans/repository.hpp"
#include "dc24h/accounts/registration.hpp"
#include "dc24h/accounts/password_security.hpp"
#include "dc24h/accounts/identity_mapping.hpp"
#include "dc24h/admin/core.hpp"
#include <openssl/evp.h>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <new>
#include <limits>
#include <source_location>

namespace {bool fail_allocation=false;thread_local std::function<void()> after_kdf;}
void* operator new(std::size_t n){if(fail_allocation){throw std::bad_alloc();}if(auto* p=std::malloc(n?n:1)){return p;}throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}void operator delete(void* p) noexcept{std::free(p);}void operator delete[](void* p) noexcept{std::free(p);}void operator delete(void* p,std::size_t) noexcept{std::free(p);}void operator delete[](void* p,std::size_t) noexcept{std::free(p);}
extern "C" int __real_EVP_PBE_scrypt(const char*,std::size_t,const unsigned char*,std::size_t,std::uint64_t,std::uint64_t,std::uint64_t,std::uint64_t,unsigned char*,std::size_t);
extern "C" int __wrap_EVP_PBE_scrypt(const char* p,std::size_t n,const unsigned char* s,std::size_t z,std::uint64_t a,std::uint64_t b,std::uint64_t c,std::uint64_t d,unsigned char* o,std::size_t e){
    const int result=__real_EVP_PBE_scrypt(p,n,s,z,a,b,c,d,o,e);if(result&&after_kdf){auto callback=std::move(after_kdf);after_kdf={};callback();}return result;
}
namespace {
using namespace dc24h;using namespace bans;constexpr std::string_view password="Ban Core account password 42!";
void require(bool value,std::source_location line=std::source_location::current()){if(!value){throw std::runtime_error("Ban assertion at line "+std::to_string(line.line()));}}
template<class F> void rejects(ErrorKind expected,F&& operation){try{operation();}catch(const Error& e){require(e.kind()==expected);require(std::string_view(e.what()).find(password)==std::string_view::npos);return;}require(false);}
void unit(){
    require(canonical_subject(Kind::ip,"::ffff:192.0.2.1")=="192.0.2.1");require(canonical_subject(Kind::ip,"2001:DB8:0:0::1")=="2001:db8::1");
    for(const auto value:{"host.invalid","127.0.0.1/32","127.000.0.1","[::1]","fe80::1%eth0",""}){require(!canonical_subject(Kind::ip,value));}
    require(!canonical_subject(Kind::account,"01")&&!canonical_subject(Kind::account,"9223372036854775808"));
    require(!canonical_subject(Kind::cid,std::string(39,'B')));const auto cid=std::string(39,'A');require(canonical_subject(Kind::cid,cid)==cid);
    Directory directory({{Kind::account,"7",20},{Kind::account,"7",30},{Kind::cid,cid,{}},{Kind::ip,"192.0.2.1",20}});
    fail_allocation=true;const auto account=directory.account(7,21),ip=directory.ip("::ffff:192.0.2.1",19),public_cid=directory.cid(cid,100);fail_allocation=false;
    require(account&&account->expires_at==30&&ip&&public_cid&&!public_cid->expires_at);
    require(!directory.account(7,30)&&!directory.ip("192.0.2.1",20));
    rejects(ErrorKind::invalid_record,[&]{Directory invalid({{Kind::ip,"::ffff:192.0.2.1",{}}});});
    Record timed{1,Kind::ip,"192.0.2.1","Timed",10,20,1,{},{},1};
    require(state_at(timed,19)==State::temporary&&seconds_left(timed,19)==1);
    require(state_at(timed,20)==State::expired&&!seconds_left(timed,20)&&state_at(timed,21)==State::expired);
    timed.revoked_at=15;timed.revoked_by=1;require(state_at(timed,21)==State::revoked&&!seconds_left(timed,19));
    timed.revoked_at.reset();timed.revoked_by.reset();timed.expires_at.reset();require(state_at(timed,100)==State::permanent&&!seconds_left(timed,100));
    require(expires_after(10,1)==11&&expires_after(0,max_duration_seconds)==static_cast<Timestamp>(max_duration_seconds));
    require(expires_after(accounts::max_timestamp-1,1)==accounts::max_timestamp);
    for(const auto duration:{std::uint64_t{0},max_duration_seconds+1,std::numeric_limits<std::uint64_t>::max()}){
        rejects(ErrorKind::invalid_input,[&]{(void)expires_after(10,duration);});
    }
    rejects(ErrorKind::invalid_input,[&]{(void)expires_after(-1,1);});
    rejects(ErrorKind::invalid_input,[&]{(void)expires_after(accounts::max_timestamp,1);});
    rejects(ErrorKind::invalid_input,[&]{(void)expires_after(accounts::max_timestamp-1,2);});
    for(auto f:{Filter::all,Filter::active,Filter::permanent,Filter::temporary,Filter::expired,Filter::revoked})require(parse_filter(filter_key(f))==f);
    require(!parse_filter("TEMPORARY")&&!parse_filter(""));
    Directory overlap({{Kind::ip,"192.0.2.1",20},{Kind::ip,"192.0.2.1",{}},{Kind::cid,cid,20},{Kind::cid,cid,30},
        {Kind::account,"8",20},{Kind::account,"8",30}});
    require(overlap.ip("192.0.2.1",100).has_value()&&!overlap.ip("192.0.2.1",100)->expires_at);
    require(overlap.cid(cid,29).has_value()&&!overlap.cid(cid,30)&&overlap.account(8,29).has_value()&&!overlap.account(8,30));
    Record bad{1,Kind::ip,"127.0.0.1","bad\nreason",1,{},1,{},{},1};rejects(ErrorKind::invalid_record,[&]{validate(bad);});
}
accounts::StoredAccount create(database::Connection& db,std::string name,accounts::Role role){auto r=accounts::RegistrationService(db).create_account(name,password,10);return accounts::AccountRepository(db).update(r,std::get<accounts::Account>(r.account.with_access(role,accounts::Status::active,10)));}
void integration(const database::Options& options){
    using accounts::Role;database::Connection db(options),writer(options);accounts::AccountRepository accounts(db);Service service(db);Repository repository(db);
    const auto owner=create(db,"BanOwner",Role::owner),op=create(db,"BanOperator",Role::operator_),mod=create(db,"BanModerator",Role::moderator),user=create(db,"BanRegistered",Role::registered);
    rejects(ErrorKind::forbidden,[&]{(void)service.create(mod,password,Kind::ip,"192.0.2.1","Reason",{},15);});
    rejects(ErrorKind::forbidden,[&]{(void)service.create(op,password,Kind::account,std::to_string(owner.account.id().value()),"Reason",{},15);});
    rejects(ErrorKind::forbidden,[&]{(void)service.create(op,"Wrong ban password!",Kind::ip,"192.0.2.1","Reason",{},15);});
    auto ban=service.create(op,password,Kind::account,std::to_string(user.account.id().value()),"Private ban reason",20,15);
    auto directory=repository.load(15);require(directory->account(user.account.id().value(),15).has_value()&&!directory->account(user.account.id().value(),20));
    const auto revoked=service.revoke(op,password,ban,16);require(revoked.revision==2&&revoked.revoked_at==16&&!repository.load(16)->account(user.account.id().value(),16));
    rejects(ErrorKind::stale,[&]{(void)service.revoke(op,password,ban,17);});
    const auto protected_ban=service.create(owner,password,Kind::ip,"::ffff:192.0.2.1","Owner reason",{},15);
    require(protected_ban.subject=="192.0.2.1");rejects(ErrorKind::forbidden,[&]{(void)service.revoke(op,password,protected_ban,16);});
    const auto cid=std::string(39,'A');(void)accounts::IdentityRepository(db).bind(owner.account.id(),*accounts::IdentityKey::create("BanOwnerADC",cid),10);
    rejects(ErrorKind::forbidden,[&]{(void)service.create(op,password,Kind::cid,cid,"Reason",{},15);});
    rejects(ErrorKind::forbidden,[&]{(void)service.create(owner,password,Kind::cid,cid,"Reason",{},15);});
    // Proof-only changes without metadata revisions are detected under the lock.
    const auto original=*db.scalar("SELECT verifier FROM dc24h_account_credentials WHERE account_id=?",{std::to_string(op.account.id().value())});
    after_kdf=[&]{writer.execute("UPDATE dc24h_account_credentials SET verifier=CONCAT(LEFT(verifier,LENGTH(verifier)-1),IF(RIGHT(verifier,1)='a','b','a')) WHERE account_id=?",{std::to_string(op.account.id().value())});};
    rejects(ErrorKind::stale,[&]{(void)service.create(op,password,Kind::ip,"192.0.2.2","Race",{},15);});
    writer.execute("UPDATE dc24h_account_credentials SET verifier=? WHERE account_id=?",{original,std::to_string(op.account.id().value())});
    require(repository.list().size()==2&&accounts::PasswordSecurityService(db).verify(user.account.id(),password)==accounts::PasswordVerification::matched);
    // Runtime load fails closed on malformed stored keys rather than ignoring them.
    db.execute("INSERT INTO dc24h_bans(kind,subject,reason,created_at,created_by) VALUES ('ip','not-an-ip','Reason',10,?)",{std::to_string(op.account.id().value())});
    rejects(ErrorKind::invalid_record,[&]{(void)repository.load(15);});db.execute("DELETE FROM dc24h_bans WHERE subject='not-an-ip'");
    // Keep the fixture directory empty for the parent's later hub/HTTP checks.
    (void)service.revoke(owner,password,protected_ban,16);
    // Exercise multiple bounded SQL pages and the write/load capacity guards.
    const std::string digits="(SELECT 0 AS n UNION ALL SELECT 1 UNION ALL SELECT 2 UNION ALL SELECT 3 UNION ALL SELECT 4 UNION ALL SELECT 5 UNION ALL SELECT 6 UNION ALL SELECT 7 UNION ALL SELECT 8 UNION ALL SELECT 9)";
    const auto bulk=db.execute_prepared("INSERT INTO dc24h_bans(kind,subject,reason,created_at,created_by) SELECT 'ip','192.0.2.100','Bulk policy',10,? FROM "+digits+" a CROSS JOIN "+digits+" b CROSS JOIN "+digits+" c CROSS JOIN "+digits+" d LIMIT 4096",{op.account.id().value()});
    require(bulk.affected_rows==4096&&repository.load(15)->ip("192.0.2.100",15).has_value());
    const auto page=repository.list();require(page.size()==16&&repository.list(page.back().id).size()==16);
    rejects(ErrorKind::capacity,[&]{(void)service.create(op,password,Kind::ip,"192.0.2.101","Capacity",{},15);});
    db.execute("INSERT INTO dc24h_bans(kind,subject,reason,created_at,created_by) VALUES ('ip','192.0.2.100','Bulk policy',10,?)",{std::to_string(op.account.id().value())});
    rejects(ErrorKind::capacity,[&]{(void)repository.load(15);});
    db.execute("DELETE FROM dc24h_bans WHERE reason='Bulk policy'");
    // Filtering happens before LIMIT, and expiry leaves stored history untouched.
    const auto skipped=db.execute_prepared("INSERT INTO dc24h_bans(kind,subject,reason,created_at,expires_at,created_by) SELECT 'ip','192.0.2.101','Expired paging',10,20,? FROM "+digits+" a CROSS JOIN "+digits+" b LIMIT 17",{op.account.id().value()});
    require(skipped.affected_rows==17);
    admin::Engine engine(options,admin::Origin::maintenance_api);admin::Command c;c.action=admin::Action::ban_create;c.actor={op.account.id().value(),0};c.proof=password;
    c.time=30;c.ban_kind=Kind::ip;c.subject="192.0.2.102";c.reason="Relative fixture";c.duration_seconds=5;
    const auto relative=std::get<Record>(engine.execute(c));require(relative.created_at==30&&relative.expires_at==35);
    const auto temporary=repository.list(0,Filter::temporary,34);require(temporary.size()==1&&temporary[0]==relative);
    require(repository.list(0,Filter::active,34)==temporary&&repository.list(0,Filter::permanent,34).empty());
    require(repository.list(0,Filter::temporary,35).empty()&&repository.list(0,Filter::active,35).empty());
    const auto expired=repository.list(0,Filter::expired,35);require(expired.size()==16);
    const auto rest=repository.list(expired.back().id,Filter::expired,35);require(rest.size()==2&&rest.back()==relative);
    require(repository.find(relative.id)==relative&&repository.list(0,Filter::revoked,35).size()==2);
    rejects(ErrorKind::invalid_input,[&]{(void)repository.list(0,static_cast<Filter>(999),35);});
    rejects(ErrorKind::invalid_input,[&]{(void)repository.list(0,Filter::all,-1);});
    // Expired records do not consume active capacity; another ban can be issued.
    auto next=service.create(op,password,Kind::ip,"192.0.2.102","Replacement",40,35);
    require(repository.load(35)->ip("192.0.2.102",35).has_value());(void)service.revoke(op,password,next,36);
    db.execute("DELETE FROM dc24h_bans WHERE reason IN ('Expired paging','Relative fixture')");
    require(!db.in_transaction());
}
void lost_commit(const database::Options& options,std::uint64_t actor){
    admin::Engine engine(options,admin::Origin::maintenance_api);admin::Command c;c.action=admin::Action::ban_create;c.actor={actor,0};c.proof=password;c.ban_kind=Kind::ip;c.subject="192.0.2.99";c.reason="Unknown fixture";c.time=30;
    bool unknown=false;try{(void)engine.execute(c);}catch(const admin::Error& e){unknown=e.kind()==admin::ErrorKind::outcome_unknown;}
    require(unknown&&engine.statistics().outcome_unknown==1&&engine.statistics().succeeded==0);
}

}
int main(int argc,char* argv[]){try{unit();if(argc==5){database::Options o;o.enabled=true;o.socket=argv[1];o.password_file=argv[2];o.timeout=2;lost_commit(o,std::stoull(argv[4]));}
else if(argc==3){database::Options o;o.enabled=true;o.socket=argv[1];o.password_file=argv[2];integration(o);}
std::cout<<"Ban model, storage, authority and snapshot checks passed\n";}
catch(const std::exception& e){fail_allocation=false;std::cerr<<e.what()<<'\n';return 1;}}
