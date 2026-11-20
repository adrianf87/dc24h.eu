# Build

**alpha-0.7.15 Configuration:** [database and login startup policy](DATABASE_AUTH_CONFIGURATION.md) adds shared effective timeout resolution, strict disabled-mode authentication bounds, a redacted --show-configuration preview and a bilingual read-only Configuration page. Secrets/private paths stay outside public snapshots; changes require validation and restart. CI verifies four logical CPUs per runner and derives parallelism from nproc, without a fixed test-worker cap or waiting for remote completion.

**alpha-0.7.14 WebAdmin Accounts:** [local account maintenance](WEBADMIN_ACCOUNTS.md) adds paginated checked metadata, disabled registration, activation, disable/reset and password changes through a separate non-serving executable. Stop serving hubs before writes; fresh administrator proof and revision checks protect changes. Schema v6 and startup authentication snapshots remain. CI uses four workers per command and is not awaited.

**alpha-0.7.13 Database Resilience:** [explicit recovery and independent timeouts](DATABASE_RESILIENCE.md) add one owner-thread reconnect attempt, fresh password-file validation and protection against SQL replay or live transaction replacement. Schema v6 and startup authentication snapshots remain. CI uses GitHub-hosted Ubuntu 24.04 runners; remote completion is not awaited. This entry supersedes earlier descriptions of reconnect as unavailable.

**Current increment — alpha-0.7.5:** [Registration](REGISTRATION.md) atomically creates registered/disabled accounts and separately stored, randomly salted OpenSSL scrypt password verifiers. Migration v4 adds credential storage; validated Unicode inputs, fixed KDF costs and rollback prevent partial accounts. Authentication/session grants and public API/WebAdmin registration remain planned.

**Current increment — alpha-0.7.2:** [Database Migrations](DATABASE_MIGRATIONS.md) adds an ordered compiled SQL catalog, exact checksums, read-only `--database-status`, preserved v1 upgrade and durable applying/applied history for future non-bootstrap changes. Migrations remain explicit; no application schema or automatic DDL replay.

**Increment delivered in alpha-0.7.1:** [Database Core](DATABASE_CORE.md) adds typed prepared values, bounded owning results, write metadata and thread-owned RAII transactions, with redacted errors, rollback-only failure handling and connection retirement without replay. Existing local-socket credentials/migrations and Ubuntu-only CI remain.

**Increment delivered in alpha-0.6.13:** [Transfer Configuration](TRANSFER_CONFIGURATION.md) adds an exact bounded `allowed_protocols` startup policy, shared Core CTM/RCM enforcement and read-only effective limits/protocols in the authenticated API and bilingual Connections page. Defaults preserve existing forwarding; changes require restart.

**Current increment — alpha-0.6.11:** [Transfer Statistics](TRANSFER_STATISTICS.md) adds fixed aggregate CTM/RCM outcome, queue/error and peer-report counters, the authenticated API extension and bilingual Connections monitoring.

**Current increment — alpha-0.6.10:** [Transfer Limits](TRANSFER_LIMITS.md) adds shared CTM/RCM source and aggregate target count/byte budgets, bounded private refusals and monotonic refill. Settings are validated at startup; session-lifetime cleanup and existing security checks remain.

**alpha-0.6.5 local validation (2026-11-13):** All **141/141 CTest tests** passed in each of Debug, Release and ASan/UBSan with required integration tools and default leak detection. Coverage includes protocol selection/ownership/faults, explicit known/extension routing, fragmented IPv4/IPv6 mixed ADC/ADCS peer failures, four certificate-checked real ncdc TLS peer suites, earlier plaintext and terminal-pressure ncdc, Search/UDP, MariaDB and Apache HTTPS. All 11 active ncdc scenarios also passed three consecutive ASan/UBSan runs (33 passes) with reserved ports. No compiler warnings. WebAdmin build/typecheck, **12 Node tests**, **10 Chromium tests** and real C++ API integration passed. Metadata, UTF-8, language parity/local links, changed Python syntax, example INI and whitespace checks passed. The full Debian/Ubuntu support matrix and production load remain unverified; remote CI results are reported separately by GitHub Actions.

