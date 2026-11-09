# Changelog

## alpha-0.2.4 — 2026-11-09

SUP increment, based on main at `1dd0b9f1a94626789d1ac9addad991a6eae78910` after the alpha-0.2.3 ADC State merge:

- Added independent C++20 SupportPolicy/SupportNegotiation in dc24h_adc: owned separate local/peer feature sets, ordered idempotent AD/RM operations, mandatory BASE, explicit common TIGR selection and immutable Tiger-192 session-hash metadata. Unknown peer features are retained as bounded inert metadata and never guessed to be hash algorithms or echoed into hub capabilities. This stage selects the hash; it does not compute Tiger/TTH, CID or password responses.
- Added local 128-token policy/input and 128-distinct-peer-feature limits across updates, checked before insertion. Candidate copies provide unchanged-state rollback on invalid syntax/capacity/required-feature removal. NORMAL updates cannot remove BASE or the selected hash; remove-then-restore in one frame is accepted when final requirements and intermediate capacity hold. Local policy is immutable; outbound NORMAL SUP changes are explicitly unsupported.
- Added owning deterministic initial ISUP Message and bounded SUP-only LF encoding: ADBASE, selected ADTIGR, then explicitly configured local extensions. Default policy advertises only BASE/TIGR. Additional local features are caller declarations requiring actual implementations. Reply values do not alias negotiation storage. Fixed SupportError categories and suggested future fatal STA codes 245/247/240 carry no peer payload.
- Added noncopyable/nonmovable ProtocolConnection composing SUP with the existing state machine. A valid first HSUP automatically approves protocol; a failed SUP changes neither negotiation nor state. The initial completed ISUP must exactly match the generated advertisement before ISID can advance to IDENTIFY. NORMAL client updates are transactional; existing identity/password/source-SID/terminal gates remain in force. The low-level ADC State API remains available independently.
- Added adc.sup release-active coverage for required features/hash, ordered/duplicate/unknown features, hand-built guards, policy/initial/cumulative limits, rollback, reply bytes/ownership, immutable hash, state sequencing, both login continuations, source binding and every two-chunk split of a SUP/replay stream.
- Added adc.sup.ncdc: real ncdc exchanges SUP with the C++ module through disposable loopback TCP and Python-owned TLS fixtures, accepts the module reply and fixed test SID, and sends actual BINF with Tiger-sized ID/PD field shapes. The fixture remains in unapproved IDENTIFY, logs no identity payload and is not the C++ Network reactor. Existing adc.ncdc/adcs.ncdc continue to test actual C++ receive-only TCP/OpenSSL transport separately.
- Added matched US/PL ADC_SUP.md and updated state/parser, architecture, API, network, security, development, roadmap, build/version/testing guides. Updated runtime/WebAdmin/package-lock metadata and fixtures to alpha-0.2.4 without dependency changes. Existing languages/us and languages/pl catalogs are preserved; no operator strings are added.

Local validation: **26/26 CTest suites passed in Debug, Release and ASan/UBSan** with DC24H_REQUIRE_INTEGRATION_TESTS=ON, including the new SUP/ncdc exchange, actual C++ ncdc ADC/ADCS, disposable MariaDB and isolated Apache HTTPS. Sanitizers used -fno-pie / -no-pie. WebAdmin typecheck/production build, **3 Node tests, 3 Chromium browser tests and real C++ API → Vite → React integration** passed. Chromium used PLAYWRIGHT_CHROMIUM_EXECUTABLE=/usr/bin/chromium; frontend dependencies were reused with unchanged dependency versions. Metadata, UTF-8/local links/language parity and Git whitespace checks passed. Other OS targets and remote CI are not inferred from local results.

Limits: standalone SUP semantics and state composition only. The actual receive-only Network still invokes the parser alone; no live SUP reply/state enforcement, general serializer/bounded output/preflight, unique SID allocator, Tiger computation, identity/password validation, dispatcher, full hub login or binary transfer mode is delivered. The test fixture's TLS is Python-owned and its SID is a constant fixture value, not a production allocator. Main, historical branches, immutable SQL migrations and unrelated local changes are preserved; this branch is not merged automatically.

## alpha-0.2.3 — 2026-11-09

ADC State increment, based on main at `4c98e1048b02c35448638fa6d7bacd98a677c5bf` after the alpha-0.2.2 parser merge:

