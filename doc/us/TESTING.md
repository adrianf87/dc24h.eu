# Testing

Run `ctest --test-dir build --output-on-failure` and the equivalent for `build-release`. Tests use only loopback, ephemeral ports and temporary data; they never use the user's ncdc configuration or shares. Each wait/process has a timeout and cleanup.

| CTest suite | Coverage |
| --- | --- |
| `core.lifecycle` | States, single-use run, early/idempotent/concurrent stop, signal cancellation |
| `core.process` | Version, CLI/help, US/PL selection and messages, SIGINT/SIGTERM, repeated/startup signals |
| `network.unit` | Numeric endpoint parser, option limits, early stop and descriptor rollback |
| `network.process` | 48 concurrent peers, multiple listeners, IPv6, limits/reuse, resets/FIN, idle timeout, flood shutdown, descriptor pressure |
| `adc.parser` | Headers/commands, escaping, UTF-8/NFC, bounds, fragmentation, truncation, deterministic random corpus |
| `adc.process` | Real stream integration, recovery, per-client state and no sensitive logging |
| `adcs.crypto` | OpenSSL nonblocking negotiation/decryption with disposable credentials and preexisting error queue entries |
| `adcs.process` | Verified certificate/name, explicit TLS 1.2 and 1.3, TLS-only/mixed listeners, fragmentation, legacy rejection, handshake timeout, key mismatch/encryption/missing file, resets during TLS writes, simultaneous peers and large streams, active shutdown |
| `configuration.unit` | INI/CLI composition, paths, bounds and logger concurrency |
| `configuration.process` | Actual limits, TLS configuration, dry-run side effects and file safety |
| `logging.unit` | Severity, sanitization/UTF-8 boundaries, concurrency, budget rollover and PL messages |
| `logging.process` | TCP/TLS lifecycle, privacy, independent warnings, INI overrides and error sink |
| `security.unit` | Range boundaries, fake-clock windows, key/map bounds, pruning and slot reuse |
| `security.process` | Real IP/TLS quotas, frame/invalid budgets, absolute deadlines and INI/CLI |
| `adc.ncdc` | Real ncdc initial HSUP through plaintext recording relay |
| `adcs.ncdc` | Real ncdc negotiates TLS directly with C++ through transparent raw relay; one decrypted HSUP, no visible HSUP on wire |

ncdc tests run in disposable PTY sessions. The ADCS relay forwards bytes unchanged, so TLS terminates in C++, not Python. The server's idle close synchronizes the counter check. TLS socket tests trust only the generated local test certificate and verify the hostname. Tests account for missing server close_notify, which is not implemented. ncdc smoke tests validate transport/initial parsing, not full login or NORMAL state.

Local result: 16/16 CTest suites passed in Debug, Release and ASan/UBSan on Debian 13 with GCC 14.2, CMake 3.31.6, OpenSSL 3.5.7 and ncdc 1.23.1. Details are recorded in the [changelog](../../CHANGELOG.md). The CI definition targets Ubuntu 22.04/24.04 in Debug/Release; its existence does not establish a successful remote run. Debian 12 and Ubuntu 26.04 require separate verification. These tests are not a performance benchmark, long-running fuzz campaign, DDoS test or security audit. Kernel/watch allocation failures and all out-of-memory paths are not covered by fault injection.

## Configuration — alpha-0.1.5

`configuration.unit` covers defaults, merged precedence, relative paths, BOM/CRLF, UTF-8/control/size bounds, equivalent IPv6 duplicates, TLS-only defaults, incomplete file + CLI credentials, file append, level filtering and concurrent log lines. `configuration.process` covers real limits/timeout overrides, dry-run without port/log side effects, TLS file paths and checks, protected-path/hard-link/parent-symlink aliases, FIFO/symlink/directory failures, sanitized errors and Polish selection. Both real ncdc smoke tests now start the C++ hub through INI plus CLI overrides.

## Logging — alpha-0.1.6

The unit suite verifies valid bounded UTF-8 at truncation, ASCII/Unicode/ANSI control handling, severity filtering, opt-in behavior, concurrent peer budgets, one-second rollover/destructor summaries and PL translation. Process tests assert warning-only/error-only files, startup summaries, 200 invalid frames with one warning, independent limit/idle warnings, bad/stalled/healthy TLS peers, no secrets/remote addresses in event records, and config/CLI precedence. Real ncdc ADC and ADCS smoke tests now enable lifecycle logs and require accepted/closed or TLS-ready events. These are still initial-HSUP tests, not full ADC login.

## Security — alpha-0.1.7

