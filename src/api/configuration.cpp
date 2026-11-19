#include "dc24h/api/configuration.hpp"
#include "dc24h/security/moderation.hpp"

namespace dc24h::api {
std::string configuration_json(const ConfigurationSnapshot& s) {
    std::string json="{\"mode\":\"";
    json+=s.maintenance?"maintenance":"hub";
    json+="\",\"restart_required\":true,\"database\":{\"transport\":\"unix\",";
    auto boolean=[&](std::string_view name,bool value){json+='"';json+=name;json+=value?"\":true,":"\":false,";};
    auto number=[&](std::string_view name,std::uint64_t value){json+='"';json+=name;json+="\":";json+=std::to_string(value);json+=',';};
    boolean("enabled",s.database_enabled);number("timeout_seconds",s.timeout_seconds);
    number("connect_timeout_seconds",s.connect_timeout_seconds);number("read_timeout_seconds",s.read_timeout_seconds);number("write_timeout_seconds",s.write_timeout_seconds);
    json.pop_back();json+="},\"authentication\":{";
    boolean("enabled",s.authentication_enabled);boolean("require_tls",s.require_tls);number("max_accounts",s.max_accounts);
    number("attempts_per_ip",s.attempts.attempts_per_ip);number("attempts_per_account",s.attempts.attempts_per_account);
    number("attempts_per_second",s.attempts.attempts_per_second);number("attempt_window_seconds",static_cast<std::uint64_t>(s.attempts.window.count()));
    number("max_tracked_attempt_ips",s.attempts.max_tracked_ips);number("max_tracked_attempt_accounts",s.attempts.max_tracked_accounts);
    number("login_timeout_seconds",static_cast<std::uint64_t>(s.login.login_timeout.count()));
    number("tls_handshake_timeout_seconds",static_cast<std::uint64_t>(s.login.tls_handshake_timeout.count()));
    number("output_timeout_seconds",static_cast<std::uint64_t>(s.login.output_timeout.count()));
    number("close_timeout_seconds",static_cast<std::uint64_t>(s.login.close_timeout.count()));
    number("max_pending_logins",s.login.max_pending_logins);number("max_pending_per_ip",s.login.max_pending_per_ip);
    number("max_login_bytes",s.login.max_login_bytes);number("max_login_frames",s.login.max_login_frames);
    json.pop_back();json+="},\"roles\":{\"vip_permissions\":[";
    bool first=true;
    for(auto p:security::permission_catalog) {
        const auto vip=security::allows(accounts::Role::vip,p,s.roles);
        if(vip&&!security::allows(accounts::Role::registered,p)) {
            if(!first){json+=',';}first=false;json+='"';json+=security::permission_key(p);json+='"';
        }
    }
    json+="],\"catalog\":[";first=true;
    for(auto role:accounts::role_catalog) {
        if(!first){json+=',';}first=false;
        json+="{\"key\":\"";json+=accounts::role_key(role);json+="\",\"rank\":";
        json+=std::to_string(*security::role_rank(role));json+=",\"permissions\":[";
        bool first_permission=true;
        for(auto p:security::permission_catalog)if(security::allows(role,p,s.roles)) {
            if(!first_permission){json+=',';}first_permission=false;json+='"';json+=security::permission_key(p);json+='"';
        }
        json+="]}";
    }
    json+="],\"operations\":[";first=true;
    for(auto p:security::permission_catalog) {
        if(!first){json+=',';}first=false;
        json+="{\"key\":\"";json+=security::permission_key(p);json+="\",\"implemented\":";
        json+=security::permission_implemented(p)?"true}":"false}";
    }
    json+="]},\"moderation\":[";first=true;
    for(auto operation:security::moderation_catalog) {
        const auto rule=security::moderation_rule(operation);
        if(!first){json+=',';}first=false;
        json+="{\"operation\":\"";json+=security::moderation_operation_key(operation);
        json+="\",\"permission\":\"";json+=security::permission_key(rule.permission);
        json+="\",\"subject\":\"";json+=security::moderation_subject_key(rule.subject);
        json+="\",\"target\":\"";json+=security::moderation_target_key(rule.target);
        json+="\",\"guest_target\":";json+=rule.guest_target?"true":"false";
        json+=",\"forbid_self\":";json+=rule.forbid_self?"true":"false";
        json+=",\"vip_delegable\":";json+=rule.vip_delegable?"true":"false";
        json+=",\"requires_blocks\":";json+=rule.requires_blocks?"true":"false";
        json+='}';
    }
    json+="]}";return json;
}
}