- Added a standalone hub-side C++20 `ConnectionStateMachine` in dc24h_adc: PROTOCOL → IDENTIFY → VERIFY/NORMAL and terminal DISCONNECTED. BASE hub connections do not enter client-to-client DATA mode. The module is single-owner, noncopyable/nonmovable and stores only the assigned four-character SID and fixed flags, without peer payloads, PID, PAS, logging, sockets or database dependencies.
- Added explicit protocol, identity and password approval gates, ordered HSUP/ISUP/ISID and BINF/GPA/PAS sequencing, replay rejection, direction/type/state checks and source-SID binding for incoming B/D/E/F messages. Parsed INF/PAS or successful STA alone never grant NORMAL. Approvals are trusted caller decisions, not feature/hash, identity or password validation implementations.
- Added terminal/idempotent disconnect, fatal peer/hub status and own-SID QUI handling, other-user/routed-status isolation and keepalives without promotion. Rejected events leave stored state unchanged. Unknown extensions and GET/GFI/SND remain unsupported by the state profile while their existing parser syntax is preserved.
- Added release-active adc.state tests: all 15 BASE commands plus unknown extensions across command/state/direction cases, guest and password paths, early/repeated transitions, pending validation gates, B/D/E/F source spoofing, per-connection isolation, terminal events and parser-to-state pipelining/bytewise Unicode fragmentation.
- Added matched US/PL ADC_STATE.md contracts and updated architecture, API, security, network, development, roadmap, build, version and test documentation. Synchronized CMake/runtime/WebAdmin/package-lock metadata and version-sensitive fixtures to alpha-0.2.3 without dependency changes. Existing languages/us and languages/pl operator catalogs remain in use; no new operator strings are introduced.

Local validation: **24/24 CTest suites passed in Debug, Release and ASan/UBSan**, with DC24H_REQUIRE_INTEGRATION_TESTS=ON, real ncdc ADC/ADCS, disposable MariaDB and isolated Apache HTTPS. Sanitizers used -fno-pie / -no-pie. WebAdmin typecheck/production build, **3 Node unit tests, 3 Chromium browser tests and real C++ API → Vite → React integration** passed. Browser checks used the installed /usr/bin/chromium through PLAYWRIGHT_CHROMIUM_EXECUTABLE after the default Playwright browser was unavailable. Dependencies were reused from the unchanged alpha-0.2.2 dependency set. Metadata, UTF-8/local links/language parity and Git whitespace checks passed. Other OS targets and remote CI are not established by local results.

Limits: the state machine is an independently tested module; the receive-only TCP/TLS reactor still uses only the parser. No live state enforcement, serializer/output queue, output preflight API, unique SID allocator, feature/hash/identity/password validation, authentication, dispatch, full ncdc login or binary transfer extension is claimed. to_client events describe completed ordered sends for future integration, never mere queue insertion. Main, prior branches, local unrelated changes and immutable migrations are preserved.

## alpha-0.2.2 — 2026-11-09

ADC Parser increment, based on main at `ca464e02e714c1e81ed4c37279496168a6c47e23` after the alpha-0.2 / alpha-0.2.1 audit merge:

- Extended parse_line/StreamDecoder from generic frame tokens to BASE argument syntax for all 15 recognized commands: required positional counts, trailing two-character named fields, SUP AD/RM feature updates, positional SID/QUI and status-code shapes, transfer namespace and bounded numeric argument syntax.
- Added owning-result `Message::positional_count`, preserving all decoded parameter values and order in the existing vector. SUP is variadic; unknown extension commands retain an opaque layout. Bare fields, unknown names, repeated names/search terms and positional text resembling fields are preserved without inferred field semantics or deduplication.
- Added allocation-free `parse_named_parameter` for borrowed name/value views and `parse_integer` for complete signed 64-bit decimal parsing with overflow rejection. Numeric domains, field/hash values, status meaning, command/type/state permissions and identity checks remain separate future consumers.
- Added a local 128-selector F-header budget before selector allocation, independent of the existing 16 KiB / 128-parameter limits. Appended precise argument error categories without renumbering previous errors. Existing sanitized warnings, invalid-frame quotas, recovery and API v1 counter shape remain in use.
- Added release-active `adc.syntax` coverage for BASE layouts, malformed arguments, bare/repeated/unknown fields, scalar/selector/routing budgets, ownership, opaque extensions and full decoded-result equality across every two-chunk split and bytewise Unicode/escape input. Extended real TCP and TLS process regressions for argument errors, recovery/counters and omitted sensitive payloads. Kept both real ncdc transport regressions.
- Rewrote matching US/PL parser contracts and updated API/development/build/version/roadmap/testing guides. The requested parser stage supersedes the earlier alpha-0.2.2 serializer proposal; output/session work remains explicitly planned. Updated runtime/WebAdmin/package-lock metadata and fixtures to alpha-0.2.2 without dependency updates.

