# alpha-0.1 audit and alpha-0.2 preparation

Audit date: 2026-11-09. Delivery: **alpha-0.2.1 Project**. Baseline: `main` at `8277bc14ea3a722fdfc72f600be3e9cb5f2d9158` (merged alpha-0.1.11).

## Conclusion and scope

The eleven alpha-0.1 stages establish separately testable C++20 transport, parser, configuration, logging, security, SQL, local API and frontend foundations. The server cannot yet complete a normal ADC client login. Readiness for a usable hub depends first on outbound transport and session semantics. Existing green transport tests do not establish ADC conformance or production readiness.

This is a source/configuration review and reproducible regression audit, including real ncdc, temporary MariaDB, isolated Apache and browser integration. It is not an independent penetration test, exhaustive dependency analysis, sustained load test or fuzz campaign. Only demonstrated small defects and verification gaps are fixed in this stage; new hub features remain planned.

## Module inventory

| Area | Verified implementation and evidence | Boundary still to implement |
| --- | --- | --- |
| Core | Lifecycle, synchronous signals, stop/wakeup and startup rollback; `src/core/`, `tests/process_tests.py` | Separate transport/session readiness; health presently means API responsiveness |
| Network | Nonblocking epoll, IPv4/IPv6, RAII ownership, bounded reads/accepts, quotas and deadlines; `src/network/tcp_server.cpp`, network/security process suites | Outbound buffers, partial writes, backpressure and slow-reader policy |
| ADC | LF framing, bounds, UTF-8/NFC, escapes, headers and unknown command representation; `src/adc/parser.cpp`, ADC suites | Serializer, SUP/hash negotiation, SID allocation, state/identity validation and dispatch |
| ADCS | Nonblocking OpenSSL receive/handshake, TLS 1.2 minimum, credentials checked; `src/adcs/tls.cpp`, ADCS and ncdc suites | Send/retry semantics, bounded close_notify, certificate reload; cipher defaults depend on installed OpenSSL policy |
| Configuration / logging | Bounded INI, CLI precedence, collision checks, sanitized bounded records and event budgets; configuration/logging suites | Reload, log rotation and asynchronous sinks; slow synchronous sinks can block their caller |
| Security / languages | Bounded IP/frame accounting, absolute deadlines, private-file policy, US/PL operator catalogs; `src/security/`, security/API suites | User authentication, permissions and bans; quotas do not establish DDoS resistance |
| Database | Explicit optional Unix-socket connection, prepared values, private password, explicit checksum migration; `src/database/database.cpp`, disposable SQL test | Baseline creates metadata only; no account/role/ban persistence, runtime query queue or verified remote SQL TLS |
| API | Same-UID private Unix socket/token, bounded strict read-only HTTP/JSON; `src/api/`, API suites | No remote API, role/session model, mutation/audit trail or live SQL readiness; counters are independently sampled |
| WebAdmin | Typed read-only EN/PL panel and local Vite adapter; `webadmin/src/`, `server/`, unit/browser/live tests | Production browser authentication, Apache gateway, roles and writes; service token must stay outside browser assets |
| Apache2 | Local-only static HTTPS template, canonical redirect and access/TLS tests; `config/apache2-webadmin-https.conf.example`, Apache suite | Operator deployment/renewal and authenticated API gateway; example does not activate a host site |
| Users / Chat / Search / Transfers | Architecture and future scope only | No implementation or completed acceptance claim |

