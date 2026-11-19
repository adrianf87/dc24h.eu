#include "dc24h/api/accounts.hpp"
#include "dc24h/api/http.hpp"
#include "dc24h/accounts/management.hpp"
#include "dc24h/accounts/password_security.hpp"
#include "dc24h/accounts/registration.hpp"
#include <chrono>
#include <iostream>
#include <source_location>
#include <thread>

namespace {
using namespace dc24h;
void require(bool v,std::source_location at=std::source_location::current()) {
    if(!v){throw std::runtime_error("Account WebAdmin assertion failed at line "+std::to_string(at.line()));}
}
void run(const database::Options& options) {
    using namespace accounts;
    database::Connection connection(options);AccountRepository repository(connection);
    const auto now=std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    auto actor=RegistrationService(connection).create_account("WebAdminOperator","WebAdmin operator password!",now);
    actor=repository.update(actor,std::get<Account>(actor.account.with_access(Role::administrator,Status::active,now)));
    auto target=RegistrationService(connection).create_account("WebAdminTarget","WebAdmin target password!",now);
    auto active=AccountManagementService(connection).activate_account(actor,"WebAdmin operator password!",target,now);
    require(active.account.status()==Status::active&&active.revision==target.revision+1);
    require(PasswordSecurityService(connection).verify(active.account.id(),"WebAdmin target password!")==PasswordVerification::matched);
    auto page=repository.list_after(0,1);require(page.size()==1&&page[0].account.id()==actor.account.id());
    require(repository.list_after(actor.account.id().value(),1).at(0).account.id()==target.account.id());
    require(repository.list_after(target.account.id().value()).empty());
    auto handler=api::account_handler(options);
    std::string grant;
    auto call=[&](std::string method,std::string path,std::string body={},bool use_grant=true) {
        api::Request request;
        request.method=std::move(method);request.target=std::move(path);request.body=std::move(body);
        if(use_grant){request.admin_grant=grant;}
        return handler(request);
    };
    auto take_grant=[&](const std::string& http) {
        const auto marker=std::string("\"grant\":\"");
        const auto at=http.find(marker);require(at!=std::string::npos&&http.size()>=at+marker.size()+64);
        grant=http.substr(at+marker.size(),64);require(grant.find_first_not_of("0123456789abcdef")==std::string::npos);
    };
    const auto actor_id=std::to_string(actor.account.id().value());
    const auto proved=call("POST","/api/v1/admin/access","actor_id="+actor_id+"&proof=WebAdmin+operator+password!",false);
    require(proved.starts_with("HTTP/1.1 200")&&proved.find("verifier")==std::string::npos);
    take_grant(proved);
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    const auto listed=call("GET","/api/v1/accounts?after=0");
    require(listed.starts_with("HTTP/1.1 200")&&listed.find("WebAdminTarget")!=std::string::npos&&listed.find("verifier")==std::string::npos);
    const auto management=call("GET","/api/v1/management");
    require(management.starts_with("HTTP/1.1 200"));
    require(management.find("\"schema\":1,\"maintenance\":true,\"restart_required\":true")!=std::string::npos);
    require(management.find("\"key\":\"accounts\",\"permission\":\"accounts.manage\",\"allowed\":true")!=std::string::npos);
    require(management.find("\"key\":\"roles\",\"permission\":\"roles.manage\",\"allowed\":true")!=std::string::npos);
    require(management.find("\"key\":\"bans\",\"permission\":\"users.ban+blocks.manage\",\"allowed\":true")!=std::string::npos);
    require(management.find(grant)==std::string::npos&&management.find("WebAdmin")==std::string::npos);
    require(call("GET","/api/v1/management","",false).starts_with("HTTP/1.1 401"));
    require(call("GET","/api/v1/accounts?after=-1").starts_with("HTTP/1.1 400"));
    require(call("GET","/api/v1/accounts/999999999").starts_with("HTTP/1.1 404"));
    require(call("POST","/api/v1/accounts","name=X&name=Y").starts_with("HTTP/1.1 400"));
    require(call("POST","/api/v1/accounts","x=y").starts_with("HTTP/1.1 429"));
    handler=api::account_handler(options);grant.clear();
    const auto proved_again=call("POST","/api/v1/admin/access","actor_id="+actor_id+"&proof=WebAdmin+operator+password!",false);
    require(proved_again.starts_with("HTTP/1.1 200"));take_grant(proved_again);
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    require(call("POST","/api/v1/management","x=y").starts_with("HTTP/1.1 405"));
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    require(call("POST","/api/v1/accounts","actor_id="+actor_id+"&proof=Incorrect+administrator+password!&name=NoAccount&password=WebAdmin+test+password!").starts_with("HTTP/1.1 403"));
    require(repository.list_after().size()==2);
    actor=repository.update(actor,std::get<Account>(actor.account.with_access(Role::operator_,Status::active,now)));
    const auto limited=call("GET","/api/v1/management");
    require(limited.starts_with("HTTP/1.1 200"));
    require(limited.find("\"key\":\"accounts\",\"permission\":\"accounts.manage\",\"allowed\":false")!=std::string::npos);
    require(limited.find("\"key\":\"roles\",\"permission\":\"roles.manage\",\"allowed\":false")!=std::string::npos);
    require(limited.find("\"key\":\"bans\",\"permission\":\"users.ban+blocks.manage\",\"allowed\":true")!=std::string::npos);
    require(limited.find(grant)==std::string::npos&&limited.find("password")==std::string::npos);
    actor=repository.update(actor,std::get<Account>(actor.account.with_access(Role::administrator,Status::active,now)));
    const auto stale_actor=actor;
    actor=repository.update(actor,std::get<Account>(actor.account.with_access(Role::administrator,Status::active,now)));
    bool stale=false;
    try{(void)AccountManagementService(connection).create_account(stale_actor,"WebAdmin operator password!","StaleActorCreate","New account password!",now);}
    catch(const ManagementError& e){stale=e.kind()==ManagementErrorKind::stale;}
    require(stale&&repository.list_after().size()==2);
    // Remaining real HTTP operations run in the Python disposable-DB harness.
    std::cout<<"actor="<<actor.account.id().value()<<" target="<<target.account.id().value()<<'\n';
}
}
int main(int argc,char* argv[]) {
    try {if(argc!=3){throw std::runtime_error("Invalid WebAdmin test arguments");}
        database::Options options;options.enabled=true;options.socket=argv[1];options.password_file=argv[2];run(options);
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
