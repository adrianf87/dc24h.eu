# Changelog

## alpha-0.1.9 — 2026-11-09

Local administrative API foundation, based on main after the alpha-0.1.8 merge:

- Added independent C++20 `dc24h_api`: strict bodyless HTTP/1.1 parsing, bounded JSON responses, a private Unix-domain socket and one nonblocking RAII worker. No new external dependency.
- Added authenticated read-only `GET /api/v1/health` and `/api/v1/status` with API version v1, server version/Core-running/steady uptime, independently sampled network counters and configured database presence. No request executes SQL or mutates server state.
- Requires a private owner-controlled socket directory, 0600 socket, same-effective-UID `SO_PEERCRED` and a private 64-hex-character Bearer token file. Equal-length token checks use `CRYPTO_memcmp`; tokens and request input never appear in logs or JSON errors.
- Extracted the existing bounded private credential reader into Security for both MariaDB and API, retaining Database diagnostics and password policy. Added token/socket ignore rules and configuration/log/credential collision protections.
- Added opt-in `[api]` settings (disabled by default), 1..128 active clients, 1..30-second non-sliding accept-to-response deadline, 8192-byte headers, 32 fields and 1024-byte request line. Rejects duplicate/ambiguous framing, bodies, folded headers, Transfer-Encoding/Expect/Upgrade, and already received trailing/pipelined data. Every connection serves at most one request.
- Added fixed 400/401/404/405/431 JSON errors, authentication before routing/method decisions, HEAD without a body, exact Content-Length, Date, close/no-store/nosniff headers, and no CORS/HTTP upgrade support.
- Anchors socket creation/cleanup to the checked directory fd, never overwrites existing/stale endpoints, and removes only the recorded inode. Worker stop/join precedes dependency destruction on normal signals and Network startup errors. Fatal infrastructure/provider exceptions request owner shutdown with a sanitized error.
- Added API unit/process suites for framing/authentication boundaries, deterministic malformed bytes, private file/socket policy, simultaneous clients, absolute deadlines, slot reuse, rollback, replacements, callback failure containment, same-UID policy, dry-run/disabled behavior and US/PL operator messages.
- Both real ncdc ADC/ADCS smoke tests now read actual counters through the authenticated API before shutdown; the disposable MariaDB integration verifies enabled database presence through the same API. Full ADC login is still not claimed.
- Updated version, INI example, CI title, module/build/install/roadmap documentation and matched US/PL API/operator catalogs. WebAdmin, cross-UID Apache gateway, web sessions/roles/audit, remote API/TLS and administrative writes remain future work.

Local validation: **20/20 CTest suites passed in Debug, Release and ASan/UBSan** on Debian 13 (GCC 14.2, OpenSSL 3.5.7, MariaDB 11.8.6 / Connector/C 3.4.9, ncdc 1.23.1). Staged installation, installed example/migration bytes, installed-binary curl over Unix socket with stdin authorization, relative API paths, shutdown cleanup, UTF-8/local links/language parity and Git whitespace checks passed. Remote CI and other target OS versions are not claimed as verified.

Limits: local same-UID read-only API only; health means API responsiveness, Core-running is not complete ADC readiness, and database.enabled is not a live SQL health check. No API admission-rate limiter, token reload, request audit, remote listener, roles or frontend. Trusted callbacks/synchronous logging can block their caller; SIGKILL cannot clean up a socket. Main and historical release branches are preserved.

## alpha-0.1.8 — 2026-11-09

Database foundation, based on main after alpha-0.1.7; prior branches/history preserved:

- Added independent C++20 `dc24h_database` using MariaDB Connector/C with noncopyable RAII connections/statements, utf8mb4, bounded connect/read/write timeouts and sanitized errors.
- Added opt-in `[database]` settings. Disabled by default; enabled startup connects/pings before hub listeners. SQL stays outside Core and the Network reactor. No automatic reconnect, runtime query queue, pooling or persistence consumers.
- Uses explicitly selected local Unix sockets only. Remote database TCP/TLS is deferred pending a portable strict hostname-verification adapter; ADCS remains unchanged.
- Reads the service password from a bounded private regular file owned by the effective UID, without group/other access or extra links. Rejects final symlinks, FIFOs, controls, NUL and oversized/empty input; removes one terminal LF/CRLF and wipes temporary application buffers.
- Extends log-path collision protection to database password aliases. Never logs raw connector errors, SQL, credentials or private paths. No password CLI/INI/environment option, LOCAL INFILE or multi-statement execution.
- Added prepared execution/scalar queries with bound values and bounded scalar results; statement failures release resources and permit subsequent queries.
- Added explicit `--check-database` and `--migrate-database`, mutually exclusive with dry-run. These actions do not open hub listeners or configured logs. `--check-config` never reads database passwords or connects to SQL.
- Added immutable SQL baseline for `dc24h_schema_migrations`, embedded SQL/SHA-256, cooperative schema-specific GET_LOCK, idempotent initial application and version/name/checksum verification. Migration 1 creates metadata only; no accounts or authentication schema is claimed.
- Added private-file unit tests and disposable real-MariaDB integration covering prepared values/Unicode, result bounds/recovery, migration lock contention/repetition/history tampering, startup/shutdown, INI/CLI, US/PL and diagnostics privacy. The fixture uses --no-defaults, its own data directory/socket and disabled TCP; it never touches the host database.
- Updated matched US/PL documentation/catalogs, example INI, version checks, installable migration SQL and CI dependencies. CMake reports missing integration-test tools.

Local validation: 18/18 CTest suites passed in Debug, Release and ASan/UBSan, including real ncdc ADC/ADCS and disposable MariaDB, on Debian 13 with MariaDB 11.8.6 / Connector/C 3.4.9. Staged installation, installed example/migration bytes, UTF-8 documentation/local links/language parity and Git whitespace checks passed. Remote CI and other target OS/database combinations are not claimed as verified.

Limits: synchronous startup and per-operation timeouts, no live database health monitoring, no remote SQL transport, no automatic migrations/backups/downgrade or complete schema drift audit. DDL may implicitly commit; only the current metadata baseline is retryable. User accounts, Argon2id authentication, roles/bans, application persistence, full ADC login, HTTP API and WebAdmin remain future work. Main is not modified by this branch.

## alpha-0.1.7 — 2026-11-09

Security/resource-policy milestone, based on main after alpha-0.1.6:

- Reviewed and adapted chatgpt-alpha-0.1.7 while preserving main's TLS scheduling, typed/private logging, strict INI handling and US/PL support.
- Added independent C++20 `dc24h_security` with validated per-IP active/admission limits and per-connection fixed-window frame budgets.
- Shares canonical kernel source-IP state across TCP/ADCS listeners before TLS-session allocation/watch registration; releases active slots on disconnect, timeout, rollback and stop.
- Bounded the admission map to 65536 entries and keys to 48 bytes, with periodic inactive expiry and fail-closed admission when full. Source IPs remain in memory only.
- Added six INI/CLI settings: 128 active peers/IP, 256 admissions/IP/minute, 1024 decoder results/s/connection, 512 invalid results/connection lifetime, 10-second TLS and partial-frame deadlines by default.
- Retained parser limits of 16384 frame-body bytes and 128 parameters, UTF-8/NFC validation and discard/resynchronization. `in_progress()` includes oversized discard state for timeout tracking.
- Applies non-sliding TLS/partial-frame deadlines before/after I/O and before decoder segments. A completed frame followed by a new partial frame in the same read correctly starts a new deadline.
- Stops parsing the remainder of a read after rate/invalid quota breach. Valid, invalid and keepalive results consume the live frame budget; the crossing rate result is excluded from normal ADC counters.
- Added atomic security_rejected/security_closed telemetry, rate-limited quota/deadline warnings, typed close events and startup limit summaries without source addresses or payloads.
- Added security unit/process suites covering exact clock-window boundaries, map exhaustion/pruning, active-slot reuse, shared TCP/TLS and independent-source quotas, keepalive/invalid floods, length/parameter limits, slow partial/TLS input, deadline renewal and INI/CLI validation.
- Real ncdc ADC/ADCS smoke tests now explicitly configure security limits and assert no security rejection/closure. The large TLS transport regression explicitly raises its frame budget.
- Updated matched US/PL technical documentation, operator catalogs, INI example, version and CI title. Added a regression check that validates the actual shipped INI example. No redundant root/language documentation copies were introduced.