Protocol reference: [ADC 1.0.4](https://adc.sourceforge.io/ADC.html), especially line grammar and client–hub state management. The current parser implements syntax with a stricter local control-character policy; recognizing a command does not implement its semantics. Real ncdc tests stop after initial HSUP and transport counter verification.

## Findings and disposition

P1 blocks the named next capability or wider deployment. P2 identifies a reliability/verification gap. P3 is documentation consistency. Open items below are documented limitations, not claims of newly discovered exploitable vulnerabilities.

| ID | Priority | Evidence / impact | Disposition and acceptance |
| --- | --- | --- | --- |
| A01 | P3 | `App.tsx` hardcoded the documentation branch to alpha-0.1.10; root README said no frontend existed | **Closed:** link derives from WEBADMIN_VERSION; browser regression checks US/PL destinations; README corrected |
| A02 | P2 | CMake silently omitted SQL, Apache and ncdc suites when tools were missing, including in CI | **Closed:** opt-in `DC24H_REQUIRE_INTEGRATION_TESTS=ON` fails configuration if required tools are absent; CI enables it. Default developer builds remain optional |
| A03 | P2 | Release metadata and documentation integrity were checked manually | **Closed:** `project.metadata` checks CMake/npm/lock/frontend versions, current documentation introductions, UTF-8, matching US/PL Markdown file names and local link targets without Node/Git. It does not validate translated meaning, headings or external URLs |
| A04 | P3 | Roadmap retained obsolete alpha-0.1.5 task wording and no alpha-0.2 acceptance plan | **Closed:** roadmap/version policy and this bilingual audit define the new baseline and proposed gates |
| A10 | P1 | `TcpServer::run` decodes inbound frames but emits no ADC responses; no session manager | **Open / first alpha-0.2 feature:** bounded serializer/send, then SID/SUP/INF and identity/state checks. Require complete real ncdc login over ADC and ADCS |
| A11 | P2 | Reactor scans connections for pending TLS/expiry; logger writes synchronously | **Open:** measure large-client fairness, memory and stop latency; isolate slow sinks. Add slow-reader tests with output queues before fan-out features |
| A12 | P2 | `core_running`, health and database.enabled do not imply listener/session/live SQL readiness | **Open:** define separate readiness states and failure semantics before operational monitoring; preserve existing v1 meaning |
| A13 | P1 | Vite is a local development adapter; Apache denies API routes and serves static files only | **Open / before remote administration:** design browser authentication, least-privilege gateway, sessions/roles, CSRF/origin policy and audit. Do not expose the same-UID service token to browsers |
| A14 | P1 | Migration 1 records migrations only; accounts/roles/password handling are not implemented | **Open / before registered users:** versioned application schema, password design and authorization tests. Never edit the applied baseline migration |
| A15 | P2 | Local OS coverage is Debian 13; CI matrix covers Ubuntu 22.04/24.04 | **Open:** verify Debian 12 and Ubuntu 26.04 separately; add scheduled fuzz/load/fault-injection and dependency review. Version declarations alone are not test evidence |

## Evidence and reproduction

Baseline GitHub Actions push run [42561475283](https://github.com/adrianf87/dc24h.eu/actions/runs/42561475283) completed successfully for the exact main SHA above: Ubuntu 22.04/24.04 Debug/Release plus the frontend job. This observed historical result is distinct from CI for the new branch.

Local baseline: 21/21 Release CTest suites and frontend production build / 3 Node tests passed. `npm audit --json` returned zero reported vulnerabilities across the locked production and development dependency graph on the audit date; that registry result does not cover C++/OS libraries or guarantee absence of defects. Dependency versions were retained.

Final stage checks are recorded in [CHANGELOG.md](../../CHANGELOG.md). Reproduce full local checks with installed integration tools:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DDC24H_REQUIRE_INTEGRATION_TESTS=ON
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
python3 tests/project_checks.py
npm --prefix webadmin ci
npm --prefix webadmin run build
npm --prefix webadmin test
npm --prefix webadmin run test:e2e
npm --prefix webadmin run test:integration -- ../build/dc24h
npm --prefix webadmin audit
```

Playwright needs its installed Chromium or `PLAYWRIGHT_CHROMIUM_EXECUTABLE=/usr/bin/chromium`. Use a separate Release directory and the sanitizer flags in [BUILD.md](BUILD.md). This audit added `-fno-pie` / `-no-pie` for the local sanitizer build. Negative configuration checks also disable each dependency family through CMake cache entries, confirm strict mode rejects it, and confirm optional mode reports the omitted suites. Strict mode combined with BUILD_TESTING=OFF is rejected. The new metadata check is exercised against temporary mismatched versions, missing translations and broken local links.

Environment: Debian 13, GCC 14.2, CMake 3.31.6, OpenSSL 3.5.7, MariaDB 11.8.6 / Connector/C 3.4.9, ncdc 1.23.1, Apache 2.4.69, Node 20.19.2, npm 9.2 and Chromium 156. Tests use temporary independent services and credentials. No host Apache/database configuration is activated.

## Branch handoff

`alpha-0.2.1` starts at the audited main SHA and carries this report, bounded fixes, regression guards and version 0.2.1. `alpha-0.2` is prepared at the same reviewed commit as a line marker. Neither branch merges itself into main. New numbered stage branches continue to originate from current main as requested; merge each reviewed stage into main separately before beginning its successor. Historical alpha-0.1 branches and SQL bytes are preserved.

See [ROADMAP.md](ROADMAP.md) for proposed subsequent increments and measurable acceptance gates. Their numbering/scope is a plan, not implemented functionality.