**Current increment — alpha-0.6.5:** [Protocol Negotiation](PROTOCOL_NEGOTIATION.md) gives Transfer Core owning protocol values and explicit ADC/1.0 plaintext / ADCS/1.0 TLS intent. Pure preference/capability selection never downgrades required TLS. Live CTM/RCM preserve the selected name and extension compatibility; peer failure replies retain opaque tokens. Real ncdc tests cover certificate-checked TLS peer handshakes over both hub transports, alongside the terminal-backpressure regression.

**alpha-0.6.4 local validation (2026-11-13):** All **134/134 CTest tests** passed in each of Debug, Release and ASan/UBSan with required integration tools and default leak detection, including the common SID matrix/fault/lifetime checks, same-token parallel mixed ADC/ADCS IPv4/IPv6 pairs, real ncdc CTM/RCM peer handshakes, previous Search/UDP, MariaDB and Apache HTTPS. No compiler warnings. WebAdmin build/typecheck, **12 Node tests**, **10 Chromium tests** and real C++ API integration passed. Metadata, UTF-8, language parity/local links, changed Python syntax, example INI and whitespace checks passed. Remote CI, the full OS/client matrix and production load remain unverified.

**Increment delivered in alpha-0.6.4:** [Connection Routing](CONNECTION_ROUTING.md) consolidates D/E CTM and RCM admission and delivery into one SID router, one immutable packet type and one bounded queue path. Recipients are fixed ready-session lifetimes: D targets only, E targets then independently echoes to the source. Tokens never select recipients; existing roles, wire validation, errors and privacy remain. Mixed-pair tests use the same token without crossing audiences.

**alpha-0.6.3 local validation (2026-11-13):** All **132/132 CTest suites** passed in Debug, Release and ASan/UBSan with required integration tools and default leak detection, including RCM native/fault/queue/lifetime checks, fragmented IPv4/IPv6 ADC/ADCS, real ncdc RCM→CTM and peer handshakes in both roles, previous CTM/Search/UDP, MariaDB and Apache HTTPS. No compiler warnings. WebAdmin build/typecheck, **12 Node tests**, **10 Chromium tests** and real C++ API integration passed. Metadata, UTF-8, language parity/local links, changed Python syntax, example INI and whitespace checks passed. Remote CI, the full OS/client matrix and production load remain unverified.

**Increment delivered in alpha-0.6.3:** [ADC RCM](ADC_RCM.md) adds owning D/E reverse-request validation and live target/echo routing through Transfer Core, with per-family passive-source/active-target checks, stable lifetime/address admission and shared bounded queues. CTM/RCM share wire-field validation. Real ncdc covers RCM→CTM and peer handshakes in both roles; no pending-token registry or file-data handling is claimed.

**alpha-0.6.2 local validation (2026-11-13):** All **128/128 CTest suites** passed in Debug, Release and ASan/UBSan with required integration tools and default leak detection, including typed/native CTM faults/queues/lifetimes, fragmented IPv4/IPv6 ADC/ADCS, actual ncdc CTM-to-CSUP/CINF/CGET peer handshakes, prior Search/UDP, MariaDB and Apache HTTPS. No compiler warnings. WebAdmin build/typecheck, **12 Node tests**, **10 Chromium tests** and real C++ API integration passed. Metadata, UTF-8, language parity/local links, changed Python syntax, example INI and whitespace checks passed. Remote CI, full OS/client coverage and production load remain unverified.

**Increment delivered in alpha-0.6.2:** [ADC CTM](ADC_CTM.md) adds owning D/E CTM validation and live target/echo routing over Transfer Core, with checked source TCP activity, lifetime/endpoint rechecks, shared bounded queues and real ncdc client-handshake interoperability. Protocol/port/token/extensions retain their checked spelling and privacy. RCM and file-data handling remain later stages.

