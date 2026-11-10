# Configuration — alpha-0.1.11

**Current increment — alpha-0.2.9:** [STA/QUI](ADC_STATUS.md) adds semantic status/quit validation, source-checked B/D/E/F status routing, precise errors and bounded terminal ISTA/own-IQUI output. Historical descriptions below retain the behavior of their original releases.

Settings are composed in this order: built-in defaults, one explicit `--config FILE`, then command-line overrides regardless of where `--config` appears. There is no automatic file search, environment substitution, include directive, reload or remote configuration.

```sh
cp config/dc24h.ini.example config/dc24h.ini
./build/dc24h --config config/dc24h.ini --check-config
./build/dc24h --config config/dc24h.ini --max-connections 512
```

## INI format

UTF-8, optional BOM, LF or CRLF, maximum 65536 bytes in the actual bounded read and maximum 4096 bytes per physical line (including an optional CR). The final LF is optional. The opened file must be regular; a final-component symlink, FIFO or device is rejected. The parser validates UTF-8, rejects NUL/control characters (except line endings/tab) and reports line numbers without echoing values. Section/key names are case-sensitive. Full-line `#`/`;` comments and blank lines are ignored. ASCII spaces/tabs around keys/values are trimmed; inline comments, quoting and escape expansion are not supported. Paths may contain spaces and `#`/`;` literally. Unknown keys/sections and duplicate singular keys are errors. Only listener keys are repeatable.

| Section | Key | Default | Meaning |
| --- | --- | --- | --- |
| `general` | `language` | `us` | `us` or `pl` operator messages |
| `network` | `listen` | `127.0.0.1:1511` | Numeric IPv4:port / [IPv6]:port, or `none` |
| `network` | `adcs_listen` | None | TLS endpoint or `none` |
| `network` | `max_connections` | 1024 | Shared limit 1..65535 |
| `network` | `idle_timeout` | 120 | 1..86400 seconds |
| `tls` | `certificate` | None | Full PEM chain |
| `tls` | `private_key` | None | Matching unencrypted PEM key |
| `logging` | `level` | `info` | `info`, `warning`, `error`, `off` |
| `logging` | `connections` | `off` | `on` or `off`; per-peer INFO events |
| `logging` | `file` | `stderr` | `stderr` or append-file path |

The first explicit listener removes implicit plaintext. Thus an INI containing only `adcs_listen` enables TLS only. Repeated addresses build that transport's list; `none` must be its only entry. At least one total listener is required, at most 16 across both transports. Equivalent numeric IPv6 duplicates are rejected; repeated port 0 requests independent ephemeral sockets. Wildcards must be explicit. Wildcard/specific-address bind conflicts are detected at actual startup, not by the syntax check.

INI paths resolve against the configuration file's resolved directory; CLI paths remain relative to the process working directory. No directories are created automatically. Certificate/key paths require ADCS listeners, and ADCS requires both paths. To disable TLS, remove its listener and credential settings together; disabling only a listener while retaining credentials is an error. Scalar syntax/ranges must be valid in the file even if overridden; cross-field requirements are validated after CLI overrides, so the file may omit a TLS key later supplied by CLI.

## Command line

| Option | Meaning |
| --- | --- |
| `--config FILE` | Read one explicit INI file |
| `--check-config` | Validate merged settings and load/check TLS credentials; exit without binding ports, starting Core or opening/creating a log |
| `--listen ENDPOINT/none` | Replace the plaintext list on first occurrence; append subsequent entries |
| `--adcs-listen ENDPOINT/none` | Replace the TLS list on first occurrence; append subsequent entries |
| `--tls-cert PATH`, `--tls-key PATH` | Override TLS paths |
| `--max-connections N`, `--idle-timeout SECONDS` | Override connection limits |
| `--log-level info/warning/error/off` | Set verbosity threshold |
| `--log-connections on/off` | Opt into lifecycle records at INFO |
| `--log-file PATH/-` | Append to PATH or use stderr with `-` |
| `--language us/pl` | Override file language |
| `--help`, `--version` | May combine only with language selection |

CLI listener overrides preserve the other explicitly configured transport. Only implicit default plaintext is removed when the first explicit listener is configured. Repeated CLI scalars use their last value. `--check-config` does not test port availability, log writability or directory existence: these are checked when resources open at actual startup. It does check path collisions and TLS loadability/key correspondence; it does not validate certificate trust/name/expiry for clients.

Exit codes: 0 successful check/help/version/normal stop, 2 bad CLI/file/settings or failed check, 1 runtime/startup failure (including log open or TLS errors during normal startup). Diagnostics use sanitized internal messages; unknown system text falls back to English. A CLI `--language` also selects parse-error language; errors encountered before file settings load fall back to US. Data/protocol command names and counter keys are never translated.

## Logging and paths

`info` emits INFO, WARN and ERROR; `warning` emits WARN and ERROR; `error` emits only ERROR; `off` suppresses both, but a configured file is still opened and validated. UTC lines use `YYYY-MM-DDTHH:MM:SSZ [INFO|WARN|ERROR] message`. Threads share a mutex; append mode preserves previous records. New files request mode 0600 (umask can restrict further); existing permissions are preserved. Log paths must have a regular final component with exactly one hard link; final symlinks, FIFOs and devices are rejected. Opening uses O_APPEND/O_CLOEXEC/O_NOFOLLOW/O_NONBLOCK. Configured path comparisons reject the active INI, TLS certificate or key, including existing hard-link and parent-symlink aliases.