Local validation: **23/23 CTest suites passed in Debug, Release and ASan/UBSan**, including real ncdc ADC/ADCS, isolated MariaDB, Apache HTTPS and metadata checks. Sanitizer flags included `-fno-pie` / `-no-pie`. Frontend production build/typecheck, **3 Node tests, 3 Chromium browser tests and real C++ API → Vite → React integration** passed. Staged installs with/without WebAdmin verified alpha-0.2.2, accepted INI and byte-identical configurations/migration/assets. Documentation UTF-8, local links, US/PL file parity and Git whitespace checks passed. Environment: Debian 13, GCC 14.2, OpenSSL 3.5.7, ncdc 1.23.1, MariaDB 11.8.6, Apache 2.4.69, Node 20.19.2 and Chromium 156. Other OS/remote CI results require their own evidence.

Limits: syntax parsing only. No serializer, server replies, sessions/full ncdc login, authentication, dispatch or transfer DATA mode. Known malformed BASE arguments previously accepted as generic tokens are now rejected; unknown extension parameter layouts remain accepted and opaque. Main, the alpha-0.2 audit marker, historical branches and immutable migration bytes are preserved.

## alpha-0.2.1 — 2026-11-09

Project audit of alpha-0.1 and preparation of alpha-0.2, based on main at `8277bc14ea3a722fdfc72f600be3e9cb5f2d9158`:

- Added matched US/PL audit reports with module inventory, evidence, four closed consistency/verification findings, six prioritized open capability/operational gaps and reproducible checks. Distinguished initial ncdc HSUP transport coverage from the still missing complete ADC login.
- Prepared the alpha-0.2 line marker and alpha-0.2.1 stage at the same reviewed commit. Updated roadmap with proposed serializer, bounded output, session and real-client login acceptance gates. New numbered stages continue to originate from main; neither new branch merges itself into main.
- Added default-OFF `DC24H_REQUIRE_INTEGRATION_TESTS`. CI enables it and fails configuration when openssl, ncdc, MariaDB server/client tools or Apache2/mod_ssl are missing, instead of accepting a partial suite. Rejects strict mode with BUILD_TESTING=OFF; ordinary developer builds retain optional integrations.
- Added Python-only `project.metadata` CTest: CMake/npm/package-lock/frontend release consistency, current documentation introductions, UTF-8, US/PL Markdown filename parity and local link targets. No translation/external-URL validation is claimed.
- Fixed WebAdmin documentation links pinned to alpha-0.1.10, deriving the branch from WEBADMIN_VERSION and testing US/PL links in the browser. Corrected the obsolete README frontend statement and stale roadmap/version wording.
- Updated runtime, npm/lock/frontend metadata and version-sensitive fixtures to alpha-0.2.1; retained dependency versions, API v1 meaning, SQL migration bytes and alpha-0.1 history.

Local validation: **22/22 CTest suites passed in Debug, Release and ASan/UBSan**, including real ncdc ADC/ADCS, disposable MariaDB, Apache HTTPS and new project checks. Sanitizer build used `-fno-pie` / `-no-pie`. Frontend production build/typecheck, **3 Node tests, 3 Playwright browser tests and real C++/Vite/Chromium integration** passed. Configuration negative checks covered each required tool family, optional omission and disabled testing. Metadata negative fixtures rejected mismatched package/lock/frontend/introduction versions, missing language peers, invalid UTF-8 and broken links. Staged installs with/without WebAdmin verified binary version, accepted INI and exact examples/migration/assets. Git whitespace and documentation checks passed. Full locked npm dependency audit reported zero vulnerabilities on the audit date; it does not cover C++/OS libraries.