Unit tests cover lower/upper bounds, exact one-second/minute edges without real-time waits, concurrent-slot reuse, oversized identities, 65536-entry exhaustion, inactive pruning and active-entry retention. Process tests cover independent loopback source IPs, quotas shared by TCP/TLS, closed-slot recovery, valid/invalid/keepalive floods, exact 16 KiB bodies, parameter and UTF-8 errors, partial-frame trickles, renewed deadlines after a completed frame plus new partial data in one read, oversized discard deadlines, TLS trickle timeouts/recovery and all CLI bounds/INI overrides. Both ncdc smoke tests enable security settings and assert no security rejection/close. The large TLS transport regression explicitly raises frames-per-second to 100000 to test throughput separately from the default abuse policy. These results are not a DDoS benchmark or security audit.

The actual shipped INI example is also checked through `--check-config`; staged installation verifies the installed example matches the source and is accepted by the installed binary.

## Database — alpha-0.1.8

`database.unit` checks options and private password boundaries, CRLF, permissions, symlinks, hardlinks, directory/FIFO rejection and 4096-byte limits. `database.process` initializes a disposable MariaDB data directory with `--no-defaults`, disables TCP entirely, and cleans up the daemon. Install `mariadb-server` to enable it; CMake explicitly reports missing tools. Tests never use the host database. Coverage includes real connections, Unicode/injection-like bound values, SQL NULL, exact/oversized/invalid scalar results and recovery, concurrent migration-lock contention, first/repeated migration, source SHA-256, altered/unknown migration records, enabled startup/shutdown, check-only side effects, US/PL messages, missing/incorrect/private password failures, log/secret aliases and invalid INI/action combinations. Database TLS is not implemented in this increment.

alpha-0.1.8 local validation: all 18 suites passed in Debug, Release and ASan/UBSan on Debian 13, MariaDB 11.8.6 and Connector/C 3.4.9, including both real ncdc smoke tests. Remote CI and other OS/database combinations are not claimed as verified.

## Local API — alpha-0.1.9

`api.unit` covers strict header parsing, all byte-prefix fragmentation points, complete 8192-byte boundary, duplicate/ambiguous headers, fixed token format/comparison, HEAD response shape, a deterministic malformed-byte corpus, single-use start, 0600 sockets, competing servers, preserving replacement inodes and fatal callback containment. `api.process` covers live JSON/status counters, auth-before-routing, 401/404/405/400/431, security headers, fragments, duplicate/framing/body/pipeline rejection, simultaneous clients, slot recovery, non-sliding deadlines, active shutdown/restart, private/malformed token files, stale/symlink/regular socket safety, collisions, dry-run, disabled mode, US/PL, Network-failure rollback and same-UID enforcement after relaxed filesystem permissions (cross-UID case requires root and otherwise reports skipped).

Both real ncdc regressions now enable local API and read actual ADC/ADCS counters before shutdown, while still testing initial HSUP rather than complete ADC login. The disposable MariaDB process test verifies enabled database presence through API and socket cleanup. API tests need no browser, curl or additional service. CI retains Ubuntu 22.04/24.04 Debug/Release jobs; remote results and other OS targets are not implied by local tests.

alpha-0.1.9 local result: **20/20 CTest suites passed in Debug, Release and ASan/UBSan**, including real ncdc ADC/ADCS, disposable MariaDB and cross-UID rejection, on Debian 13 / GCC 14.2 / OpenSSL 3.5.7 / MariaDB 11.8.6 / Connector/C 3.4.9. Staged install, exact installed example/migration bytes, installed-binary curl via Unix socket with token on stdin, relative API paths, shutdown cleanup, documentation UTF-8/local links/language parity and Git whitespace checks passed. Remote CI and other OS targets remain unverified here.

## WebAdmin — alpha-0.1.10

Local verification: 20/20 CTest suites passed in Debug, Release and ASan/UBSan, including ncdc ADC/ADCS and isolated MariaDB. Frontend: TypeScript check and Vite production build, 3 Node unit tests, 3 Playwright browser tests, and a real C++ → Unix API → Vite adapter → Chromium integration passed. Tests verify EN/PL persistence, desktop/mobile layout, live/absent/malformed/lost data, export/navigation, safe numeric/schema normalization, private-file/root isolation, injected server-only tokens, host/origin/site restrictions, overload and size/time limits. No token was exposed through browser requests/HTML/JSON or Vite file serving.

The Apache2 sample passed syntax validation and isolated loopback serving with CSP and API denial. Optional CMake frontend installation matched source build assets/example/INI byte-for-byte, and the installed binary accepted its example. Runtime npm dependency audit reported no vulnerabilities at execution time; this is not a security audit. Local environment: Debian 13, Node 20.19.2, npm 9.2, React 19.3.1, TypeScript 7.0.2, Vite 8.3.6, Chromium 156, Apache 2.4.69. CI now defines a separate Node 22 frontend build/test job, but remote runs and other OS/browser combinations are not claimed as verified. See [WEBADMIN.md](WEBADMIN.md).
