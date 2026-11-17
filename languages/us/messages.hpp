#pragma once
#include <array>
#include <string_view>

namespace dc24h::i18n::us {
inline constexpr std::array<std::string_view, 165> messages{{
    "Kicked from hub",
    "User kicked",
    "Kick denied",
    "Kick refused; use +kick <nickname> [reason]",

    "Invalid administrative command",
    "Administrative command refused",
    "Administrative account revision is stale",
    "Administrative account not found",
    "Administrative service unavailable",
    "Administrative write outcome unknown",

    "Owner provisioning completed",
    "Ownership transfer completed",
    "Ownership outcome unknown; inspect storage before retrying",
    "Ownership operation refused",

    "Invalid role permission configuration",
    "Invalid ADC authentication configuration",
    "Invalid ADC credential input",
    "ADC credential enrollment proof failed",
    "ADC credential already enrolled",
    "ADC credential requires an account identity mapping",
    "ADC authentication requires compatible current storage",
    "Invalid stored ADC credential or identity",
    "ADC credential cryptography failed",
    "ADC authentication failed",
    "ADC account authentication enabled",
    "ADC authentication requires database, private key and compatible listeners",
    "ADC authentication key must use a separate private file",
    "Invalid database migration catalog",
    "Database migration requires operator recovery",
    "Database migration metadata schema mismatch",
    "Database schema version=",
    "Database connection is unusable",
    "Database constraint violation",
    "Database transaction conflict",
    "Database operation limit exceeded",
    "Invalid database transaction state",
    "Database connection used from a different thread",

    "Transfer switches must be on or off",
    "Chat event=",
    "Chat log events suppressed=",
    "Invalid chat policy",
    "Invalid chat welcome message",
    "Invalid chat anti-flood limits",
    "Users stopped; online=",
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
    "Security admission or pending-login quota reached; peer rejected",
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
    "Security negotiation limit reached; connection closed",
    "Security identity verification limit reached; connection closed",
    "Session identifier space exhausted; peer rejected",
    "Invalid user/session limits",
    "User session limit reached; excess peer rejected",
}};
inline constexpr std::string_view help = R"HELP(Usage: dc24h [--config FILE] [--check-config | --check-database | --database-status | --migrate-database]
             [--listen IPv4:port | [IPv6]:port | none] ...
             [--adcs-listen IPv4:port | [IPv6]:port | none] ...
             [--tls-cert CHAIN.pem --tls-key KEY.pem]
             [--max-connections N] [--max-sessions N] [--max-users N] [--idle-timeout SECONDS]
             [--log-level info|warning|error|off] [--log-file PATH|-] [--language us|pl]
             [--log-connections on|off] [--log-chat on|off]
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
User/session ceilings: 1..65535, default 65535; effective caps also obey connections.
Run dc24h.eu in the foreground; stop with SIGINT (Ctrl+C) or SIGTERM.
Security defaults: 128 peers/IP, 256 admissions/IP/minute, 1024 frames/s/peer,
512 invalid frames/peer, 10 seconds for TLS and each unfinished frame.
ADC guest login, public presence, public and private chat are implemented.
MSG flags: PM selects the private reply SID; ME1 marks a third-person action.
The hub sends a system IMSG welcome after your own INF; clients cannot originate IMSG.
Client chat checks UTF-8/NFC text, source SID ownership and session readiness.
Anti-flood [chat]: 5 messages/s (burst 10), 32768 text bytes/s (burst 131072).
Excess B/D/E messages are dropped; the recoverable notice is limited to once per second.
Chat [chat]: max_text_bytes=16384; public_enabled=on, private_enabled=on,
actions_enabled=on, welcome_enabled=on. Switches accept only on/off.
welcome_message is literal UTF-8/NFC INI text (up to 4096 bytes); restart to apply settings.
Search [search]: checked startup policy, count/byte quotas and token-window limits.
SCH routing and hub DRES are implemented; client-direct UDP is outside hub counters.
The authenticated status API exposes aggregate search statistics and effective settings.
Restart to apply search configuration; WebAdmin Search is read-only.
Transfer Core coordinates checked DCTM target / ECTM target+sender delivery.
Only TCP-active sources may send CTM; peer protocol/port/token are preserved.
RCM routes passive requests to active TCP peers, with D target / E target+sender delivery.
One Connection Router selects CTM/RCM recipients by ready SID lifetime, never by token.
Peer protocols: ADC/1.0 means plaintext; ADCS/1.0 requires TLS between clients.
The hub preserves explicit requests and opaque failure tokens; peer TLS is independent of hub TLS.
Peers preserve protocol/token in CTM replies; the hub never supplies their listener port.
Client capabilities track TCP4/TCP6 separately; UDP and hub TLS do not grant TCP activity.
Connection tokens: 1..128 decoded NFC UTF-8 bytes; exact case, spaces and backslashes preserved.
Transfers [transfers]: ctm_enabled, rcm_enabled, guest_initiate, guest_receive (on/off, default on).
allowed_protocols: any (default), none, or up to 16 exact comma-separated names.
Restart to apply transfer settings; the API and WebAdmin expose effective startup policy.
Source initiation and target receipt permissions are checked again before dispatch.
Transfer quotas: request_rate/burst/byte_rate/byte_burst and incoming_rate/burst/byte_rate/byte_burst.
CTM/RCM share source and recipient budgets; excess is dropped with at most one private notice/s.
Peer STA errors retain explicit protocol/token; routed fatal severity does not close hub sessions.
STA uses shared bounded output; validation/queue/terminal memory faults isolate affected connections.
Pending negotiation correlation and file-data handling remain planned.
Group chat remains planned.
--show-configuration prints redacted startup settings as JSON without reading credentials or contacting SQL.
Database settings use [database] in INI; disabled by default.
--check-database connects/pings; --database-status reads version/latest/pending/dirty.
--migrate-database explicitly applies the ordered compiled SQL catalog; dirty history requires operator recovery.
Database Core provides typed bound parameters, bounded results and owner-thread RAII transactions.
SQL/result errors require rollback; lost connections are retired without reconnect or write replay.
--check-config never connects to MariaDB or reads the password file.
Local API: INI [api], disabled by default; Unix socket and private Bearer token.
GET /api/v1/health and /api/v1/status only. No administrative mutations.
)HELP";
} // namespace dc24h::i18n::us
