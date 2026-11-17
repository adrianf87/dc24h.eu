#pragma once
#include "dc24h/security/authentication_limits.hpp"
#include "dc24h/security/limits.hpp"
#include <string>

namespace dc24h::api {
// Deliberately cannot hold a database name/user/socket, secret, key or file path.
// Startup policy only: no health probe, live credential/account information or IO.
struct ConfigurationSnapshot {
    bool maintenance{false}, database_enabled{false};
    unsigned int timeout_seconds{5}, connect_timeout_seconds{5}, read_timeout_seconds{5}, write_timeout_seconds{5};
    bool authentication_enabled{false}, require_tls{true};
    std::size_t max_accounts{1024};
    security::AuthenticationLimits attempts{};
    security::Limits login{};
};
[[nodiscard]] std::string configuration_json(const ConfigurationSnapshot& settings);
}
