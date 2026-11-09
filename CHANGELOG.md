# Changelog

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
