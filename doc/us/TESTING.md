# Testing

**Current increment — alpha-0.2.9:** [STA/QUI](ADC_STATUS.md) adds semantic status/quit validation, source-checked B/D/E/F status routing, precise errors and bounded terminal ISTA/own-IQUI output. Historical descriptions below retain the behavior of their original releases.

**Integration delivered in alpha-0.2.8:** [Guest login](ADC_LOGIN.md) attaches the state/SUP/SID/INF/Tiger stack to the actual C++ TCP/TLS reactor with bounded ordered output, nickname/CID admission, public presence and complete ncdc NORMAL/reconnect tests. Historical stage descriptions below retain their original scope; statements that transport attachment/output/login were still planned describe those earlier versions. Registered-user authentication, chat, search and transfer dispatch remain planned.

## STA/QUI increment — alpha-0.2.9

The strict gate registers **40 CTest entries**, adding `adc.status`, `adc.status.process`, `adc.close.process`, `adc.status.ncdc` and `adcs.status.ncdc`. [STA/QUI](ADC_STATUS.md) documents semantic bounds, terminal ordering/ownership, normal status relays and the actual TCP/OpenSSL/ncdc coverage. The close fixture uses executable-local GNU-compatible `--wrap=accept4` to force blocked TCP/TLS writes and verify the absolute drain deadline without altering production sockets or host sysctl. The existing ncdc relay now permits reset/broken-pipe as per-connection closure during reconnect/quit; full protocol/UI assertions remain required.

Local result: **40/40 passed in Debug, Release and ASan/UBSan** with required integrations, MariaDB and Apache HTTPS. WebAdmin build/typecheck, 3 Node tests, 3 Chromium browser tests and actual C++ API/Vite/React integration also passed. Existing sanitizer/no-PIE and Chromium reproduction settings below apply. Metadata, UTF-8/local links/language parity, Python syntax and Git whitespace checks passed. Remaining OS/remote CI, arbitrary faults, all TLS WANT_READ dependencies and production load/security audits require separate evidence.

Focused gate: `ctest --test-dir build-debug -R 'adc.status|adcs.status|adc.close' --output-on-failure`; run the full strict suite before finishing a stage.

## Login increment — alpha-0.2.8

The strict configuration registers **35 CTest entries**. New `adc.login` and `adc.login.process` cover bounded serialization, ordered partial sends, admission/uniqueness, simultaneous user lists, updates/cancellations, privacy, rejection/cleanup, real TCP/TLS guest sessions, absolute login deadlines and slow-reader isolation on both transports. Extended `adcs.crypto` forces SSL write backpressure and checks exact decrypted bytes. `adc.ncdc` and `adcs.ncdc` now prove complete live C++ guest NORMAL and reconnect; earlier module fixtures retain their historical narrower scope. See [ADC_LOGIN.md](ADC_LOGIN.md).

Local result: **35/35 CTest suites passed in Debug, Release and ASan/UBSan**, with required integrations, MariaDB and Apache HTTPS. The invalid-TLS fixture now permits the server to reject bad records before a client half-close; corrected ADCS/logging suites passed in all configurations. WebAdmin build/typecheck, 3 Node tests, 3 Chromium browser tests and real C++ API/Vite/React integration passed. Sanitizer flags: `-fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie` and linker `-fsanitize=address,undefined -no-pie`. These are local checks; other OS/remote CI, production load/fuzz and exhaustive TLS WANT_READ/fault-injection work remain separate.

Reproduce the full suite using the existing strict CMake commands; focused scope: `ctest --test-dir build-debug -R 'adc.login|adc.ncdc|adcs.ncdc|adcs.crypto' --output-on-failure`. Chromium uses `PLAYWRIGHT_CHROMIUM_EXECUTABLE=/usr/bin/chromium` when the system browser is installed. Metadata/link/language-parity, Python syntax and Git whitespace checks also passed.