Local validation: **16/16 CTest suites passed in Debug, Release and ASan/UBSan**, including real ncdc ADC and ADCS, on Debian 13 with GCC 14.2, CMake 3.31.6, OpenSSL 3.5.7 and ncdc 1.23.1. Documentation UTF-8/local links/language parity, staged install/example and Git whitespace checks passed. Remote CI and remaining OS targets are not claimed as verified.

Limits: fixed windows allow boundary bursts, NAT clients share a source budget, and there is no byte-bandwidth or global attempt limiter. Synchronous logging and fatal infrastructure/allocation errors retain their prior limitations. This is not DDoS protection, authentication or a security audit. Full ADC login, accounts/roles/bans, output/backpressure, MariaDB, HTTP API and WebAdmin remain future work. Main and historical stage branches are preserved.

## alpha-0.1.6 — 2026-11-09

Logging milestone, based on main after the alpha-0.1.5 merge:

- Reviewed and adapted the logging changes from chatgpt-alpha-0.1.6 while preserving main's bounded INI reads, file/path protections, TLS scheduling and US/PL support.
- Added WARN severity and `warning` configuration level; INFO/WARN/ERROR thresholds and off are available through INI/CLI.
- Added `connections=on|off` / `--log-connections on|off`, default off. Per-peer INFO events use a typed ConnectionEvent API with internal token, transport and fixed event only.
- Logs accepted/closed TCP clients, TLS readiness/failure/close, I/O failures, ordinary idle timeout, handshake timeout and shutdown without peer addresses, identifiers, nicknames or ADC payloads.
- Added startup settings summaries with listener counts and connection/idle limits, without file/key paths.
- Limited connection records to 64 per logger per one-second steady-clock window, with saturating dropped counters and WARN summaries on next-window activity or destruction.
- Added independent five-second warning budgets for invalid/truncated ADC input, connection limits, idle expiry, TLS failure, handshake deadline and peer I/O. Existing accept/watch resource cooldown remains intact.
- Bounded message bodies to 1024 bytes including truncation marker, with valid UTF-8 codepoint boundaries. Invalid encoding and Unicode controls/format/line/paragraph separators are replaced with question marks; large input is clipped before translation.
- Added logger unit and process suites for severity, privacy, injection/Unicode bounds, concurrent budgets, rollover summaries, warning-category independence, real TLS failures/timeouts and startup error files.
- Both real ncdc tests now require lifecycle events for ADC/ADCS while starting through INI and CLI overrides.
- Added matched doc/us/LOGGING.md and doc/pl/LOGGING.md, updated module/configuration/test documentation, operator catalogs, tracked INI example and CI title. Redundant language/root module copies remain absent.

Local validation: **14/14 CTest suites passed in Debug, Release and ASan/UBSan** on Debian 13, GCC 14.2, CMake 3.31.6, OpenSSL 3.5.7 and ncdc 1.23.1. Real ncdc ADC and ADCS smoke tests passed. UTF-8 documentation/link/language parity, staged install/example and Git whitespace checks passed. Remote CI and remaining target OS versions are not claimed as verified.

Limits: logs remain synchronous and best effort; a blocked sink can still delay its caller. There is no rotation, async queue, reopen or runtime reload. Rate budgets can omit connection events and are not an audit log or network/DDoS protection. Generic callers must keep secrets out of messages; sanitization is not redaction. Full ADC login, accounts, MariaDB, HTTP API and WebAdmin remain future work. Main and prior stage history are preserved.