Keep configuration/log directories trusted and service-user controlled: this is not a general defense against concurrent administrative path replacement. Regular-file writes are synchronous on the calling thread and can block on storage; runtime sink failures are best effort and never throw from logging. There is no rotation, reopen, asynchronous logging or SIGHUP reload. File growth must be managed operationally (restart/reopen support is future work). Do not store secrets or client frame contents in logs.

See [ADCS/TLS](ADCS_TLS.md), [security](SECURITY.md), and the tracked [INI example](../../config/dc24h.ini.example).

Logging now adds bounded single-line UTF-8 messages, opt-in typed connection events and independent warning budgets. See [LOGGING.md](LOGGING.md) for event names, privacy boundaries, suppression summaries and the synchronous I/O limit.

## Security — alpha-0.1.7

Defaults apply even when the security section is omitted. All values are positive integers; zero does not disable a limit. INI syntax/ranges must be valid before CLI overrides, and merged values are checked by `security::validate`.

| INI key in `[security]` | CLI override | Default | Range | Meaning |
| --- | --- | --- | --- | --- |
| `max_connections_per_ip` | `--max-per-ip` | 128 | 1..65535 | Concurrent active peers per IP |
| `max_accepts_per_minute` | `--accepts-per-minute` | 256 | 1..100000 | Admissions/IP/fixed minute |
| `max_frames_per_second` | `--frames-per-second` | 1024 | 1..100000 | Decoder results/peer/fixed second |
| `max_invalid_frames` | `--max-invalid-frames` | 512 | 1..65535 | Invalid results/connection lifetime |
| `tls_handshake_timeout` | `--tls-handshake-timeout` | 10 | 1..3600 | Absolute TLS deadline, seconds |
| `partial_frame_timeout` | `--partial-frame-timeout` | 10 | 1..3600 | Absolute unfinished-frame deadline, seconds |

All listeners share IP admission state. The frame rate is per connection, including keepalives and invalid decoder results. The invalid threshold is lifetime-based; its Nth invalid result closes the peer. TLS expiry is bounded by both its absolute configured deadline and idle timeout. Partial-frame deadlines do not slide with more data and restart only for a new frame after LF. See [SECURITY.md](SECURITY.md) for exact window, memory, counter and timeout semantics.

## Database — alpha-0.1.8

INI-only `[database]`: `enabled=off`, `socket=/run/mysqld/mysqld.sock`, `name=dc24h`, `user=dc24h`, `timeout=5`, optional `password_file` (required when enabled). Only local Unix sockets are supported. `--check-database` connects/pings; `--migrate-database` explicitly applies/verifies metadata. These actions and `--check-config` are mutually exclusive. Dry-run never reads SQL credentials or contacts MariaDB. Log destinations cannot alias the database password. See [DATABASE.md](DATABASE.md) for exact ranges, private-file rules and exit codes.

## Local API — alpha-0.1.9

INI-only `[api]` options: `enabled=off`, no default `socket`/`token_file`, `max_connections=16` (1..128), `request_timeout=2` seconds (1..30). Enabling requires both paths; relative paths use the INI directory. No token values, remote addresses or CLI overrides are supported. `--check-config` checks syntax/collisions but does not read the token, inspect private runtime directories or bind the socket; database maintenance actions never start API. Normal startup requires a private owner-controlled directory and 64-hex-character private token file. Log/socket collisions with configuration/credentials are rejected. Complete setup and behavior: [API.md](API.md).

## WebAdmin development configuration

Frontend settings are separate from INI: server-only `DC24H_API_SOCKET`/`DC24H_API_TOKEN_FILE` configure the optional same-UID local Vite adapter; both must be absolute, with the token outside WebAdmin. There is no new C++ runtime setting. The static production build contains neither adapter nor token. See [WEBADMIN.md](WEBADMIN.md).

## Apache2 HTTPS template

The new `apache2-webadmin-https.conf.example` is independent of the INI. Set the actual hostname, static asset directory and external certificate/key paths in the Apache copy. It contains no Listen directives, relying on distro ports.conf, and defaults to local static access/API denial. No native C++ runtime setting is added. See [APACHE2_HTTPS.md](APACHE2_HTTPS.md).


## alpha-0.2.8

[security] adds login_timeout (default 30 seconds) and output_timeout (default 10 seconds), both 1–3600. INI/library only; no new CLI switches. Login has an absolute accept-to-own-INF deadline, including TLS; output uses time since last send progress. Compiled queue limits: 256 KiB/client, 16 MiB/hub. See [ADC_LOGIN.md](ADC_LOGIN.md).


## alpha-0.2.9

INI `[security] close_timeout=2` accepts 1–3600 seconds. It is the absolute terminal-drain deadline, without renewal from I/O; no new CLI switch. See [ADC_STATUS.md](ADC_STATUS.md).