**alpha-0.6.1 local validation (2026-11-13):** All **124/124 CTest suites** passed in Debug, Release and ASan/UBSan with required integration tools and default leak detection, including native Transfer Core faults/lifetimes, live ADC/ADCS boundaries, real ncdc file-list refusal/chat recovery, existing Search/UDP interoperability, MariaDB and Apache HTTPS. The final attempt-retention/moved-plan regression also passed again in Debug and Release. No compiler warnings. WebAdmin production build/typecheck, **12 Node tests**, **10 Chromium tests** and real C++ API integration passed. Metadata, UTF-8, language parity/local links, changed Python syntax, example INI and whitespace checks passed. Remote CI, the full OS/client matrix and production load remain unverified.

**Increment delivered in alpha-0.6.1:** [Transfer Core](TRANSFER_CORE.md) adds an independent C++20 connection-coordination engine: bounded owning intents, active/passive IPv4/IPv6 roles and immutable session-bound one-attempt plans with cancellation and dispatch rechecks. Shared Users address classification preserves Search behavior. Live CTM/RCM routing and file data transfer remain future work.

**alpha-0.5.13 local validation (2026-11-13):** Debug server build and all **120/120 CTest suites** passed with required integration tools, including real ncdc ADC/ADCS/UDP, MariaDB and Apache HTTPS. WebAdmin production build/typecheck, **12 Node tests**, **10 Chromium tests** and real C++ API-to-React integration with positive search rates passed. Desktop and mobile PL views were inspected. Metadata, UTF-8, language parity/local links, changed Python syntax, shipped INI and whitespace checks passed; no compiler warnings. This increment changes frontend monitoring and release metadata, with no C++ search algorithm/API change. Remote CI, the full OS/client matrix and production load remain unverified.

**Increment delivered in alpha-0.5.13:** [WebAdmin Search Monitoring](WEBADMIN_SEARCH.md) adds browser-local query/result/refusal/queue rates, a bounded traffic chart and accessible recent-interval table. First samples, gaps, detected resets and failures start fresh baselines; pause/manual refresh and local clear preserve cumulative counters. Existing API/settings and credential isolation remain.

**alpha-0.5.12 verification (historical):** Local verification on 2026-11-13: **120/120 CTest suites passed in each of Debug, Release and ASan/UBSan**, with required integration tools and default leak detection, including real ncdc ADC/ADCS/UDP, MariaDB and Apache HTTPS. No compiler warnings. WebAdmin build/typecheck, 6 Node tests, 8 Chromium tests and real C++ API integration passed. Metadata, UTF-8, documentation language parity/local links, changed Python syntax, shipped INI and whitespace checks passed. Remote CI, the full OS/client matrix and production load remain unverified.

**Increment delivered in alpha-0.5.12:** [Search Configuration and Statistics](SEARCH_CONFIGURATION.md) exposes 19 cumulative hub SCH/RES counters, a lazy stored-token-window gauge and all 16 effective startup search settings through the authenticated read-only API and bilingual WebAdmin Search page. Queue acceptance is distinct from delivery; direct client UDP is excluded. No payload/history or configuration write endpoint is added.

**Increment delivered in alpha-0.5.11:** [Search Security](SEARCH_SECURITY.md) adds connection-lifetime query/sent-result/received-result count and byte quotas, bounded denial notices, guest/global permission policy, trusted session grants and configurable active-search limits. Admission precedes routing/token publication. Optional strict result-token policy supplements existing owner/generation checks; direct client UDP remains outside hub enforcement.

**Increment delivered in alpha-0.5.10:** [Active/Passive](ACTIVE_PASSIVE.md) projects checked I4/I6/U4/U6 into public Users and adds allocation-free per-family TCP/UDP reply-path metadata to Search Core and immutable plans. Passive DRES remains hub-routed; active URES travels directly between clients. Dynamic INF changes, endpoint ownership and real ncdc UDP sending/receiving are tested.

**Increment delivered in alpha-0.5.9:** [Result Routing](RESULT_ROUTING.md) forwards checked DRES only to its ready target over ADC/ADCS. Plans bind session lifetimes and, when TO is present, the target owner’s active search generation. Tokenless results remain supported. Shared bounded queues preserve original fields without public fanout, sender echo or administration payloads.