## alpha-0.1.5 — 2026-11-09

Configuration milestone, based on main after the alpha-0.1.4 merge:

- Reviewed chatgpt-alpha-0.1.5 and adapted its configuration/logger/test sources while retaining main's TLS and US/PL fixes.
- Added independent `dc24h_configuration`: bounded UTF-8 INI, explicit `--config`, scalar/list parsing, CLI overrides and merged validation.
- Configures TCP/ADCS ports, shared connection limits, idle timeouts, certificate/key paths, language, log severity and output path.
- Preserved TLS-only defaults; CLI listener lists replace only their transport and retain other explicit listeners. Rejects duplicate equivalent IPv6 endpoints and invalid mixed/empty lists.
- Resolves INI-relative paths against the file directory and CLI paths against the working directory; no implicit config search, variable expansion or reload.
- Added `--check-config`: checks merged settings and TLS loadability/key correspondence without binding ports, starting Core or creating/opening a log.
- Reads configuration through a type-checked descriptor with a 64 KiB actual-read cap and 4096-byte line cap; rejects final symlinks, FIFOs/devices, invalid UTF-8 and disallowed controls.
- Added synchronized append-file logging, private new files, info/error/off levels and translated operator messages. Final symlinks, non-regular and multiply-linked log files are rejected; FIFO open cannot block startup.
- Rejects log paths aliasing the active INI or TLS credentials, including existing hard links and parent-directory symlinks; diagnostics never echo configuration values.
- Added a tracked INI example and install rule; local active INI files are ignored. No service or active configuration is installed automatically.
- Removed redundant root module navigation files from doc/, retaining its index and the matched complete doc/us and doc/pl trees. No extra en/ tree was imported.
- Added configuration unit/process coverage, logger append/filter/concurrency checks, actual listener-limit overrides, file safety cases and real ncdc tests through INI configuration.

Validation: **12/12 CTest suites passed in Debug, Release and ASan/UBSan** on Debian 13 (GCC 14.2, CMake 3.31.6, OpenSSL 3.5.7, ncdc 1.23.1). Both ncdc ADC and ADCS smoke tests passed. Documentation links/language parity and staged-install/example checks are recorded with the release work. Remote CI and untested target OS versions are not claimed as verified.

Limits: regular-file logging is synchronous and best effort; there is no rotation, reopen, reload, async logging or proof of log writability/port availability in check-only mode. Keep configured directories trusted. Full ADC login, accounts, MariaDB, HTTP API and WebAdmin remain future work. Historical stage branches and main are preserved.

## alpha-0.1.4 — 2026-11-09

ADCS transport, building on the existing Core, Network and ADC stages:

- Created `alpha-0.1.4` from current `main` and retained the reviewed `chatgpt-alpha-0.1.4` commits through a merge.
- Added the `dc24h_adcs` OpenSSL module, PEM chain/key loading and nonblocking per-client TLS sessions.
- Added `--adcs-listen`, `--tls-cert` and `--tls-key`; supports TLS-only or explicitly mixed TCP/TLS listeners, with a shared connection limit.
- Requires TLS >=1.2, disables compression/renegotiation and the in-memory session cache; validates loadable matching credentials before listener readiness.
- Rejects password-encrypted PEM keys without terminal prompts; ignores SIGPIPE in the executable to isolate aborted peer writes.
- Handles WANT_READ/WANT_WRITE, clears the OpenSSL error queue before I/O and reschedules buffered decrypted bytes without relying on new socket readiness.
- Sends decrypted application bytes to the existing bounded ADC decoder. Handshake lifetime is capped at min(idle timeout, 10 seconds), with separate success/incomplete counters.
- Added verified TLS 1.2/1.3 tests, legacy rejection, TLS-only/mixed listeners, timeout recovery, key errors, handshake resets, large streams and active shutdown coverage.
- Added real ncdc ADCS testing through a transparent raw relay: TLS terminates in C++, initial HSUP is parsed, and no plaintext HSUP appears on the wire.
- Added built-in `languages/us` and `languages/pl` catalogs; `--language us|pl` selects UTF-8 help and operator messages, default EN/US. Untranslated system diagnostics retain English fallback.
- Organized all technical module documentation in `doc/us` and `doc/pl`, with English root navigation; clarified planned Database/API/WebAdmin scope.
- Updated CI dependencies and ignored private TLS credential files.

