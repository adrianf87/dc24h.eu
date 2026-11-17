#include "dc24h/admin/core.hpp"
#include "dc24h/accounts/registration.hpp"
#include <iostream>
#include <source_location>
#include <type_traits>

namespace {
using namespace dc24h;
using namespace admin;
constexpr std::string_view proof="Admin Core account password 42!";
constexpr std::string_view replacement="Admin Core replacement password 42!";
void require(bool value,std::source_location line=std::source_location::current()) {
    if(!value){throw std::runtime_error("Admin Core assertion at line "+std::to_string(line.line()));}
}
template<class F> void rejects(ErrorKind expected,F&& operation) {
    try { operation(); } catch(const Error& e) {
        require(e.kind()==expected);require(std::string_view(e.what()).find(proof)==std::string_view::npos);return;
    }
    require(false);
}
accounts::StoredAccount account(Result result) { return std::get<accounts::StoredAccount>(std::move(result)); }
AccountRef ref(const accounts::StoredAccount& a) { return {a.account.id().value(),a.revision}; }
void unit() {
    static_assert(std::is_nothrow_constructible_v<Error,ErrorKind>);
    static_assert(noexcept(Error(ErrorKind::outcome_unknown).what()));
    database::Options options;
    Engine api(options,Origin::maintenance_api),local(options,Origin::owner_cli);
    for(auto a:action_catalog){
        require(parse_action(action_key(a))==a);
        require(available(a,Origin::maintenance_api)!=available(a,Origin::owner_cli));
        require(!available(a,static_cast<Origin>(99)));
    }
    require(!parse_action("ownership.force")&&!parse_action("ACCOUNTS.CREATE"));
    require(action_key(static_cast<Action>(99)).empty());
    Command c;c.action=Action::bootstrap_owner;c.name="Owner";c.password=proof;
    rejects(ErrorKind::forbidden,[&]{(void)api.execute(c);});
    c.action=Action::create;c.proof=proof;c.actor={1,0};
    rejects(ErrorKind::forbidden,[&]{(void)local.execute(c);});
    for(int i=0;i<7;++i){auto invalid=c;
        if(i==0){invalid.actor.id=0;}if(i==1){invalid.target={1,1};}if(i==2){invalid.time=-1;}
        if(i==3){invalid.proof={};}if(i==4){invalid.successor_proof=proof;}
        if(i==5){invalid.role=accounts::Role::owner;}if(i==6){invalid.name={};}
        rejects(ErrorKind::invalid_input,[&]{(void)api.execute(invalid);});
    }
    require(api.statistics().attempts==8&&api.statistics().refused==8&&api.statistics().succeeded==0);
    require(local.statistics().attempts==1&&local.statistics().refused==1);
    rejects(ErrorKind::invalid_input,[&]{(void)api.account(0);});
    require(api.statistics().attempts==8);
    // Invalid duration shapes must be refused before opening a SQL connection.
    c={};c.action=Action::ban_create;c.actor={1,0};c.proof=proof;c.ban_kind=bans::Kind::ip;c.subject="192.0.2.1";c.reason="Timed";c.time=10;
    c.duration_seconds=0;rejects(ErrorKind::invalid_input,[&]{(void)api.execute(c);});
    c.duration_seconds=bans::max_duration_seconds+1;rejects(ErrorKind::invalid_input,[&]{(void)api.execute(c);});
    c.duration_seconds=1;c.expires_at=20;rejects(ErrorKind::invalid_input,[&]{(void)api.execute(c);});
    c.expires_at.reset();c.time=accounts::max_timestamp;rejects(ErrorKind::invalid_input,[&]{(void)api.execute(c);});
    c.time=10;c.action=Action::ban_revoke;c.target={1,1};c.subject={};c.reason={};c.ban_kind=bans::Kind::account;
    rejects(ErrorKind::invalid_input,[&]{(void)api.execute(c);});
    c.action=Action::disable;rejects(ErrorKind::invalid_input,[&]{(void)api.execute(c);});
}
void integration(const database::Options& options) {
    using namespace accounts;
    Engine local(options,Origin::owner_cli),api(options,Origin::maintenance_api);
    Command c;c.action=Action::bootstrap_owner;c.name="CoreOwner";c.password=proof;c.time=10;
    const auto owner=account(local.execute(c));
    database::Connection db(options);AccountRepository repository(db);
    auto successor=RegistrationService(db).create_account("CoreSuccessor",proof,10);
    successor=repository.update(successor,std::get<Account>(successor.account.with_access(Role::administrator,Status::active,10)));
    c={};c.action=Action::create;c.actor=ref(owner);c.proof=proof;c.password=proof;c.name="CoreManaged";c.time=20;
    auto managed=account(api.execute(c));require(managed.account.status()==Status::disabled);
    require(api.list().size()==3&&api.list(0,true).size()==2&&api.account(managed.account.id().value())==managed);
    require(api.statistics().attempts==1);
    c={};c.action=Action::activate;c.actor=ref(owner);c.target=ref(managed);c.proof=proof;c.time=20;
    managed=account(api.execute(c));require(managed.account.status()==Status::active);
    c.action=Action::password;c.actor={};c.target=ref(managed);c.password=replacement;
    managed=account(api.execute(c));
    c.action=Action::role;c.actor=ref(owner);c.target=ref(managed);c.password={};c.role=Role::operator_;
    managed=account(api.execute(c));require(managed.account.role()==Role::operator_&&managed.account.status()==Status::disabled);
    c.action=Action::activate;c.target=ref(managed);c.role=Role::registered;
    managed=account(api.execute(c));
    c.action=Action::reset;c.target=ref(managed);c.password=proof;
    managed=account(api.execute(c));require(managed.account.status()==Status::disabled);
    c.action=Action::disable;c.target=ref(managed);c.password={};
    managed=account(api.execute(c));require(api.statistics().succeeded==7);
    rejects(ErrorKind::stale,[&]{(void)api.execute(c);});
    c.target={99999999,1};rejects(ErrorKind::not_found,[&]{(void)api.execute(c);});
    c.target=ref(managed);c.action=Action::role;c.role=Role::owner;
    rejects(ErrorKind::forbidden,[&]{(void)api.execute(c);});
    c.action=Action::disable;c.role=Role::registered;c.proof="Incorrect Core password!";
    rejects(ErrorKind::forbidden,[&]{(void)api.execute(c);});
    c.proof=proof;++c.actor.revision;rejects(ErrorKind::stale,[&]{(void)api.execute(c);});
    c={};c.action=Action::transfer_owner;c.actor=ref(owner);c.target=ref(successor);c.proof=proof;c.successor_proof=proof;c.time=30;
    rejects(ErrorKind::forbidden,[&]{(void)api.execute(c);});
    const auto transfer=std::get<OwnershipTransfer>(local.execute(c));
    require(transfer.new_owner.account.role()==Role::owner&&transfer.former_owner.account.status()==Status::disabled);
    require(local.statistics().succeeded==2&&api.statistics().refused==6);
    auto limited=options;limited.user="management_limited";Engine restricted(limited,Origin::maintenance_api);
    c={};c.action=Action::role;c.actor=ref(transfer.new_owner);c.target=ref(transfer.former_owner);c.proof=proof;c.role=Role::operator_;c.time=40;
    rejects(ErrorKind::unavailable,[&]{(void)restricted.execute(c);});
    require(restricted.statistics().unavailable==1&&restricted.statistics().succeeded==0);
    require(repository.find_by_id(transfer.former_owner.account.id())==transfer.former_owner);
    auto offline=options;offline.socket="/tmp/dc24h-admin-core-missing/database.sock";Engine unavailable(offline,Origin::maintenance_api);
    rejects(ErrorKind::unavailable,[&]{(void)unavailable.execute(c);});
    require(unavailable.statistics().unavailable==1&&unavailable.statistics().outcome_unknown==0);
}
void lost_commit(const database::Options& options,std::uint64_t actor) {
    Engine engine(options,Origin::maintenance_api);
    Command c;c.action=Action::create;c.actor={actor,0};c.proof=proof;c.password=replacement;c.name="CoreUnknownWrite";c.time=40;
    rejects(ErrorKind::outcome_unknown,[&]{(void)engine.execute(c);});
    const auto stats=engine.statistics();
    require(stats.attempts==1&&stats.outcome_unknown==1&&stats.succeeded==0&&stats.unavailable==0&&stats.refused==0);
}

}
int main(int argc,char* argv[]) {
    try {unit();if(argc==5){database::Options options;options.enabled=true;options.socket=argv[1];options.password_file=argv[2];options.timeout=2;
        require(std::string_view(argv[3])=="commit_ack");lost_commit(options,std::stoull(argv[4]));}
        else if(argc==3){database::Options options;options.enabled=true;options.socket=argv[1];options.password_file=argv[2];integration(options);}
        std::cout<<"Admin Core dispatch, entry-point isolation and result checks passed\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