**Increment delivered in alpha-0.5.8:** [ADC RES](ADC_RES.md) adds owning typed DRES parsing and live ADC/ADCS validation of names, sizes, slots, opaque tokens and canonical TTH roots after session/source checks. Semantic errors are recoverable and private; valid results explicitly report pending result routing. Token correlation, result delivery and UDP remain future increments.

**Increment delivered in alpha-0.5.7:** [Feature Routing](FEATURE_ROUTING.md) compiles checked FSCH selectors once and uses current public INF SU for all-required/no-excluded matching. SUP negotiation is independent. Live capability changes, issuer membership, unknown revisions and limits are covered over ADC/ADCS and real ncdc. RES/result routing remains a later increment.

**Increment delivered in alpha-0.5.6:** [Search Routing](SEARCH_ROUTING.md) forwards checked SCH over real ADC/ADCS: B/F broadcasts and lifetime-bound D/E delivery share one immutable packet with independent bounded queues. Supplied TO is registered in Search Core; absent TO remains valid. Source/state/feature checks, one-attempt dispatch and token cleanup are enforced. RES/result routing remains a later increment.

**Increment delivered in alpha-0.5.5:** [Search Tokens](SEARCH_TOKENS.md) adds bounded session-lifetime/TO registration, local search generations, deadlines, cancellation and explicit active-search APIs in Core. Stateless preparation stays available; refused live SCH requests do not reserve tokens. Result correlation/routing and search administration remain later increments.

**Increment delivered in alpha-0.5.4:** [TTH Search](TTH_SEARCH.md) carries the full 192-bit file root from SCH into immutable Core queries/plans, descriptors and compiled filters. Hash-only and combined queries use exact equality and retain all additional criteria. Real ncdc ADC/ADCS TTH searches are tested; live request/result routing remains a later increment.

**Increment delivered in alpha-0.5.3:** [Search Filters](SEARCH_FILTERS.md) adds compiled Unicode case-insensitive name/path predicates, extension alternatives, inclusive/exact byte limits and file/directory types over checked descriptors. Filter-only queries and opaque NFC tokens now project to Search Core. Matching is allocation-free; routing/results, indexes and search administration remain later increments.

**Increment delivered in alpha-0.5.2:** [ADC SCH](ADC_SCH.md) adds owning typed SCH parsing and live ADC/ADCS reception after state/source checks. Known fields, selectors, limits and canonical TR are checked; valid requests receive an explicit routing-not-implemented response, malformed semantic requests are recoverably refused, and criterion-free requests are ignored. Routing/results remain later increments.

**Increment delivered in alpha-0.5.1:** [Search Core](SEARCH_CORE.md) adds an independent C++20 library for bounded immutable textual queries, ready-session/feature selection and lifetime/domain-bound dispatch. Live SCH/RES parsing, serialization and transport attachment remain future increments; the running server still returns an explicit unsupported response.

**Current increment — alpha-0.4.10:** [Chat Configuration](CHAT_CONFIGURATION.md) groups startup chat settings and adds decoded-text limits, independent public/private/action switches and optional/custom welcome. Defaults preserve previous behavior; recoverable policy refusals retain protocol validation and Anti-Flood.

**Increment delivered in alpha-0.4.9:** [Anti-Flood](ANTI_FLOOD.md) adds enabled per-connection B/D/E message and decoded-byte buckets, configurable in `[chat]`. Excess messages are dropped before fanout with bounded recoverable notices; existing validation and general resource limits remain.

**Increment delivered in alpha-0.4.8:** [Chat Validation](CHAT_VALIDATION.md) centralizes client text, SID and sender readiness/ownership checks in both adapters. Recipient lifetime selection and dispatch revalidation remain authoritative; system IMSG stays separate.

**Increment delivered in alpha-0.4.7:** [Hub Messages](HUB_MESSAGES.md) adds trusted, lifetime-bound system IMSG delivery and a fixed welcome after own BINF. Client text cannot originate system messages; existing queue bounds and final-LF readiness remain.