## CID/PID increment — alpha-0.2.7

The strict suite registers **33 CTest entries**, adding `adc.identity`, `adc.identity.failure` and `adc.identity.ncdc`. They cover direct Tiger vectors/aliasing, canonical CID/PID values, all nonalphabet byte substitutions at every position, all tail symbols, wrong lengths, failed-output clearing, 1000 valid/mismatched pairs, public result ownership and INF preservation. Executable-local linker wrappers inject algorithm/context/read failures and verify no commit plus recovery/cleanup. The real ncdc fixture rejects a tampered canonical CID before accepting the actual pair on TCP and Python-owned TLS, stopping at unapproved IDENTIFY. Existing actual C++ transport tests remain separate. See [ADC_IDENTITY.md](ADC_IDENTITY.md).

Local result: **33/33 CTest suites passed in Debug, Release and ASan/UBSan**, including deterministic provider faults, actual C++ ncdc ADC/ADCS, identity/INF/SID/SUP fixtures, disposable MariaDB and isolated Apache HTTPS. WebAdmin typecheck/production build, 3 Node tests, 3 Chromium browser tests and real C++ API/Vite/React integration passed. Chromium used `PLAYWRIGHT_CHROMIUM_EXECUTABLE=/usr/bin/chromium`; dependency versions are unchanged. These local results do not establish other OS/remote CI results or a side-channel audit.

Reproduce with `cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug -DDC24H_REQUIRE_INTEGRATION_TESTS=ON`, `cmake --build build-debug --parallel 4`, `ctest --test-dir build-debug --output-on-failure --parallel 3`; repeat for Release. Focused scope: `ctest --test-dir build-debug -R '^adc.identity' --output-on-failure`. Fault tests need a GNU-compatible --wrap linker; production targets do not use wrappers. Version-initialization and arbitrary allocation failures are not injected. Historical stage results below keep their original scope.

## INF increment — alpha-0.2.6

The strict suite now registers **30 CTest entries**, adding `adc.inf` and `adc.inf.ncdc`. Coverage includes Tiger empty/abc reference vectors and byte order, canonical base32/tail bits and 1000 round trips, required/duplicate/private/unknown fields, numeric/text/SU/storage boundaries, peer-IP checks/canonicalization, immutable CID and updates, public snapshots and composition/admission gates. Every two-chunk split and bytewise UTF-8 input exercise parser recovery and atomic semantic rejection. The new real ncdc fixture validates PID/CID and exports no PD over loopback TCP and Python-owned TLS, remaining in unapproved IDENTIFY. It is separate from actual C++ transport tests. See [ADC_INF.md](ADC_INF.md).

Local result: **30/30 CTest suites passed in Debug, Release and ASan/UBSan** with strict integration tools, real C++ ncdc ADC/ADCS, module ncdc fixtures, disposable MariaDB and isolated Apache HTTPS. Additional exact nickname/SU/cumulative-storage INF boundary checks and the INF ncdc fixture passed in all three configurations. WebAdmin production build/typecheck, 3 Node tests, 3 Chromium browser tests and real C++ API/Vite/React integration passed. Chromium used `PLAYWRIGHT_CHROMIUM_EXECUTABLE=/usr/bin/chromium`. Libgcrypt 1.11.0 was installed; npm dependency versions are unchanged. These local results do not establish other OS/remote CI results.

Build with libgcrypt20-dev installed: `cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug -DDC24H_REQUIRE_INTEGRATION_TESTS=ON`, `cmake --build build-debug --parallel 4`, `ctest --test-dir build-debug --output-on-failure --parallel 3`; repeat with Release. Focused checks: `ctest --test-dir build-debug -R '^adc.inf' --output-on-failure`. Sanitizer/Chromium commands below remain applicable. Algorithm-unavailable and out-of-memory branches are handled but not fault-injected by these tests. Historical results retain their original stage scope.