Local validation: Debian 13, GCC 14.2, CMake 3.31.6, OpenSSL 3.5.7, ncdc 1.23.1. **10/10 CTest suites passed in Debug, Release and AddressSanitizer/UndefinedBehaviorSanitizer builds**, including real ncdc ADC and ADCS smoke tests. UTF-8 documentation, matching language trees, local documentation links and Git whitespace checks passed. Remote CI and the remaining target OS versions are not claimed as verified.

Scope: encrypted inbound transport and syntax parsing. Full ADC login/session state, application responses/routing, TLS close_notify on server shutdown, credential reload, authentication, MariaDB, HTTP API and React WebAdmin are not implemented. Existing stage branches and `main` are preserved.

## alpha-0.1.3 — 2026-11-09

ADC parser milestone:

- Independent `dc24h_adc` library with `parse_line` and bounded `StreamDecoder`.
- LF framing, keepalive, fragmented/coalesced input, EOF truncation and recovery after invalid/oversized frames.
- B/C/D/E/F/H/I/U headers, base32 routing identifiers and feature selectors.
- Recognition of 15 BASE commands and preservation of valid unknown extension names.
- Escape decoding and strict UTF-8/NFC validation using utf8proc.
- Local policy: 16384 body bytes, 128 parameters, rejection of raw ASCII controls/DEL and CRLF, discard-until-LF after overflow.
- Independent per-client decoders and aggregate counters without logging parameters, private IDs or password hashes.
- Syntax, Unicode, bounds, randomized corpus and TCP integration tests; isolated real ncdc initial HSUP smoke test.
- Debug/Release checks and parser AddressSanitizer/UndefinedBehaviorSanitizer validation; CI and technical documentation updates.

Syntax validation only: no session semantics, authorization, serializer, routing, responses or complete ncdc login.

## alpha-0.1.2 — 2026-11-09

Network milestone:

- Separate `dc24h_network` and `dc24h_logging` targets.
- Nonblocking IPv4/IPv6 TCP with up to 16 listeners and concurrent clients in epoll.
- RAII descriptors, close-on-exec and event tokens independent of reused fd values.
- CLI listener, connection-limit and idle-timeout options; defaults 127.0.0.1:1511, 1024 clients and 120 seconds.
- Bounded accept/read turns, excess rejection, FIN/reset slot recovery and eventfd stop wakeup.
- Peer error isolation, temporary resource-pressure recovery and partial startup rollback; aggregate network counters.
- Tests for endpoints/limits, descriptor cleanup, 48 concurrent peers, multiple ports, IPv6, resets, timeouts, fd pressure and flood shutdown.
- CI and module/configuration/testing documentation updates.

At this stage TCP data was received and discarded. ADC, TLS, authentication, MariaDB, API and WebAdmin remained planned.

## alpha-0.1.1 — 2026-11-09

Core milestone:

- CMake >=3.22, C++20 `dc24h_core` library and `dc24h` executable.
- Single-use created/running/stopping/stopped lifecycle and condition-variable stop waiting.
- Thread-safe, idempotent stop retained before startup.
- Synchronous SIGINT/SIGTERM via pthread_sigmask/sigtimedwait and a joined std::jthread, normal exit code 0.
- Best-effort synchronized UTC stderr logging and startup/signal error handling.
- CLI help/version and argument validation.
- Lifecycle, concurrent stop, signal cancellation, CLI and real process-signal tests.
- Debug/Release CI definition for Ubuntu 22.04/24.04 and technical documentation.

Network, ADC/ADCS, users, MariaDB, API, file configuration and WebAdmin were not implemented at this stage.