**Increment delivered in alpha-0.4.6:** [Message Flags](MESSAGE_FLAGS.md) centralizes PM/ME decoding and canonical output for the codec and both chat adapters. Absent ME, ME0 and ME1 remain distinct; PM never grants delivery authority.

**Increment delivered in alpha-0.4.5:** [Message Routing](MESSAGE_ROUTING.md) centralizes MSG scope selection and bounded queue delivery. Plans are bound to their originating Chat Core domain and live session IDs; private intent never falls back to public delivery.

**Increment delivered in alpha-0.4.4:** [Private Chat](PRIVATE_CHAT.md) adds DMSG target-only and EMSG target-plus-sender delivery, deduplicated self-messages, lifetime-bound targets and PM origin checks. Public chat remains isolated; groups/history/moderation remain separate stages.

**Increment delivered in alpha-0.4.3:** [Public Chat](PUBLIC_CHAT.md) adds a dedicated BMSG policy/dispatch adapter and one immutable shared public frame per publication. Recipient offsets and logical queue charges remain independent; source/state/private-message gates and FIFO are preserved.

**Increment delivered in alpha-0.4.2:** [Chat Core](CHAT_CORE.md) adds a separate protocol-independent message engine and live public BMSG delivery, including sender echo and /me. Private/group/feature/hub chat, moderation and history remain separate increments.

**Increment delivered in alpha-0.4.1:** [ADC MSG](ADC_MSG.md) adds a dedicated MSG codec with typed PM/ME views, checked builders, bounded serialization and live state-machine validation. Chat delivery and conversation policy remain later increments.

**Increment delivered in alpha-0.3.13:** [Statistics](STATISTICS.md) adds constant-time lifecycle counts, online/peak users and completed-login totals, atomic transport statistics, authenticated API fields and bilingual WebAdmin metrics. Historical increments below retain their delivery scope.

**Increment delivered in alpha-0.3.12:** [Limits](LIMITS.md) adds configurable session/user ceilings shared by ADC/ADCS, bounded by the connection cap. SessionManager checks capacity transactionally; LoginHub rejects a full user registry before welcome/list preparation with ISTA 211 and own IQUI. Closing reservations count until cleanup. Earlier descriptions retain their historical scope.

**Increment delivered in alpha-0.3.5:** [User Core](USER_CORE.md) adds checked UTF-8/NFC Nickname values, shared ADC/User text validation, immutable rename candidates and explicit projection from validated ClientInfo/VerifiedIdentity. Users now links the neutral dc24h_text library using the existing utf8proc package. Manager ownership/uniqueness remains unchanged. Earlier descriptions retain their historical scope.

**Increment delivered in alpha-0.3.4:** [Session Manager](SESSION_MANAGER.md) owns SID/CID/nickname indexes together with live models. Const lookups and managed lifecycle/User mutations preserve uniqueness through admission, rename and cleanup. LoginHub retains protocol and output ownership. Earlier descriptions retain their historical delivery scope.

**Increment delivered in alpha-0.3.3:** [User Core](USER_CORE.md) adds an owning public guest identity/profile to Session, with validated full-INF projection and lifecycle-guarded updates. It was developed directly from main with Session Core; main now combines it with the previously merged alpha-0.3.2 Session Manager. Earlier descriptions retain their historical scope.

**Increment delivered in alpha-0.3.2:** [Session Manager](SESSION_MANAGER.md) centrally owns all registered lifecycle models, with bounded capacity, stable lookup, phase counts and ordered snapshot pages. Protocol peers borrow models; their queues and public routing indexes remain in ADC. Earlier descriptions retain their historical scope.

**Increment delivered in alpha-0.3.1:** [Session Core](SESSION_CORE.md) provides the independent Users model and stable lifetime IDs, with live readiness/closure/timeline integration. Earlier stage descriptions below retain their original delivery scope.

**Increment delivered in alpha-0.2.10:** [Negotiation security](NEGOTIATION_SECURITY.md) adds pending admission caps, cumulative pre-readiness input budgets and shared pre-INF/Tiger attempt limits. Earlier release descriptions below retain their original scope.

