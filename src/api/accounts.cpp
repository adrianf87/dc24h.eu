#include "dc24h/api/accounts.hpp"
#include "dc24h/api/http.hpp"
#include "dc24h/admin/access.hpp"
#include "dc24h/admin/core.hpp"
#include "dc24h/accounts/password_security.hpp"
#include "dc24h/accounts/registration.hpp"
#include "../accounts/registration_internal.hpp"
#include "dc24h/configuration/configuration.hpp"
#include <openssl/crypto.h>
#include <charconv>
#include <chrono>
#include <limits>
#include <map>
#include <pwd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>

namespace dc24h::api {
namespace {
using namespace accounts;
struct HttpError { unsigned status; const char* key; };
[[noreturn]] void bad() { throw HttpError{400,"invalid_input"}; }
std::uint64_t number(std::string_view text, bool zero = false) {
    std::uint64_t value{};
    auto [end,error] = std::from_chars(text.data(),text.data()+text.size(),value);
    if (error != std::errc{} || end != text.data()+text.size() || std::to_string(value) != text ||
        (!zero && !value) || value > std::uint64_t(std::numeric_limits<std::int64_t>::max())) { bad(); }
    return value;
}
std::string json_string(std::string_view text) {
    std::string value="\"";
    for (char c : text) { if (c=='"' || c=='\\') { value+='\\'; } value+=c; }
    return value+'"'; // Account names are checked UTF-8 without control characters.
}
std::string json(const StoredAccount& stored) {
    const auto& a=stored.account; const auto& t=a.times();
    return "{\"id\":"+json_string(std::to_string(a.id().value()))+",\"name\":"+json_string(a.name().text())+
        ",\"role\":"+json_string(role_key(a.role()))+",\"status\":"+json_string(status_key(a.status()))+
        ",\"revision\":"+json_string(std::to_string(stored.revision))+",\"created_at\":"+std::to_string(t.created_at)+
        ",\"updated_at\":"+std::to_string(t.updated_at)+",\"last_login_at\":"+(t.last_login_at?std::to_string(*t.last_login_at):"null")+"}";
}
struct Fields {
    std::map<std::string,std::string> values;
    ~Fields() { for(auto& [key,value]:values){(void)key;OPENSSL_cleanse(value.data(),value.size());} }
    std::string_view get(const char* name) const { auto it=values.find(name); if(it==values.end()){bad();}return it->second; }
    void exact(std::initializer_list<const char*> keys) const {
        if(values.size()!=keys.size()){bad();}for(auto key:keys){(void)get(key);}
    }
};
int hex(char c) {
    if(c>='0'&&c<='9'){return c-'0';}if(c>='A'&&c<='F'){return c-'A'+10;}if(c>='a'&&c<='f'){return c-'a'+10;}bad();
}
void form(std::string_view body, Fields& fields) {
    if(body.empty()||body.size()>7000){bad();}
    while(!body.empty()) {
        auto end=body.find('&');auto part=body.substr(0,end);auto equal=part.find('=');
        if(equal==std::string_view::npos||equal==0||fields.values.size()>=7){bad();}
        std::string key(part.substr(0,equal));
        if(key.find_first_not_of("abcdefghijklmnopqrstuvwxyz_")!=std::string::npos){bad();}
        auto [it,inserted]=fields.values.emplace(std::move(key),std::string{});if(!inserted){bad();}
        auto encoded=part.substr(equal+1);auto& value=it->second;value.reserve(encoded.size());
        for(std::size_t i=0;i<encoded.size();++i){
            char c=encoded[i];if(c=='+'){c=' ';}else if(c=='%'){
                if(i+2>=encoded.size()){bad();}c=static_cast<char>(hex(encoded[i+1])*16+hex(encoded[i+2]));i+=2;
            }
            if(c=='\0'||static_cast<unsigned char>(c)<32||c==127){bad();}value+=c;
        }
        if(value.size()>1024){bad();}
        if(end==std::string_view::npos){break;}body.remove_prefix(end+1);if(body.empty()){bad();}
    }
}
std::string ban_json(const bans::Record& r){
    return "{\"id\":"+json_string(std::to_string(r.id))+",\"kind\":"+json_string(bans::kind_key(r.kind))+",\"subject\":"+json_string(r.subject)+",\"reason\":"+json_string(r.reason)+
        ",\"created_at\":"+std::to_string(r.created_at)+",\"expires_at\":"+(r.expires_at?std::to_string(*r.expires_at):"null")+",\"created_by\":"+json_string(std::to_string(r.created_by))+
        ",\"revoked_at\":"+(r.revoked_at?std::to_string(*r.revoked_at):"null")+",\"revoked_by\":"+(r.revoked_by?json_string(std::to_string(*r.revoked_by)):"null")+",\"revision\":"+json_string(std::to_string(r.revision))+"}";
}
std::string serve_bans(const Request& request,admin::Engine& engine,bool& written){
    std::string_view tail(request.target);tail.remove_prefix(std::string_view("/api/v1/bans").size());
    if(request.method=="GET"){
        std::uint64_t after{};auto filter=bans::Filter::all;
        if(!tail.empty()){
            if(tail.front()!='?'){throw HttpError{404,"not_found"};}tail.remove_prefix(1);if(tail.empty()){bad();}
            if(tail.starts_with("after=")){
                const auto end=tail.find('&');after=number(tail.substr(6,end==std::string_view::npos?end:end-6),true);
                if(end==std::string_view::npos){tail={};}else{tail.remove_prefix(end+1);if(tail.empty()){bad();}}
            }
            if(!tail.empty()){
                if(!tail.starts_with("state=")){bad();}const auto parsed=bans::parse_filter(tail.substr(6));if(!parsed){bad();}filter=*parsed;
            }
        }
        const auto now=bans::Directory::now();const auto rows=engine.bans_list(after,filter,now);
        std::string output="{\"maintenance\":true,\"restart_required\":true,\"observed_at\":"+std::to_string(now)+",\"bans\":[";
        for(const auto& r:rows){if(output.back()!='['){output+=',';}output+=ban_json(r);}
        return response(200,output+"],\"next_after\":"+(rows.size()==16?json_string(std::to_string(rows.back().id)):"null")+"}");
    }
    if(request.method!="POST"){throw HttpError{405,"method_not_allowed"};}
    Fields fields;form(request.body,fields);admin::Command command;command.time=bans::Directory::now();command.proof=fields.get("proof");command.actor.id=number(fields.get("actor_id"));
    if(tail.empty()){
        const bool duration=fields.values.contains("duration_seconds");
        if(duration){fields.exact({"actor_id","proof","kind","subject","reason","duration_seconds"});}
        else{fields.exact({"actor_id","proof","kind","subject","reason","expires_at"});}
        command.action=admin::Action::ban_create;
        const auto kind=bans::parse_kind(fields.get("kind"));if(!kind){bad();}command.ban_kind=*kind;command.subject=fields.get("subject");command.reason=fields.get("reason");
        if(duration){command.duration_seconds=number(fields.get("duration_seconds"));}
        else if(!fields.get("expires_at").empty()){command.expires_at=static_cast<bans::Timestamp>(number(fields.get("expires_at")));}
    }else{
        if(tail.front()!='/'||!tail.ends_with("/revoke")){throw HttpError{404,"not_found"};}
        fields.exact({"actor_id","proof","revision"});command.action=admin::Action::ban_revoke;
        command.target.id=number(tail.substr(1,tail.size()-8));command.target.revision=number(fields.get("revision"));
    }
    const auto result=engine.execute(command);written=true;
    return response(200,"{\"maintenance\":true,\"restart_required\":true,\"observed_at\":"+std::to_string(bans::Directory::now())+",\"ban\":"+ban_json(std::get<bans::Record>(result))+"}");
}
std::string access_json(const admin::IssuedAccess& issued, accounts::Role role) {
    std::string output = "{\"maintenance\":true,\"restart_required\":true,\"actor\":{\"id\":"+json_string(std::to_string(issued.id))+
        ",\"role\":"+json_string(issued.role)+",\"status\":"+json_string(issued.status)+"},\"expires_in\":600,\"grant\":"+json_string(issued.grant)+
        ",\"session\":"+json_string(issued.session)+",\"csrf\":"+json_string(issued.csrf)+",\"permissions\":[";
    for (const auto& fact : admin::permission_facts(role)) {
        if (output.back() != '[') { output += ','; }
        output += "{\"key\":"+json_string(fact.key)+",\"granted\":"+(fact.granted?"true":"false")+",\"implemented\":"+(fact.implemented?"true":"false")+"}";
    }
    output += "],\"routes\":[";
    for (const auto& route : admin::access_routes) {
        if (output.back() != '[') { output += ','; }
        const bool allowed = admin::catalog_allowed(role, route.permission);
        output += "{\"method\":"+json_string(route.method)+",\"path\":"+json_string(route.path)+",\"permission\":"+json_string(route.permission)+
            ",\"allowed\":"+(allowed?"true":"false")+"}";
    }
    return output+"]}";
}
std::string administration(const admin::Engine& engine) {
    const auto counters=engine.statistics();
    std::string output="{\"maintenance\":true,\"actions\":[";
    for(auto action:admin::action_catalog) {
        if(output.back()!='['){output+=',';}
        const bool owner=action==admin::Action::bootstrap_owner||action==admin::Action::transfer_owner;
        output+="{\"key\":"+json_string(admin::action_key(action))+",\"available\":"+(admin::available(action,engine.origin())?"true":"false")+
            ",\"local_owner_only\":"+(owner?"true":"false")+"}";
    }
    output+="],\"statistics\":{\"attempts\":"+json_string(std::to_string(counters.attempts))+
        ",\"succeeded\":"+json_string(std::to_string(counters.succeeded))+",\"refused\":"+json_string(std::to_string(counters.refused))+
        ",\"unavailable\":"+json_string(std::to_string(counters.unavailable))+",\"outcome_unknown\":"+json_string(std::to_string(counters.outcome_unknown))+"}}";
    return response(200,output);
}
std::string management_json(accounts::Role role) {
    struct Area { const char* key; const char* permission; bool implemented; };
    constexpr Area areas[]={
        {"accounts","accounts.manage",true},
        {"roles","roles.manage",true},
        {"bans","users.ban+blocks.manage",true},
        {"configuration","configuration.manage",false},
        {"hub","hub.manage",true},
    };
    std::string output="{\"schema\":2,\"maintenance\":true,\"restart_required\":true,\"areas\":[";
    for(const auto& area:areas){
        if(output.back()!='['){output+=',';}
        const bool allowed=admin::catalog_allowed(role,area.permission);
        output+="{\"key\":"+json_string(area.key)+",\"permission\":"+json_string(area.permission)+
            ",\"allowed\":"+(allowed?"true":"false")+",\"implemented\":"+(area.implemented?"true":"false")+"}";
    }
    return response(200,output+"]}");
}
std::string configuration_review_json(const configuration::ReviewSnapshot& snapshot) {
    auto fields = [&](const configuration::ReviewOptions& options) {
        std::string result = "{";
        for (const auto& option : options) {
            if (result.size() > 1) { result += ','; }
            result += json_string(option.key) + ':' + json_string(option.value);
        }
        return result + '}';
    };
    return "{\"schema\":1,\"maintenance\":true,\"read_only\":true,\"restart_required\":true,"
        "\"active_scope\":\"maintenance_process\",\"hub_active_available\":false,\"revision\":" +
        json_string(snapshot.revision) + ",\"matches_active\":" + (snapshot.matches_active ? "true" : "false") +
        ",\"saved\":" + fields(snapshot.saved) + ",\"active\":" + fields(snapshot.active) + '}';
}
std::string configuration_review_request(const Request& request, const configuration::Review& review) {
    if (request.method == "GET") {
        return response(200, configuration_review_json(review.read()));
    }
    if (request.method != "POST") { return response(405, "{\"error\":\"method_not_allowed\"}", false, "GET, POST"); }
    Fields fields;
    form(request.body, fields);
    const auto revision = fields.get("revision");
    if (revision.size() != 64 || revision.find_first_not_of("0123456789abcdef") != std::string_view::npos ||
        fields.values.size() < 2 || fields.values.size() > 5) { bad(); }
    std::vector<std::pair<std::string,std::string>> changes;
    for (const auto& [key,value] : fields.values) {
        if (key == "revision") { continue; }
        if (key != "network_max_connections" && key != "network_idle_timeout" &&
            key != "logging_level" && key != "general_language") { bad(); }
        changes.emplace_back(key,value);
    }
    const bool valid = review.validate_candidate(revision, changes);
    return response(200, valid ? "{\"valid\":true,\"written\":false,\"restart_required\":true}" :
        "{\"valid\":false,\"written\":false,\"restart_required\":true}");
}
std::string configuration_write_request(const Request& request, const configuration::Review& review,
    const database::Options& database_options, std::uint64_t actor, bool& written) {
    if (request.method != "POST") { throw HttpError{405,"method_not_allowed"}; }
    const bool restoring = request.target == "/api/v1/management/configuration/restore";
    Fields fields;form(request.body, fields);
    const auto revision = fields.get("revision");
    if (revision.size() != 64 || revision.find_first_not_of("0123456789abcdef") != std::string_view::npos) { bad(); }
    std::vector<std::pair<std::string,std::string>> changes;
    if (restoring) { fields.exact({"revision"}); }
    else {
        if (fields.values.size() < 2 || fields.values.size() > 5) { bad(); }
        for (const auto& [key,value] : fields.values) {
            if (key == "revision") { continue; }
            if (key != "network_max_connections" && key != "network_idle_timeout" &&
                key != "logging_level" && key != "general_language") { bad(); }
            changes.emplace_back(key,value);
        }
        if (!review.validate_candidate(revision,changes)) { throw HttpError{422,"invalid_configuration"}; }
    }
    database::Connection connection(database_options);
    audit::Repository audit(connection);
    const auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    const auto entry = audit.begin_configuration(restoring ? "configuration.restore" : "configuration.write", actor,timestamp);
    std::string new_revision;
    try {
        new_revision = restoring ? review.restore_backup(revision) : review.write_candidate(revision,changes);
    } catch (const configuration::WriteOutcomeUnknown&) {
        // Durable 'started' audit entry tells operators that recovery may be needed.
        throw;
    } catch (...) {
        try { audit.finish_configuration(entry,actor,false); }
        catch (...) { throw configuration::WriteOutcomeUnknown(); }
        throw;
    }
    written = true; // A later audit failure must never imply that the INI stayed unchanged.
    audit.finish_configuration(entry,actor,true);
    return response(200,"{\"written\":true,\"restart_required\":true,\"revision\":"+
        json_string(new_revision)+",\"backup_available\":true}");
}

// Only the separate maintenance API can reach this fixed local controller.
// Verify its kernel peer credentials; never trust a browser-selected socket.
std::string controller_action(std::string_view command) {
    // A hub worker (or root-running development API) cannot impersonate the
    // independently provisioned maintenance service to control its own unit.
    const auto* process=::getpwuid(::geteuid());
    if(!process||std::string_view(process->pw_name)!="dc24h-maintenance"||::geteuid()==0)
        throw HttpError{503,"service_unavailable"};
    const auto* identity=::getpwnam("dc24h-control");
    if(!identity||identity->pw_uid==0){throw HttpError{503,"service_unavailable"};}
    const uid_t expected=identity->pw_uid;
    const int fd=::socket(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC,0);
    if(fd<0){throw HttpError{503,"service_unavailable"};}
    struct Close { int fd; ~Close(){(void)::close(fd);} } close{fd};
    const bool read_only=command=="STATUS";
    const timeval timeout{read_only?4L:28L,0};
    (void)::setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
    (void)::setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout));
    sockaddr_un address{};address.sun_family=AF_UNIX;
    constexpr char path[]="/run/dc24h-control/status.sock";
    static_assert(sizeof(path)<sizeof(address.sun_path));
    std::memcpy(address.sun_path,path,sizeof(path));
    if(::connect(fd,reinterpret_cast<sockaddr*>(&address),sizeof(address))!=0)
        throw HttpError{503,"service_unavailable"};
    struct ucred peer{};socklen_t length=sizeof(peer);
    if(::getsockopt(fd,SOL_SOCKET,SO_PEERCRED,&peer,&length)!=0||length!=sizeof(peer)||peer.uid!=expected)
        throw HttpError{503,"service_unavailable"};
    const std::string message=std::string(command)+"\n";
    if(::send(fd,message.data(),message.size(),MSG_NOSIGNAL)!=static_cast<ssize_t>(message.size()))
        throw HttpError{503,read_only?"service_unavailable":"outcome_unknown"};
    std::string result;
    char text[512]{};
    while(result.size()<sizeof(text)) {
        const auto count=::recv(fd,text,sizeof(text)-result.size(),0);
        if(count<=0)throw HttpError{503,read_only?"service_unavailable":"outcome_unknown"};
        result.append(text,static_cast<std::size_t>(count));
        if(result.ends_with('\n'))break;
    }
    if(!result.ends_with('\n')||result.find('\0')!=std::string::npos||
       result.size()==sizeof(text))throw HttpError{503,read_only?"service_unavailable":"outcome_unknown"};
    return result;
}

