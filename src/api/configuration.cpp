#include "dc24h/api/configuration.hpp"

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
    json.pop_back();json+="}}";return json;
}
}