Observed baseline remote CI: [main push run 42561475283](https://github.com/adrianf87/dc24h.eu/actions/runs/42561475283) succeeded for the exact audited SHA on Ubuntu 22.04/24.04 Debug/Release and frontend. This historical observation is separate from new-branch CI. Local environment: Debian 13, GCC 14.2, OpenSSL 3.5.7, MariaDB 11.8.6, ncdc 1.23.1, Apache 2.4.69, Node 20.19.2 and Chromium 156. Debian 12 and Ubuntu 26.04 remain unverified.

Limits: engineering regression/source audit, not an independent penetration test or production certification. Full ADC sessions, Users/Chat/Search/Transfers, application persistence and production authenticated web administration remain unimplemented. Main and historical stage branches are preserved.

## alpha-0.1.11 — 2026-11-09

Apache2 HTTPS configuration milestone, based on main after alpha-0.1.10:

- Added a standalone customizable static-WebAdmin HTTPS template with fixed-host HTTP→HTTPS 301 redirection, separate port 80/443 virtual hosts, TLS 1.2/1.3 AEAD suites, external certificate/key paths and SSL compression/session-ticket settings. Listen directives stay in distro ports.conf.
- Retained local-only static access, denied private API and hidden/credential/config/log/SQL paths, disabled indexes/symlinks, and added HTTPS-only short host-scoped HSTS plus CSP/no-sniff/referrer/frame headers on success and error responses.
- Always installs the HTTPS example alongside the INI, independent of the optional prebuilt WebAdmin asset install. No active site, production credential, CA client/renewal job, firewall/DNS or host service is configured or reloaded.
- Added an optional `apache2.https` CTest suite with seven isolated real-Apache cases. Only paths/ports change from the shipped template; TLS/access/header policy is tested unchanged under a disposable ServerRoot/certificate/cache/PID/log setup on loopback ephemeral ports, with dropped-privilege workers.
- Tests verify trusted TLS 1.2/1.3/static content, legacy rejection, canonical redirect/path/query/Host behavior, HTTPS-only HSTS/security headers, private/hidden/secret/index/symlink denial, trust/name errors, missing credential checks and real startup failure for a mismatched certificate/key. Apache -t alone is explicitly not claimed to verify correspondence.
- Added matched US/PL Apache HTTPS documentation, updated build/install/architecture/configuration/security/roadmap/version guides and CI Apache dependencies. Updated runtime/WebAdmin release metadata and version-sensitive fixtures to alpha-0.1.11 without changing the private API contract or frontend behavior.

Local validation: **21/21 CTest suites passed in Debug, Release and ASan/UBSan**, including Apache HTTPS, real ncdc ADC/ADCS and isolated MariaDB. Frontend production build/typecheck, **3 Node tests, 3 browser tests and real C++/Vite/Chromium integration** passed. Staged installation with/without WebAdmin verified exact example bytes and accepted the installed INI. UTF-8/local links/language parity and Git whitespace checks passed. Environment: Debian 13, Apache 2.4.69, OpenSSL 3.5.7, Node 20.19.2 and Chromium 156. Remote CI and remaining target OS versions are not claimed as verified.

Limits: configuration example only, local access by default, static frontend without a production authenticated gateway/web sessions/roles or mutations. Certificate issuance/renewal, DNS, deployment and actual host-service activation remain operator responsibilities. ADCS and the private Unix API remain independent. This is not a production security audit. Main and historical branches are preserved.

## alpha-0.1.10 — 2026-11-09

WebAdmin foundation, based on main after the alpha-0.1.9 merge:

- Added a separate locked React 19 / TypeScript / Vite project under webadmin/, without adding Node/npm requirements to default C++ builds.
- Added a responsive read-only panel with overview, connection, security and database views, EN/US and PL catalogs, language preference, keyboard/mobile navigation, refresh controls, bounded in-session observations and safe JSON export. Future configuration/log controls remain explicitly disabled.
- Uses actual validated API data only, with clear connecting/unconfigured/offline states, unavailable-value dashes, cleared stale data and no fabricated statistics. Database presence remains a startup flag, not a live health claim.
- Added an optional same-UID local Vite development adapter. Private socket/token-file path variables remain server-only; credentials are rejected inside the served root and never sent to browser JavaScript/storage/URLs. Only two read-only routes are forwarded, with own-token injection, loopback/host/origin/site/method/path/body checks and bounded concurrency/deadlines/responses.
- Added strict status normalization and safe-integer checks on both adapter and client, dropping unknown fields. Added private-file/file-serving protections, generic errors and production-bundle separation. Static previews intentionally disable the adapter.
- Added default-OFF DC24H_INSTALL_WEBADMIN for already built frontend assets plus an Apache2 loopback static-preview example with CSP and explicit API denial. No service/configuration is activated automatically; production authenticated gateway, web sessions, roles, writes and HTTPS deployment remain planned.
- Added frontend unit/browser tests and an isolated real C++ + Unix API + Vite + Chromium harness covering counters, token isolation, desktop/mobile PL and cleanup. Existing ncdc/MariaDB/server tests remain intact. Added a Node 22 CI job and complete matched US/PL build/configuration/security/installation/development documentation.
- Updated project/runtime version and version-sensitive tests while retaining main and all historical stage branches.

Local validation: **20/20 CTest suites passed in Debug, Release and ASan/UBSan**; frontend production build/typecheck, **3 Node unit tests, 3 Playwright browser tests and 1 real C++ integration** passed. Apache2 syntax/isolated serving/CSP/API denial and optional staged installation/example/asset equality passed. Browser and bundle checks found no service credential exposure. Runtime npm dependency audit reported no vulnerabilities at execution time. Documentation UTF-8/local links/language parity and Git whitespace checks passed. Environment: Debian 13, Node 20.19.2, npm 9.2, React 19.3.1, TypeScript 7.0.2, Vite 8.3.6, Chromium 156, Apache 2.4.69. Remote CI and remaining OS/browser targets are not claimed as verified.

Limits: monitoring scaffold only; no production web authentication/roles, authenticated Apache gateway, mutations, persistent metrics, full ADC login or SQL application persistence. The local Vite adapter trusts local machine access and is not a production service. Counter precision outside JavaScript's safe-integer range is rejected. Prebuilt-asset provenance remains the installer's responsibility; CMake does not run npm or start services. This is not a security audit.

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