## SID increment — alpha-0.2.5

The complete strict suite registers **28 CTest entries**, including new `adc.sid` and `adc.sid.ncdc`. SID checks cover every 20-bit codec value, the entire client pool namespace and exhaustion, 64-bit bitmap boundaries/wrap, reuse/churn, move-only ownership and surviving allocator destruction, rejected/repeated ISID, pending-send rollback, shared sessions and terminal cleanup. The ncdc fixture uses an allocated SID and reports separate SUP/SID send completions on TCP and Python-owned TLS; it stops at unapproved IDENTIFY. Existing actual C++ Network/OpenSSL tests remain separate. See [ADC_SID.md](ADC_SID.md).

Local result: **28/28 CTest suites passed in Debug, Release and ASan/UBSan** with strict integration tools, actual C++ ADC/ADCS ncdc, SID/SUP fixtures, disposable MariaDB and isolated Apache HTTPS. WebAdmin npm ci, typecheck/production build, 3 Node tests, 3 Chromium browser tests and real C++ API/Vite/React integration passed. Browser runs used `PLAYWRIGHT_CHROMIUM_EXECUTABLE=/usr/bin/chromium`; dependency versions are unchanged. This is local verification, not proof of other OS/remote CI results.

Reproduce: `cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug -DDC24H_REQUIRE_INTEGRATION_TESTS=ON`, `cmake --build build-debug --parallel 4`, `ctest --test-dir build-debug --output-on-failure --parallel 3`; repeat for Release. New scope: `ctest --test-dir build-debug -R '^adc.sid' --output-on-failure`. The full-namespace SID suite has a 60-second timeout; ncdc fixtures use 30 seconds. Historical results below retain their named stage scope.

## SUP increment — alpha-0.2.4

The strict full suite registers **26 CTest entries**, adding `adc.sup` and `adc.sup.ncdc`. The first covers required BASE/common TIGR, deterministic independent local replies, owned support, AD/RM ordering, cumulative/policy bounds and rollback, fixed session hash, manual-message guards and composition with state sequencing. Stream fixtures test every split of an initial SUP/replay transcript. The second uses real ncdc on disposable loopback TCP and Python-owned TLS through the C++ module driver, receiving BINF while remaining in unapproved IDENTIFY. Existing actual C++ Network/OpenSSL ncdc tests remain separate; no live hub login is claimed. See [ADC_SUP.md](ADC_SUP.md).

Local result: **26/26 CTest suites passed in Debug, Release and ASan/UBSan**, with strict integration-tool detection. WebAdmin production build/typecheck, 3 Node tests, 3 Chromium browser tests and real C++ API/Vite/React integration passed. Browser runs used `PLAYWRIGHT_CHROMIUM_EXECUTABLE=/usr/bin/chromium`; dependency versions are unchanged. These are local results; other operating systems and remote CI require separate evidence.

Reproduce with `cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug -DDC24H_REQUIRE_INTEGRATION_TESTS=ON`, `cmake --build build-debug --parallel 4`, `ctest --test-dir build-debug --output-on-failure --parallel 3`; repeat for Release. Use `ctest --test-dir build-debug -R '^adc.sup' --output-on-failure` for the new tests. Existing sanitizer and Chromium commands below remain applicable. Historical results retain their original stage scope.

## Current state module — alpha-0.2.3

The complete suite now has **24 CTest entries**, including `adc.state`. All passed locally in Debug, Release and ASan/UBSan with strict integration-tool detection. State coverage includes guest/password paths, all BASE command/state/direction cases, explicit approval gates, replay/ordering rejection, spoofed source SIDs, per-instance isolation, fatal/local/other-user termination and parser-to-state pipelining/fragmentation. No live transport state enforcement or full login is claimed. Both real ncdc transports, disposable MariaDB and isolated Apache HTTPS remain covered separately. See [ADC_STATE.md](ADC_STATE.md).

