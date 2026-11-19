#include "dc24h/api/accounts.hpp"
#include "dc24h/api/http.hpp"
#include "dc24h/admin/access.hpp"
#include "dc24h/admin/core.hpp"
#include "dc24h/accounts/password_security.hpp"
#include "dc24h/accounts/registration.hpp"
#include <openssl/crypto.h>
#include <charconv>
#include <chrono>
#include <limits>
#include <map>

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
        ",\"role\":"+json_string(issued.role)+",\"status\":"+json_string(issued.status)+"},\"expires_in\":600,\"grant\":"+json_string(issued.grant)+",\"permissions\":[";
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
                ",\"outcome\":\"succeeded\"}";
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
AccountHandler account_handler(database::Options options) {
    admin::AccessGate gate(options);
    return [engine=admin::Engine(std::move(options),admin::Origin::maintenance_api),gate=std::move(gate),next=std::chrono::steady_clock::time_point{}](const Request& request) mutable {
        bool written=false;
        try {
            if(request.method=="POST") {
                const auto now=std::chrono::steady_clock::now();if(now<next){throw HttpError{429,"rate_limited"};}
                next=now+std::chrono::seconds(1);
            }
            if(request.target=="/api/v1/admin/access") {
                if(request.method!="POST"){throw HttpError{405,"method_not_allowed"};}
                Fields fields;form(request.body,fields);fields.exact({"actor_id","proof"});
                auto issued=gate.issue(number(fields.get("actor_id")),fields.get("proof"));
                const auto role=parse_role(issued.role);if(!role){throw HttpError{503,"account_unavailable"};}
                std::string body=access_json(issued, *role);
                OPENSSL_cleanse(issued.grant.data(),issued.grant.size());issued.grant.clear();
                auto http=response(200,body);OPENSSL_cleanse(body.data(),body.size());
                return http;
            }
            if(request.admin_grant.empty()){throw HttpError{401,"admin_grant_required"};}
            switch(gate.authorize(request.admin_grant,request.method,request.target)) {
            case admin::GrantCheck::accepted: break;
            case admin::GrantCheck::missing: throw HttpError{401,"admin_grant_required"};
            case admin::GrantCheck::forbidden: throw admin::Error(admin::ErrorKind::forbidden);
            case admin::GrantCheck::not_found: throw admin::Error(admin::ErrorKind::not_found);
            case admin::GrantCheck::unavailable: throw admin::Error(admin::ErrorKind::unavailable);
            case admin::GrantCheck::outcome_unknown: throw admin::Error(admin::ErrorKind::outcome_unknown);
            }
            return serve(request,engine,written);
        } catch(const HttpError& e){return response(e.status,"{\"error\":"+json_string(e.key)+"}");}
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
}
