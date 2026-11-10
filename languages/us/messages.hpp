#pragma once
#include <array>
#include <string_view>

namespace dc24h::i18n::us {
inline constexpr std::array<std::string_view, 116> messages{{
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
    "Use brackets only for IPv6 listeners",
    "Configuration OK (network ports not bound; log file not opened)\n",
    "Configuration check failed: ",
    "Startup failure: ",
    "Invalid numeric configuration value",
    "Log level must be info, warning, error or off",
    "Empty configuration path",
    "Invalid control character in path",
    "Cannot resolve configured path",
    "none cannot be combined with listener addresses",
    "Invalid listener list",
    "Invalid listener address",
    "Duplicate listener endpoint",
    "Cannot open configuration file (regular file required, no symlink)",
    "Configuration must be a regular file of at most 64 KiB",
    "Configuration read failure",
    "Configuration file exceeds 64 KiB",
    "Configuration must be UTF-8 without control characters",
    "Invalid listener or connection limits",
    "ADCS requires listeners, a certificate and a private key",
    "Invalid log level",
    "Log file cannot target configuration or credential files",
    "Cannot resolve configuration file path",
    "Invalid configuration line ",
    "Invalid section at line ",
    "Unknown configuration section at line ",
    "Expected section and key=value at line ",
    "Duplicate configuration key at line ",
    "Invalid or unknown configuration key/value at line ",
    "Provide --config only once with a file",
    "Help/version cannot be combined with operational options",
    "Cannot open configured log file",
    "Configured log destination must be a regular file with one link",
    "Startup settings: adc_listeners=",
    "Connection id=",
    "Connection log events suppressed=",
    "ADC invalid frame rejected; payload omitted",
    "Network connection limit reached; excess peer rejected",
    "ADCS session initialization failed; peer rejected",
    "ADCS handshake failed; connection closed",
    "Network peer I/O failed; connection closed",
    "ADCS handshake timed out; connection closed",
    "Network idle connection closed",
    "Connection logging must be on or off",
    "Invalid security limits",
    "Security stopped; rejected=",
    "Security per-IP admission quota reached; peer rejected",
    "Security frame quota reached; connection closed",
    "Security partial frame timed out; connection closed",
    "Invalid database settings",
    "Cannot read private database password file",
    "Database is disabled",
    "Database initialization failed",
    "Database option setup failed",
    "Database connection failed",
    "Database character set setup failed",
    "Database ping failed",
    "Database statement failed",
    "Database result shape invalid",
    "Database migration failed",
    "Database migration lock unavailable",
    "Database migration history mismatch",
    "Database actions and configuration check are mutually exclusive",
    "Database migrations applied=",
    "Database connection OK\n",
    "Database connected",
    "Invalid local API settings",
    "Cannot read private API token file",
    "API token must contain 64 hexadecimal characters",
    "API socket requires a private owner-controlled directory",
    "API socket path already exists or is inaccessible",
    "Cannot create local API socket",
    "Cannot bind local API socket",
    "Cannot inspect local API socket",
    "Cannot listen on local API socket",
    "Local API worker failed",
    "Local API listening",
    "Local API start may only be called once",
    "API socket cannot target configuration or credential files"
    "ADC login timed out; connection closed",
    "ADC output stalled; connection closed",
}};
inline constexpr std::string_view help = R"HELP(Usage: dc24h [--config FILE] [--check-config | --check-database | --migrate-database]
             [--listen IPv4:port | [IPv6]:port | none] ...
             [--adcs-listen IPv4:port | [IPv6]:port | none] ...
             [--tls-cert CHAIN.pem --tls-key KEY.pem]
             [--max-connections N] [--idle-timeout SECONDS]
             [--log-level info|warning|error|off] [--log-file PATH|-] [--language us|pl]
             [--log-connections on|off]
             [--max-per-ip N] [--accepts-per-minute N]
             [--frames-per-second N] [--max-invalid-frames N]
             [--tls-handshake-timeout SECONDS] [--partial-frame-timeout SECONDS]
       dc24h [--language us|pl] --help | --version
Configuration order: built-in defaults, optional INI file, then CLI overrides.
Paths inside INI are relative to its directory; CLI paths use the working directory.
First explicit listener removes implicit plaintext; CLI lists replace their transport.
ADCS requires a matching PEM chain and key; TLS >= 1.2. At most 16 total listeners.
--check-config validates settings/TLS without binding ports or creating a log file.
Defaults: 127.0.0.1:1511, 1024 connections, 120 seconds idle, info to stderr, us.
Limits: connections 1..65535; idle timeout 1..86400 seconds.
Run dc24h.eu in the foreground; stop with SIGINT (Ctrl+C) or SIGTERM.
Security defaults: 128 peers/IP, 256 admissions/IP/minute, 1024 frames/s/peer,
512 invalid frames/peer, 10 seconds for TLS and each unfinished frame.
ADC guest login and public presence are implemented; chat/search/transfers remain planned.
Database settings use [database] in INI; disabled by default.
--check-database connects/pings; --migrate-database explicitly applies metadata migrations.
--check-config never connects to MariaDB or reads the password file.
Local API: INI [api], disabled by default; Unix socket and private Bearer token.
GET /api/v1/health and /api/v1/status only. No administrative mutations.
)HELP";
} // namespace dc24h::i18n::us