std::string hub_status_request(const Request& request) {
    if(request.method!="GET")throw HttpError{405,"method_not_allowed"};
    if(!request.body.empty())throw HttpError{400,"bad_request"};
    auto result=controller_action("STATUS");
    // The fixed, peer-verified controller is the only source of service state.
    // Reject unexpected wire output rather than reflecting it to a browser.
    if(!result.starts_with("{\"schema\":1,\"available\":") ||
       (result.find("\"unit\":\"dc24h-twilight.service\"")==std::string::npos &&
        result.find("\"available\":false")==std::string::npos) ||
       result.find('\r')!=std::string::npos)
        throw HttpError{503,"service_unavailable"};
    return response(200,result);
}

std::string hub_action_request(const Request& request,const database::Options& options,
    std::uint64_t actor,bool& written) {
    if(request.method!="POST"){throw HttpError{405,"method_not_allowed"};}
    const std::string_view prefix="/api/v1/management/hub/";
    const auto operation=std::string_view(request.target).substr(prefix.size());
    if(operation!="start"&&operation!="stop"&&operation!="restart"){bad();}
    const std::string upper=operation=="start"?"START":operation=="stop"?"STOP":"RESTART";
    Fields fields;form(request.body,fields);fields.exact({"confirm"});
    if(fields.get("confirm")!=upper){throw HttpError{400,"confirmation_required"};}
    database::Connection connection(options);
    audit::Repository audit(connection);
    const auto timestamp=std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    const auto event=audit.begin_service("hub."+std::string(operation),actor,timestamp);
    std::string result;
    try{result=controller_action(upper);}
    catch(const HttpError& error){
        if(std::string_view(error.key)!="service_unavailable")throw HttpError{503,"outcome_unknown"};
        // Identity, connection or peer verification failed before sending a
        // command. This is a definite failure, not an ambiguous systemd action.
        try{audit.finish_service(event,actor,false);}
        catch(...){throw HttpError{503,"outcome_unknown"};}
        throw;
    }
    catch(...){throw HttpError{503,"outcome_unknown"};} // Unknown after delivery; preserve started audit intent.
    if(result=="ERROR outcome_unknown\n")throw HttpError{503,"outcome_unknown"};
    const bool succeeded=result==(operation=="stop"?"OK inactive\n":"OK active\n");
    const bool conflict=result=="ERROR already_active\n"||result=="ERROR already_inactive\n"||result=="ERROR busy\n";
    const bool failure=conflict||result=="ERROR unavailable\n"||result=="ERROR action_failed\n"||
        result=="ERROR unauthorized\n"||result=="ERROR unsupported_request\n";
    if(!succeeded&&!failure)throw HttpError{503,"outcome_unknown"};
    try{audit.finish_service(event,actor,succeeded);}
    catch(...){throw HttpError{503,"outcome_unknown"};}
    if(!succeeded)throw HttpError{conflict?409u:503u,"service_refused"};
    written=true;
    return response(200,"{\"operation\":"+json_string(operation)+",\"outcome\":\"succeeded\",\"active_state\":"+
        json_string(operation=="stop"?"inactive":"active")+"}");
}
void management_contract(accounts::Role role, std::string_view body) {
    Fields fields;form(body,fields);fields.exact({"operation"});
    const auto operation=fields.get("operation");
    const char* permission=nullptr;
    if(operation=="configuration.read"||operation=="configuration.validate"||operation=="configuration.write"||operation=="configuration.restore"){
        permission="configuration.manage";
    }else if(operation=="hub.status"||operation=="hub.start"||operation=="hub.stop"||operation=="hub.restart"){
        permission="hub.manage";
    }else{bad();}
    if(!admin::catalog_allowed(role,permission)){throw admin::Error(admin::ErrorKind::forbidden);}
    // The permission is granted and the engine is still absent. Do not open a file or spawn a process.
    throw HttpError{501,"not_implemented"};
}
std::string serve(const Request& request,admin::Engine& engine,bool& written) {
    if(request.target=="/api/v1/audit"){
        if(request.method!="POST"){throw HttpError{405,"method_not_allowed"};}
        Fields fields;form(request.body,fields);fields.exact({"actor_id","proof","after"});
        const auto rows=engine.audit_list(number(fields.get("actor_id")),fields.get("proof"),number(fields.get("after"),true));
        std::string output="{\"maintenance\":true,\"restart_required\":true,\"events\":[";
        for(const auto& event:rows){
            if(output.back()!='['){output+=',';}
            output+="{\"id\":"+json_string(std::to_string(event.id))+",\"occurred_at\":"+std::to_string(event.occurred_at)+
                ",\"action\":"+json_string(event.action)+",\"actor_id\":"+json_string(std::to_string(event.actor_id))+
                ",\"target_kind\":"+json_string(event.target_kind)+",\"target_id\":"+json_string(std::to_string(event.target_id))+
                ",\"outcome\":"+json_string(event.outcome)+"}";
        }
        return response(200,output+"],\"next_after\":"+(rows.size()==audit::page_size?json_string(std::to_string(rows.back().id)):"null")+"}");
    }
    if(request.target.starts_with("/api/v1/bans")){return serve_bans(request,engine,written);}
    if(request.target=="/api/v1/admin") {
        if(request.method!="GET"){throw HttpError{405,"method_not_allowed"};}
        return administration(engine);
    }
    if(!request.target.starts_with("/api/v1/accounts")){throw HttpError{404,"not_found"};}
    constexpr std::string_view root="/api/v1/accounts";
    std::string_view tail(request.target);tail.remove_prefix(root.size());
    if(request.method!="GET"&&request.method!="POST"){throw HttpError{405,"method_not_allowed"};}
    std::uint64_t after=0; bool staff = false;
    if(request.method=="GET"&&tail.starts_with("?after=")){after=number(tail.substr(7),true);tail={};}
    if(request.method=="GET"&&tail.starts_with("?staff_after=")){after=number(tail.substr(13),true);tail={};staff=true;}
    if(!tail.empty()&&tail.front()!='/'){throw HttpError{404,"not_found"};}
    if(request.method=="GET") {
        if(tail.empty()) {
            const auto page=engine.list(after,staff);std::string output="{\"accounts\":[";
            for(const auto& value:page){if(output.back()!='['){output+=',';}output+=json(value);}
            return response(200,output+"],\"maintenance\":true,\"staff_only\":"+(staff?"true":"false")+",\"next_after\":"+
                (page.size()==25?json_string(std::to_string(page.back().account.id().value())):"null")+"}");
        }
        return response(200,"{\"account\":"+json(engine.account(number(tail.substr(1))))+",\"maintenance\":true}");
    }
    Fields fields;form(request.body,fields);
    const auto now=std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    admin::Command command;command.time=now;command.proof=fields.get("proof");
    if(tail.empty()) {
        fields.exact({"actor_id","proof","name","password"});
        command.action=admin::Action::create;command.actor.id=number(fields.get("actor_id"));
        command.name=fields.get("name");command.password=fields.get("password");
    } else {
        tail.remove_prefix(1);const auto slash=tail.find('/');if(slash==std::string_view::npos){bad();}
        command.target.id=number(tail.substr(0,slash));const auto action=tail.substr(slash+1);
        const auto parsed=admin::parse_action("accounts."+std::string(action));
        if(!parsed||*parsed==admin::Action::create){throw HttpError{404,"not_found"};}command.action=*parsed;
        if(action=="password"){fields.exact({"revision","proof","password"});}
        else if(action=="reset"){fields.exact({"revision","actor_id","proof","password"});}
        else if(action=="role"){fields.exact({"revision","actor_id","proof","role"});}
        else{fields.exact({"revision","actor_id","proof"});}
        command.target.revision=number(fields.get("revision"));
        if(action!="password"){command.actor.id=number(fields.get("actor_id"));}
        if(action=="password"||action=="reset"){command.password=fields.get("password");}
        if(action=="role"){const auto role=parse_role(fields.get("role"));if(!role){bad();}command.role=*role;}
    }
    const auto result=engine.execute(command);written=true;const auto& updated=std::get<StoredAccount>(result);
    return response(200,"{\"account\":"+json(updated)+",\"maintenance\":true}");
}
}
AccountHandler account_handler(database::Options options, std::shared_ptr<const configuration::Review> review) {
    admin::AccessGate gate(options);
    return [engine=admin::Engine(options,admin::Origin::maintenance_api),audit_options=std::move(options),gate=std::move(gate),review=std::move(review),next=std::chrono::steady_clock::time_point{}](const Request& request) mutable {
        bool written=false;
        try {
            if(request.method=="POST") {
                const auto now=std::chrono::steady_clock::now();if(now<next){throw HttpError{429,"rate_limited"};}
                next=now+std::chrono::seconds(1);
            }
            if(request.target=="/api/v1/admin/access") {
                if(request.method!="POST"){throw HttpError{405,"method_not_allowed"};}
                Fields fields;form(request.body,fields);
                std::uint64_t actor_id{};
                if(fields.values.contains("account_name")) {
                    fields.exact({"account_name","proof"});
                    auto name=AccountName::create(fields.get("account_name"));
                    if(!name){bad();}
                    database::Connection connection(audit_options);
                    auto named=AccountRepository(connection).find_by_name(*name);
                    // Never reveal whether an unknown name or non-staff account
                    // exists. Password proof follows in AccessGate::issue.
                    if(!named || (named->account.role()!=Role::owner &&
                                    named->account.role()!=Role::administrator)) {
                        throw admin::Error(admin::ErrorKind::forbidden);
                    }
                    actor_id=named->account.id().value();
                } else {
                    fields.exact({"actor_id","proof"});
                    actor_id=number(fields.get("actor_id"));
                }
                auto issued=gate.issue(actor_id,fields.get("proof"));
                const auto role=parse_role(issued.role);if(!role){throw HttpError{503,"account_unavailable"};}
                std::string body=access_json(issued, *role);
                OPENSSL_cleanse(issued.grant.data(),issued.grant.size());issued.grant.clear();
                OPENSSL_cleanse(issued.csrf.data(),issued.csrf.size());issued.csrf.clear();
                OPENSSL_cleanse(issued.session.data(),issued.session.size());issued.session.clear();
                auto http=response(200,body);OPENSSL_cleanse(body.data(),body.size());
                return http;
            }
            if(request.admin_grant.empty()){throw HttpError{401,"admin_grant_required"};}
            switch(gate.authorize(request.admin_grant,request.method,request.target,request.admin_session,request.admin_csrf)) {
            case admin::GrantCheck::accepted: break;
            case admin::GrantCheck::missing: throw HttpError{401,"admin_grant_required"};
            case admin::GrantCheck::forbidden: throw admin::Error(admin::ErrorKind::forbidden);
            case admin::GrantCheck::not_found: throw admin::Error(admin::ErrorKind::not_found);
            case admin::GrantCheck::unavailable: throw admin::Error(admin::ErrorKind::unavailable);
            case admin::GrantCheck::outcome_unknown: throw admin::Error(admin::ErrorKind::outcome_unknown);
            case admin::GrantCheck::csrf_required: throw HttpError{403,"csrf_required"};
            case admin::GrantCheck::rate_limited: throw HttpError{429,"rate_limited"};
            }
            if(request.target=="/api/v1/management/configuration") {
                if (!review) { throw HttpError{501,"not_implemented"}; }
                return configuration_review_request(request,*review);
            }
            if(request.target=="/api/v1/management/configuration/write" ||
               request.target=="/api/v1/management/configuration/restore") {
                if (!review) { throw HttpError{501,"not_implemented"}; }
                const auto actor=gate.authorized_actor(request.admin_grant);
                if (!actor) { throw admin::Error(admin::ErrorKind::forbidden); }
                return configuration_write_request(request,*review,audit_options,*actor,written);
            }
            if(request.target=="/api/v1/management/hub/status") {
                return hub_status_request(request);
            }
            if(request.target=="/api/v1/management/hub/start"||
               request.target=="/api/v1/management/hub/stop"||
               request.target=="/api/v1/management/hub/restart") {
                const auto actor=gate.authorized_actor(request.admin_grant);
                if(!actor){throw admin::Error(admin::ErrorKind::forbidden);}
                return hub_action_request(request,audit_options,*actor,written);
            }
            if(request.target=="/api/v1/management"||request.target=="/api/v1/management/authorize"){
                const auto role=gate.actor_role(request.admin_grant);
                if(!role){throw admin::Error(admin::ErrorKind::forbidden);}
                if(request.target=="/api/v1/management/authorize"){
                    if(request.method!="POST"){return response(405,"{\"error\":\"method_not_allowed\"}",false,"POST");}
                    management_contract(*role,request.body);
                }
                if(request.method!="GET"){throw HttpError{405,"method_not_allowed"};}
                return management_json(*role);
            }
            return serve(request,engine,written);
        } catch(const HttpError& e){return response(e.status,"{\"error\":"+json_string(e.key)+"}");}
        catch(const configuration::StaleReview&){return response(409,"{\"error\":\"stale\"}");}
        catch(const configuration::WriteOutcomeUnknown&){return response(503,"{\"error\":\"outcome_unknown\"}");}
        catch(const admin::Error& e){
            const auto kind=e.kind();
            const unsigned status=kind==admin::ErrorKind::forbidden?403:kind==admin::ErrorKind::stale?409:kind==admin::ErrorKind::not_found?404:kind==admin::ErrorKind::invalid_input?400:503;
            return response(status,kind==admin::ErrorKind::outcome_unknown?"{\"error\":\"outcome_unknown\"}":"{\"error\":\"management_refused\"}");
        }
        catch(const ManagementError& e){return response(e.kind()==ManagementErrorKind::stale?409:e.kind()==ManagementErrorKind::unauthorized?403:400,"{\"error\":\"management_refused\"}");}
        catch(const RegistrationError& e){return response(e.kind()==RegistrationErrorKind::crypto||e.kind()==RegistrationErrorKind::schema?503:400,"{\"error\":\"invalid_registration\"}");}
        catch(const PasswordSecurityError& e){return response(e.kind()==PasswordSecurityErrorKind::invalid_password?403:503,"{\"error\":\"authorization_unavailable\"}");}
        catch(const RepositoryError& e){return response(e.kind()==RepositoryErrorKind::duplicate||e.kind()==RepositoryErrorKind::stale?409:503,"{\"error\":\"account_unavailable\"}");}
        catch(const database::Error& e){return response(503,e.outcome_unknown()?"{\"error\":\"outcome_unknown\"}":"{\"error\":\"database_unavailable\"}");}
        catch(...){return response(503,written?"{\"error\":\"outcome_unknown\"}":"{\"error\":\"account_unavailable\"}");}
    };
}

