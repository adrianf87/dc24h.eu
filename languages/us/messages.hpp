#pragma once
#include <array>
#include <string_view>

namespace dc24h::i18n::us {
inline constexpr std::array<std::string_view, 36> messages{{
    "Starting dc24h.eu ",
    "Core running",
    "Core stopping",
    "Core stopped after Network failure",
    "Core stopped",
    "Received SIGINT; requesting stop",
    "Received SIGTERM; requesting stop",
    "Network listening on ",
    "ADCS listening on ",
    "Network stopped; all TCP sockets closed; accepted=",
    "ADC stopped; frames=",
    "ADCS stopped; handshakes=",
    "TCP accept resource limit; retrying in one second",
    "TCP epoll watch resource limit; retrying in one second",
    "TCP listener failed",
    "Invalid arguments: ",
    ". Use dc24h --help.\n",
    "Unknown language (use us or pl)",
    "Unknown option",
    "Missing option value",
    "At most 16 listeners",
    "Numeric option outside allowed range",
    "ADCS listeners require both a TLS certificate and private key; maximum 16 listeners in total",
    "ADCS needs both --tls-cert and --tls-key",
    "ADCS needs both TLS certificate and private key with a TLS listener",
    "Cannot initialize TLS context",
    "Cannot set minimum TLS version",
    "Cannot load TLS certificate chain",
    "Cannot load TLS private key",
    "TLS certificate and private key do not match",
    "Cannot initialize TLS connection",
    "Cannot attach TLS socket",
    "Expected IPv4:port or [IPv6]:port",
    "Listener port must be an integer from 0 to 65535",
    "Listener address must be a numeric IPv4 or IPv6 address",
    "Use brackets only for IPv6 listeners"
}};
inline constexpr std::string_view help = R"HELP(Usage: dc24h [--listen IPv4:port | [IPv6]:port] ...
             [--adcs-listen IPv4:port | [IPv6]:port] ...
             [--tls-cert CHAIN.pem --tls-key KEY.pem]
             [--max-connections N] [--idle-timeout SECONDS] [--language us|pl]
       dc24h [--language us|pl] --help | --version
Run dc24h.eu in the foreground. Stop with SIGINT (Ctrl+C) or SIGTERM.
Defaults: --listen 127.0.0.1:1511 --max-connections 1024 --idle-timeout 120
Repeat --listen or --adcs-listen for up to 16 total ports; port 0 is ephemeral.
Explicit listeners replace the default; --adcs-listen alone enables TLS only.
ADCS requires a readable PEM chain and matching PEM private key. TLS >= 1.2.
Limits: connections 1..65535; idle timeout 1..86400 seconds. Default language: us.
ADC syntax parsing only: no ADC session handshake, routing or login.
)HELP";
} // namespace dc24h::i18n::us
