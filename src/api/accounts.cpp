#include "dc24h/api/accounts.hpp"
#include "dc24h/api/http.hpp"
#include "dc24h/accounts/management.hpp"
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
        if(equal==std::string_view::npos||equal==0||fields.values.size()>=6){bad();}
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
StoredAccount find(AccountRepository& repository,std::string_view id) {
    auto found=repository.find_by_id(*AccountId::from_value(number(id)));
    if(!found){throw HttpError{404,"not_found"};}return *found;
}
std::string serve(const Request& request,const database::Options& options) {
    constexpr std::string_view root="/api/v1/accounts";
    std::string_view tail(request.target);tail.remove_prefix(root.size());
    if(request.method!="GET"&&request.method!="POST"){throw HttpError{405,"method_not_allowed"};}
    std::uint64_t after=0;
    if(request.method=="GET"&&tail.starts_with("?after=")){after=number(tail.substr(7),true);tail={};}
    if(!tail.empty()&&tail.front()!='/'){throw HttpError{404,"not_found"};}
    database::Connection connection(options);AccountRepository repository(connection);
    if(request.method=="GET") {
        if(tail.empty()) {
            const auto page=repository.list_after(after);std::string output="{\"accounts\":[";
            for(const auto& value:page){if(output.back()!='['){output+=',';}output+=json(value);}
            return response(200,output+"],\"maintenance\":true,\"next_after\":"+
                (page.size()==25?json_string(std::to_string(page.back().account.id().value())):"null")+"}");
        }
        return response(200,"{\"account\":"+json(find(repository,tail.substr(1)))+",\"maintenance\":true}");
    }
    Fields fields;form(request.body,fields);
    const auto now=std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    if(tail.empty()) {
        fields.exact({"actor_id","proof","name","password"});
        const auto actor=find(repository,fields.get("actor_id"));
        const auto account=AccountManagementService(connection).create_account(actor,fields.get("proof"),fields.get("name"),fields.get("password"),now);
        return response(200,"{\"account\":"+json(account)+",\"maintenance\":true}");
    }
    tail.remove_prefix(1);const auto slash=tail.find('/');if(slash==std::string_view::npos){bad();}
    const auto target=find(repository,tail.substr(0,slash));const auto action=tail.substr(slash+1);
    if(action=="password"){fields.exact({"revision","proof","password"});}
    else if(action=="reset"){fields.exact({"revision","actor_id","proof","password"});}
    else if(action=="activate"||action=="disable"){fields.exact({"revision","actor_id","proof"});}
    else{throw HttpError{404,"not_found"};}
    if(target.revision!=number(fields.get("revision"))){throw HttpError{409,"stale"};}
    AccountManagementService management(connection);
    auto updated=[&] {
        if(action=="password"){return management.change_password(target,fields.get("proof"),fields.get("password"),now);}
        const auto actor=find(repository,fields.get("actor_id"));
        if(action=="reset"){return management.reset_password(actor,fields.get("proof"),target,fields.get("password"),now);}
        if(action=="activate"){return management.activate_account(actor,fields.get("proof"),target,now);}
        return management.disable_account(actor,fields.get("proof"),target,now);
    }();
    return response(200,"{\"account\":"+json(updated)+",\"maintenance\":true}");
}
}
AccountHandler account_handler(database::Options options) {
    return [options=std::move(options),next=std::chrono::steady_clock::time_point{}](const Request& request) mutable {
        try {
            if(request.method=="POST") {
                const auto now=std::chrono::steady_clock::now();if(now<next){throw HttpError{429,"rate_limited"};}
                next=now+std::chrono::seconds(1);
            }
            return serve(request,options);
        } catch(const HttpError& e){return response(e.status,"{\"error\":"+json_string(e.key)+"}");}
        catch(const ManagementError& e){return response(e.kind()==ManagementErrorKind::stale?409:e.kind()==ManagementErrorKind::unauthorized?403:400,"{\"error\":\"management_refused\"}");}
        catch(const RegistrationError& e){return response(e.kind()==RegistrationErrorKind::crypto||e.kind()==RegistrationErrorKind::schema?503:400,"{\"error\":\"invalid_registration\"}");}
        catch(const PasswordSecurityError& e){return response(e.kind()==PasswordSecurityErrorKind::invalid_password?403:503,"{\"error\":\"authorization_unavailable\"}");}
        catch(const RepositoryError& e){return response(e.kind()==RepositoryErrorKind::duplicate||e.kind()==RepositoryErrorKind::stale?409:503,"{\"error\":\"account_unavailable\"}");}
        catch(const database::Error& e){return response(503,e.outcome_unknown()?"{\"error\":\"outcome_unknown\"}":"{\"error\":\"database_unavailable\"}");}
        catch(...){return response(503,"{\"error\":\"account_unavailable\"}");}
    };
}
}