AccountHandler public_account_access_handler(database::Options options) {
    auto private_auth = account_handler(std::move(options));
    return [private_auth = std::move(private_auth)](const Request& request) mutable {
        // Exact method/path allowlist, deliberately narrower than the
        // maintenance API. Do not expose its write-capable dispatch here.
        if ((request.method == "POST" && request.target == "/api/v1/admin/access") ||
            (request.method == "GET" && request.target == "/api/v1/management")) {
            return private_auth(request);
        }
        return response(404, "{\"error\":\"not_found\"}");
    };
}

AccountHandler stage4_account_handler(database::Options options) {
    // Equalize the expensive branch for unknown and ineligible identities.
    auto dummy = std::make_shared<accounts::detail::EncodedPassword>();
    accounts::detail::hash_password("stage4 dummy password proof", *dummy);
    return [options=std::move(options),dummy=std::move(dummy)](const Request& request) {
        if (request.method != "POST" || request.target != "/api/v1/admin/access") {
            return response(404, "{\"error\":\"not_found\"}");
        }
        try {
            using namespace accounts;
            Fields fields;
            form(request.body, fields);
            const auto action = fields.get("action");
            const auto proof = fields.get("proof");
            if (action == "prepare") {
                fields.exact({"action","account_name","proof"});
                if (!AccountName::create(fields.get("account_name"))) { bad(); }
                accounts::detail::EncodedPassword verifier;
                accounts::detail::hash_password(proof, verifier);
                auto encoded = std::string(reinterpret_cast<const char*>(verifier.bytes.data()), verifier.bytes.size());
                return response(200, "{\"verifier\":" + json_string(encoded) + "}");
            }
            database::Connection connection(options);
            AccountRepository repository(connection);
            if (action == "change") {
                fields.exact({"action","actor_id","revision","proof","password"});
                const auto id = AccountId::from_value(number(fields.get("actor_id")));
                if (!id) { bad(); }
                const auto account = repository.find_by_id(*id);
                if (!account || account->revision != number(fields.get("revision"))) {
                    return response(403, "{\"error\":\"proof_refused\"}");
                }
                const auto now = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                (void)AccountManagementService(connection).change_password(*account, proof, fields.get("password"), now);
                return response(200, "{\"changed\":true}");
            }
            if (action != "prove") { bad(); }
            fields.exact({"action","account_name","proof"});
            const auto name = AccountName::create(fields.get("account_name"));
            if (!name) { bad(); }
            const auto account = repository.find_by_name(*name);
            const bool pending_owner = account && account->account.role() == Role::owner &&
                connection.scalar("SELECT COUNT(*) FROM stage4_emails WHERE account_id=? "
                    "AND initial_owner=1 AND verified_at IS NULL AND email='admin@dc24h.eu'",
                    {std::to_string(account->account.id().value())}) == "1";
            if (!account || (account->account.status() != Status::active && !pending_owner)) {
                const auto bytes = std::string_view(reinterpret_cast<const char*>(dummy->bytes.data()), dummy->bytes.size());
                (void)accounts::detail::verify_password(proof, bytes);
                return response(403, "{\"error\":\"proof_refused\"}");
            }
            if (PasswordSecurityService(connection).verify(account->account.id(), proof) != PasswordVerification::matched ||
                repository.find_by_id(account->account.id()) != account) {
                return response(403, "{\"error\":\"proof_refused\"}");
            }
            return response(200, "{\"account\":" + json(*account) + "}");
        } catch (const HttpError&) { return response(400, "{\"error\":\"invalid_input\"}"); }
        catch (const RegistrationError&) { return response(403, "{\"error\":\"proof_refused\"}"); }
        catch (const PasswordSecurityError&) { return response(403, "{\"error\":\"proof_refused\"}"); }
        catch (const ManagementError&) { return response(403, "{\"error\":\"proof_refused\"}"); }
        catch (...) { return response(503, "{\"error\":\"unavailable\"}"); }
    };
}
}
