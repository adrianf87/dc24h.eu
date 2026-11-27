#include "dc24h/admin/access.hpp"
#include "dc24h/api/accounts.hpp"
#include "dc24h/api/http.hpp"
#include "dc24h/accounts/management.hpp"
#include "dc24h/accounts/password_security.hpp"
#include "dc24h/accounts/registration.hpp"
#include "dc24h/configuration/configuration.hpp"
#include <unistd.h>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <source_location>
#include <thread>
#include <vector>

namespace {
using namespace dc24h;
void require(bool v,std::source_location at=std::source_location::current()) {
    if(!v){throw std::runtime_error("Account WebAdmin assertion failed at line "+std::to_string(at.line()));}
}
void run(const database::Options& options) {
    using namespace accounts;
    const auto path_template=(std::filesystem::temp_directory_path()/"dc24h-account-review-XXXXXX").string();
    std::vector<char> path_bytes(path_template.begin(),path_template.end());path_bytes.push_back('\0');
    require(::mkdtemp(path_bytes.data())!=nullptr);
    struct Temp { std::string path; ~Temp(){std::error_code ignored;std::filesystem::remove_all(std::filesystem::path(path).parent_path(),ignored);} } ini{std::string(path_bytes.data())+"/hub.ini"};
    auto write_ini=[&](std::string_view content){std::ofstream out(ini.path,std::ios::trunc);out<<content;require(out.good());};
    write_ini("[network]\nmax_connections=128\nidle_timeout=120\n[logging]\nlevel=info\n[general]\nlanguage=us\n");
    const auto review=std::make_shared<configuration::Review>(configuration::load_file(ini.path));
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
    auto handler=api::account_handler(options,review);
    std::string grant, session, csrf;
    auto call=[&](std::string method,std::string path,std::string body={},bool use_grant=true,bool bind=true) {
        api::Request request;
        request.method=std::move(method);request.target=std::move(path);request.body=std::move(body);
        if(use_grant){
            request.admin_grant=grant;
            if(bind&&request.target!="/api/v1/admin/access"){
                request.admin_session=session;request.admin_csrf=csrf;
            }
        }
        return handler(request);
    };
    auto take_field=[&](const std::string& http,const char* name,std::size_t size,std::string& out) {
        const auto marker=std::string("\"")+name+"\":\"";
        const auto at=http.find(marker);require(at!=std::string::npos&&http.size()>=at+marker.size()+size);
        out=http.substr(at+marker.size(),size);require(out.find_first_not_of("0123456789abcdef")==std::string::npos);
    };
    auto take_secrets=[&](const std::string& http) {
        take_field(http,"grant",64,grant);take_field(http,"session",32,session);take_field(http,"csrf",64,csrf);
        require(grant!=csrf);
    };
    // The public-serving hub has exactly TWO private credential/status routes.
    // Every write-capable maintenance route must remain unreachable even with
    // a valid local API bearer and a real administrative account.
    auto serving=api::public_account_access_handler(options);
    auto public_call=[&](std::string method,std::string target,std::string body={}) {
        api::Request request;request.method=std::move(method);request.target=std::move(target);
        request.body=std::move(body);return serving(request);
    };
    for(const auto& target:{"/api/v1/accounts","/api/v1/accounts/7/role", "/api/v1/bans",
            "/api/v1/audit", "/api/v1/management/hub/restart", "/api/v1/management/configuration/write"}) {
        require(public_call("POST",target,"actor_id=7&proof=invalid").starts_with("HTTP/1.1 404"));
    }
    require(public_call("GET","/api/v1/accounts").starts_with("HTTP/1.1 404"));
    require(public_call("GET","/api/v1/management/configuration").starts_with("HTTP/1.1 404"));
    require(public_call("GET","/api/v1/management").starts_with("HTTP/1.1 401"));
    const auto readonly=public_call("POST","/api/v1/admin/access",
        "account_name=WebAdminOperator&proof=WebAdmin+operator+password!");
    require(readonly.starts_with("HTTP/1.1 200"));
    require(readonly.find("\"id\":\""+std::to_string(actor.account.id().value())+"\"")!=std::string::npos);
    require(readonly.find("verifier")==std::string::npos);
    require(public_call("POST","/api/v1/management","invalid").starts_with("HTTP/1.1 404"));
    const auto actor_id=std::to_string(actor.account.id().value());
    const auto proved=call("POST","/api/v1/admin/access","actor_id="+actor_id+"&proof=WebAdmin+operator+password!",false);
    require(proved.starts_with("HTTP/1.1 200")&&proved.find("verifier")==std::string::npos);
    take_secrets(proved);
    const auto config_view=call("GET","/api/v1/management/configuration");
    require(config_view.starts_with("HTTP/1.1 200")&&config_view.find("\"hub_active_available\":false")!=std::string::npos);
    require(config_view.find("\"saved\":{")!=std::string::npos&&config_view.find("\"active\":{")!=std::string::npos);
    require(config_view.find(ini.path)==std::string::npos&&config_view.find(grant)==std::string::npos);
    std::string revision;take_field(config_view,"revision",64,revision);
    require(call("GET","/api/v1/management/configuration","",true,false).find("csrf_required")!=std::string::npos);
    require(call("GET","/api/v1/management/configuration","",false).starts_with("HTTP/1.1 401"));
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    const auto valid_config=call("POST","/api/v1/management/configuration","revision="+revision+"&network_max_connections=256");
    require(valid_config.starts_with("HTTP/1.1 200")&&valid_config.find("\"valid\":true")!=std::string::npos);
    require(valid_config.find("\"written\":false")!=std::string::npos);
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    require(call("POST","/api/v1/management/configuration","revision="+revision+"&network_max_connections=0").find("\"valid\":false")!=std::string::npos);
    write_ini("[network]\nmax_connections=256\nidle_timeout=120\n");
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    require(call("POST","/api/v1/management/configuration","revision="+revision+"&network_max_connections=512").starts_with("HTTP/1.1 409"));
    const auto changed_view=call("GET","/api/v1/management/configuration");
    require(changed_view.find("\"matches_active\":false")!=std::string::npos);
    std::string fresh_revision;take_field(changed_view,"revision",64,fresh_revision);
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    const auto saved_config=call("POST","/api/v1/management/configuration/write",
        "revision="+fresh_revision+"&network_max_connections=512");
    require(saved_config.starts_with("HTTP/1.1 200")&&saved_config.find("\"written\":true")!=std::string::npos);
    std::string saved_revision;take_field(saved_config,"revision",64,saved_revision);
    require(saved_revision!=fresh_revision);
    { std::ifstream input(ini.path);const std::string data(std::istreambuf_iterator<char>{input},std::istreambuf_iterator<char>{});
      require(data.find("max_connections = 512")!=std::string::npos); }
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    const auto restored_config=call("POST","/api/v1/management/configuration/restore","revision="+saved_revision);
    require(restored_config.starts_with("HTTP/1.1 200")&&restored_config.find("\"written\":true")!=std::string::npos);
    { std::ifstream input(ini.path);const std::string data(std::istreambuf_iterator<char>{input},std::istreambuf_iterator<char>{});
      require(data.find("max_connections=256")!=std::string::npos); }
    const auto undo_path=ini.path+".dc24h.undo";
    std::ifstream undo_stream(undo_path,std::ios::binary);
    const std::string undo_bytes(std::istreambuf_iterator<char>{undo_stream},std::istreambuf_iterator<char>{});
    require(undo_bytes.find("max_connections = 512")!=std::string::npos);
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    const auto duplicate_restore=call("POST","/api/v1/management/configuration/restore","revision="+fresh_revision);
    require(duplicate_restore.starts_with("HTTP/1.1 409"));
    { std::ifstream input(undo_path,std::ios::binary);
      const std::string still_undo(std::istreambuf_iterator<char>{input},std::istreambuf_iterator<char>{});
      require(still_undo==undo_bytes); }
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    const auto config_history=call("POST","/api/v1/audit","actor_id="+actor_id+
        "&proof=WebAdmin+operator+password!&after=0");
    require(config_history.starts_with("HTTP/1.1 200")&&
        config_history.find("configuration.write")!=std::string::npos&&
        config_history.find("configuration.restore")!=std::string::npos&&
        config_history.find("\"outcome\":\"succeeded\"")!=std::string::npos&&
        config_history.find("\"outcome\":\"failed\"")!=std::string::npos&&
        config_history.find(ini.path)==std::string::npos);
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    const auto listed=call("GET","/api/v1/accounts?after=0");
    require(listed.starts_with("HTTP/1.1 200")&&listed.find("WebAdminTarget")!=std::string::npos&&listed.find("verifier")==std::string::npos);
    const auto unbound_read=call("GET","/api/v1/accounts?after=0","",true,false);
    require(unbound_read.find("csrf_required")!=std::string::npos&&unbound_read.find("WebAdminTarget")==std::string::npos);
    const auto management=call("GET","/api/v1/management");
    require(management.starts_with("HTTP/1.1 200"));
    require(management.find("\"schema\":2,\"maintenance\":true,\"restart_required\":true")!=std::string::npos);
    require(management.find("\"key\":\"accounts\",\"permission\":\"accounts.manage\",\"allowed\":true,\"implemented\":true")!=std::string::npos);
    require(management.find("\"key\":\"roles\",\"permission\":\"roles.manage\",\"allowed\":true,\"implemented\":true")!=std::string::npos);
    require(management.find("\"key\":\"bans\",\"permission\":\"users.ban+blocks.manage\",\"allowed\":true,\"implemented\":true")!=std::string::npos);
    require(management.find("\"key\":\"configuration\",\"permission\":\"configuration.manage\",\"allowed\":true,\"implemented\":false")!=std::string::npos);
    require(management.find("\"key\":\"hub\",\"permission\":\"hub.manage\",\"allowed\":false,\"implemented\":true")!=std::string::npos);
    require(management.find(grant)==std::string::npos&&management.find("WebAdmin")==std::string::npos);
    require(call("GET","/api/v1/management","",false).starts_with("HTTP/1.1 401"));
    require(call("GET","/api/v1/accounts?after=-1").starts_with("HTTP/1.1 400"));
    require(call("GET","/api/v1/accounts/999999999").starts_with("HTTP/1.1 404"));
    require(call("POST","/api/v1/accounts","name=X&name=Y").starts_with("HTTP/1.1 400"));
    require(call("POST","/api/v1/accounts","x=y").starts_with("HTTP/1.1 429"));
    handler=api::account_handler(options,review);grant.clear();session.clear();csrf.clear();
    const auto proved_again=call("POST","/api/v1/admin/access","actor_id="+actor_id+"&proof=WebAdmin+operator+password!",false);
    require(proved_again.starts_with("HTTP/1.1 200"));take_secrets(proved_again);
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    require(call("POST","/api/v1/accounts","x=y",true,false).find("csrf_required")!=std::string::npos);
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    const auto saved_csrf=csrf;csrf[0]=csrf[0]=='a'?'b':'a';
    require(call("POST","/api/v1/accounts","x=y").find("csrf_required")!=std::string::npos);
    require(call("GET","/api/v1/accounts?after=0").find("csrf_required")!=std::string::npos);
    require(call("GET","/api/v1/accounts?after=0").find("WebAdminTarget")==std::string::npos);
    csrf=saved_csrf;
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    const auto saved_session=session;session[0]=session[0]=='a'?'b':'a';
    require(call("POST","/api/v1/accounts","x=y").find("csrf_required")!=std::string::npos);
    session=saved_session;
    require(call("GET","/api/v1/admin").starts_with("HTTP/1.1 200"));
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    require(call("POST","/api/v1/management","x=y").starts_with("HTTP/1.1 405"));
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    require(call("POST","/api/v1/accounts","actor_id="+actor_id+"&proof=Incorrect+administrator+password!&name=NoAccount&password=WebAdmin+test+password!").starts_with("HTTP/1.1 403"));
    require(repository.list_after().size()==2);
    actor=repository.update(actor,std::get<Account>(actor.account.with_access(Role::operator_,Status::active,now)));
    const auto limited=call("GET","/api/v1/management");
    require(limited.starts_with("HTTP/1.1 200"));
    const auto forbidden_review=call("GET","/api/v1/management/configuration");
    require(forbidden_review.starts_with("HTTP/1.1 403") && forbidden_review.find("\"saved\"")==std::string::npos);
    const auto forbidden_status=call("GET","/api/v1/management/hub/status");
    require(forbidden_status.starts_with("HTTP/1.1 403") && forbidden_status.find("\"active_state\"")==std::string::npos);
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    const auto forbidden_validation=call("POST","/api/v1/management/configuration","revision="+revision+"&network_max_connections=128");
    require(forbidden_validation.starts_with("HTTP/1.1 403") && forbidden_validation.find("\"valid\"")==std::string::npos);
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    require(call("POST","/api/v1/management/configuration/write","revision="+revision+"&network_max_connections=1").starts_with("HTTP/1.1 403"));
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    require(call("POST","/api/v1/management/configuration/restore","revision="+revision).starts_with("HTTP/1.1 403"));
    require(limited.find("\"key\":\"accounts\",\"permission\":\"accounts.manage\",\"allowed\":false,\"implemented\":true")!=std::string::npos);
    require(limited.find("\"key\":\"roles\",\"permission\":\"roles.manage\",\"allowed\":false,\"implemented\":true")!=std::string::npos);
    require(limited.find("\"key\":\"bans\",\"permission\":\"users.ban+blocks.manage\",\"allowed\":true,\"implemented\":true")!=std::string::npos);
    require(limited.find("\"key\":\"configuration\",\"permission\":\"configuration.manage\",\"allowed\":false,\"implemented\":false")!=std::string::npos);
    require(limited.find("\"key\":\"hub\",\"permission\":\"hub.manage\",\"allowed\":false,\"implemented\":true")!=std::string::npos);
    require(limited.find(grant)==std::string::npos&&limited.find(csrf)==std::string::npos&&limited.find("password")==std::string::npos);
    handler=api::account_handler(options,review);grant.clear();session.clear();csrf.clear();
    actor=repository.update(actor,std::get<Account>(actor.account.with_access(Role::administrator,Status::active,now)));
    const auto proved_contract=call("POST","/api/v1/admin/access","actor_id="+actor_id+"&proof=WebAdmin+operator+password!",false);
    require(proved_contract.starts_with("HTTP/1.1 200"));take_secrets(proved_contract);
    auto authorize_call=[&](std::string body,bool use_grant=true,bool bind=true){
        std::this_thread::sleep_for(std::chrono::milliseconds(1100));
        return call("POST","/api/v1/management/authorize",std::move(body),use_grant,bind);
    };
    const auto config_read=authorize_call("operation=configuration.read");
    require(config_read.starts_with("HTTP/1.1 501")&&config_read.find("\"error\":\"not_implemented\"")!=std::string::npos);
    require(config_read.find(grant)==std::string::npos&&config_read.find("password")==std::string::npos);
    require(authorize_call("operation=configuration.validate").find("\"error\":\"not_implemented\"")!=std::string::npos);
    require(authorize_call("operation=configuration.write").find("\"error\":\"not_implemented\"")!=std::string::npos);
    require(authorize_call("operation=configuration.restore").find("\"error\":\"not_implemented\"")!=std::string::npos);
    require(authorize_call("operation=hub.status").find("\"error\":\"management_refused\"")!=std::string::npos);
    require(authorize_call("operation=hub.start").find("\"error\":\"management_refused\"")!=std::string::npos);
    require(authorize_call("operation=hub.stop").find("\"error\":\"management_refused\"")!=std::string::npos);
    require(authorize_call("operation=hub.restart").find("\"error\":\"management_refused\"")!=std::string::npos);
    require(authorize_call("operation=configuration.execute").find("\"error\":\"invalid_input\"")!=std::string::npos);
    require(authorize_call("operation=hub.start&path=/tmp/dc24h.ini").find("\"error\":\"invalid_input\"")!=std::string::npos);
    require(authorize_call("operation=hub.restart&command=systemctl&service=dc24h").find("\"error\":\"invalid_input\"")!=std::string::npos);
    const auto authorize_get=call("GET","/api/v1/management/authorize");
    require(authorize_get.starts_with("HTTP/1.1 405")&&authorize_get.find("Allow: POST")!=std::string::npos);
    require(authorize_get.find("Allow: GET")==std::string::npos);
    require(authorize_call("operation=configuration.read",false).find("admin_grant_required")!=std::string::npos);
    require(authorize_call("operation=configuration.read",true,false).find("csrf_required")!=std::string::npos);
    actor=repository.update(actor,std::get<Account>(actor.account.with_access(Role::operator_,Status::active,now)));
    require(authorize_call("operation=configuration.read").find("\"error\":\"management_refused\"")!=std::string::npos);
    require(authorize_call("operation=hub.start").find("\"error\":\"management_refused\"")!=std::string::npos);
    actor=repository.update(actor,std::get<Account>(actor.account.with_access(Role::owner,Status::active,now)));
    const auto owner_hub=authorize_call("operation=hub.start");
    require(owner_hub.starts_with("HTTP/1.1 501")&&owner_hub.find("\"error\":\"not_implemented\"")!=std::string::npos);
    require(authorize_call("operation=hub.stop").find("\"error\":\"not_implemented\"")!=std::string::npos);
    require(authorize_call("operation=configuration.write").find("\"error\":\"not_implemented\"")!=std::string::npos);
    // The test process has no dedicated maintenance identity. No action can
    // reach the controller, so audit the definitive pre-send refusal as failed
    // rather than leaving an ambiguous started intent.
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    const auto unavailable_hub=call("POST","/api/v1/management/hub/start","confirm=START");
    require(unavailable_hub.starts_with("HTTP/1.1 503")&&
        unavailable_hub.find("\"error\":\"service_unavailable\"")!=std::string::npos);
    require(connection.scalar("SELECT outcome FROM dc24h_audit_events WHERE target_kind='service' "
        "AND action='hub.start' ORDER BY id DESC LIMIT 1")=="failed");
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    const auto denied_management=call("POST","/api/v1/management","operation=configuration.read");
    require(denied_management.starts_with("HTTP/1.1 405"));
    require(denied_management.find("Allow: GET")!=std::string::npos);
    actor=repository.update(actor,std::get<Account>(actor.account.with_access(Role::operator_,Status::active,now)));
    handler=api::account_handler(options,review);grant.clear();session.clear();csrf.clear();
    const auto proved_budget=call("POST","/api/v1/admin/access","actor_id="+actor_id+"&proof=WebAdmin+operator+password!",false);
    require(proved_budget.starts_with("HTTP/1.1 200"));take_secrets(proved_budget);
    for(std::uint32_t i=0;i<admin::session_request_limit;++i){
        require(call("GET","/api/v1/admin").starts_with("HTTP/1.1 200"));
    }
    const auto budget=call("GET","/api/v1/admin");
    require(budget.starts_with("HTTP/1.1 429")&&budget.find("rate_limited")!=std::string::npos);
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