**Increment delivered in alpha-0.2.9:** [STA/QUI](ADC_STATUS.md) adds semantic status/quit validation, source-checked B/D/E/F status routing, precise errors and bounded terminal ISTA/own-IQUI output. Historical descriptions below retain the behavior of their original releases.

**Integration delivered in alpha-0.2.8:** [Guest login](ADC_LOGIN.md) attaches the state/SUP/SID/INF/Tiger stack to the actual C++ TCP/TLS reactor with bounded ordered output, nickname/CID admission, public presence and complete ncdc NORMAL/reconnect tests. Historical stage descriptions below retain their original scope; statements that transport attachment/output/login were still planned describe those earlier versions. Registered-user authentication, chat, search and transfer dispatch remain planned.

For the complete integration suite, configure `-DDC24H_REQUIRE_INTEGRATION_TESTS=ON` with BUILD_TESTING enabled. This requires openssl, ncdc, MariaDB server/client tools and Apache2/mod_ssl; CI uses this mode. Missing tools fail configuration. The default OFF mode keeps existing optional-test behavior for developer machines. `project.metadata` uses Python only and checks release/documentation consistency. See [audit](AUDIT_ALPHA_0.1.md).

Requirements: Linux, CMake >=3.22, a C++20 toolchain (GCC >=11), pthreads, utf8proc 2.x development files and OpenSSL >=3 development files. Python >=3.8 and the `openssl` executable support tests. ncdc adds two real-client smoke tests when available; CI installs it. Missing ncdc is reported explicitly by CMake. Dependencies are located locally; configuration does not download them. The allowed shared libraries and the registered security tests are checked by [Dependencies](DEPENDENCIES.md).

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --parallel 2
ctest --test-dir build-release --output-on-failure
```

For a binary without tests/Python requirements:

```sh
cmake -S . -B build-runtime -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build-runtime --parallel 2
sudo cmake --install build-runtime --prefix /usr/local
```

Language catalogs are compiled into the binary. CMake generates `dc24h/version.hpp` from project version 0.4.10; `--version` prints alpha-0.4.10. Identify an exact build with that version plus `git rev-parse HEAD`. Do not edit generated files. MariaDB Connector/C development/runtime libraries are required; a running SQL server is optional. Node.js and Apache2 are not required for the C++ build; the optional frontend has separate requirements.

For one sanitizer at a time, see [Memory safety](MEMORY_SAFETY.md). `DC24H_SANITIZER` accepts `address`, `undefined` or `thread`. Leave it empty for the normal Debug and Release builds. Do not combine sanitizers in one binary. GCC or Clang must provide the selected runtime. The optional frontend build is documented in [WEBADMIN.md](WEBADMIN.md).

Installation includes the binary and `etc/dc24h/dc24h.ini.example` relative to the selected prefix; it never installs an active configuration or starts a service.

Connector/C (`libmariadb-dev`) is required. Install `mariadb-server` for the optional `database.process` test; CMake reports missing tools. Installation also includes the immutable migration SQL under `share/dc24h/migrations`.

## WebAdmin build

Use a supported Node/npm toolchain, then `npm --prefix webadmin ci` and `npm --prefix webadmin run build`. Assets are independent of default CMake builds. To install an already built frontend, configure `-DDC24H_INSTALL_WEBADMIN=ON`; rebuilding assets from the same checkout is the operator responsibility. Browser tests and Apache2 setup: [WEBADMIN.md](WEBADMIN.md).

## INF dependency — alpha-0.2.6

Install `libgcrypt20-dev` in addition to existing C++ dependencies. CMake requires gcrypt.h/libgcrypt for dc24h_adc. INF identity validation uses GCRY_MD_TIGER1 (standard Tiger/192); runtime algorithm/context unavailability fails validation. The module does not switch FIPS/global modes. This is unrelated to TLS ciphers and does not implement Tiger tree file hashing.

## Identity failure tests — alpha-0.2.7

No new dependency is added beyond Libgcrypt/OpenSSL. The isolated adc_identity_failure_tests executable needs a GNU-compatible Linux linker supporting --wrap; CMake applies these flags only to that test target. Runtime/server targets contain no fault wrappers or injected crypto interface.