Frontend typecheck/production build, 3 Node tests, 3 Chromium browser tests and the real C++ API/Vite/React integration passed. Browser runs used `PLAYWRIGHT_CHROMIUM_EXECUTABLE=/usr/bin/chromium` because the default Playwright browser cache was absent. Dependency versions are unchanged from alpha-0.2.2.

Reproduce with `cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug -DDC24H_REQUIRE_INTEGRATION_TESTS=ON`, `cmake --build build-debug --parallel 4` and `ctest --test-dir build-debug --output-on-failure --parallel 3`; repeat with Release. Sanitizer configuration used `-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie"` and `-DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined -no-pie"`. Historical results below retain their original stage scope.

## Current parser stage — alpha-0.2.2

The complete suite now has **23 CTest entries**. Local Debug, Release and ASan/UBSan passed all entries, including the new `adc.syntax`, expanded TCP/TLS syntax recovery and both real ncdc transports. `adc.syntax` exercises all 15 BASE argument layouts, required counts, named fields/cancellations/duplicates, opaque extension layouts, bounded integer/selector/parameter cases and owned results. Every split and bytewise stream comparisons verify all header fields, decoded values, layouts and errors, rather than only command counts.

Frontend build/typecheck, 3 Node tests, 3 browser tests and the real C++/Vite/Chromium integration passed with the new version. Staged installation with/without frontend assets, exact installed bytes, config/version and metadata/documentation checks passed. See the [parser contract](ADC_PROTOCOL.md), [build commands](BUILD.md) and [changelog](../../CHANGELOG.md). Full login and semantic field/state/identity checks remain unimplemented. Results below describe historical stages.

## Audit milestone — alpha-0.2.1

The complete current suite has **22 CTest entries**, including `project.metadata`. All passed locally in Debug, Release and ASan/UBSan; frontend build, 3 Node tests, 3 browser tests and the real C++ API integration also passed. CI uses `-DDC24H_REQUIRE_INTEGRATION_TESTS=ON` to reject missing openssl/ncdc/MariaDB/Apache tools. The default optional mode may register fewer tests. Metadata checks need only Python, with no Node or Git dependency. Negative configuration and metadata fixtures were checked separately.

The [audit](AUDIT_ALPHA_0.1.md) records the exact baseline SHA, observed successful historical main CI, environment, findings and reproduction commands. Older results below describe their named historical stages. Initial ncdc HSUP is still transport coverage, not full hub login. Debian 12, Ubuntu 26.04, sustained load and fuzzing remain separate acceptance work.

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

## Apache2 HTTPS — alpha-0.1.11

Local result: **21/21 CTest suites passed in Debug, Release and ASan/UBSan**, including the new `apache2.https` suite, real ncdc ADC/ADCS and disposable MariaDB. The seven Apache cases use the shipped template policy under a temporary independent ServerRoot on loopback ephemeral ports, with a disposable certificate/key and fixture assets. Coverage: verified TLS 1.2/1.3, legacy rejection, fixed HTTP redirect/path/query/Host behavior, success/error headers, HTTPS-only HSTS, private/hidden/secret/index/symlink denial, untrusted/wrong-host client rejection, missing credential syntax failure and actual startup rejection of a mismatched pair. No host site, trust store, certificate or service configuration is changed.

Apache2/mod_ssl detection is optional and absence is reported; CI installs Apache2. The frontend typecheck/build, 3 Node unit tests, 3 browser tests and real C++/Vite/Chromium integration also passed with version alpha-0.1.11. Staged installs verified the unchanged HTTPS example both with and without optional WebAdmin assets and accepted the installed INI. UTF-8/local-link/language-parity and Git whitespace checks passed. Local Apache 2.4.69 / OpenSSL 3.5.7 on Debian 13; remote CI and remaining OS versions are not claimed as verified. See [APACHE2_HTTPS.md](APACHE2_HTTPS.md).
