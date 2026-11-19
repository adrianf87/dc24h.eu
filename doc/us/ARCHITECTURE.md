# Architecture

**alpha-0.9.1 Threat model:** [Hub and WebAdmin threat analysis](THREAT_MODEL.md) records assets, actors, trust boundaries and residual risk for the current hub and local WebAdmin. It does not add a control, a schema change, a public gateway or a production audit. Remote administrator access remains separate work.
**alpha-0.8.15 Security sessions:** [Access control, CSRF, sessions and request limits](SECURITY_SESSIONS.md) binds each in-memory grant to a public session id and a separate CSRF secret. Maintenance writes require both on that same slot. Each session accepts 24 requests in 10 seconds. Reads stay grant-only. The values stay in process memory, not in SQL, cookies or web storage. A public authenticated gateway remains separate work.
**alpha-0.8.14 Apache2 HTTPS proxy:** [HTTPS reverse proxy and API protection](APACHE2_PROXY.md) forwards an allowlisted WebAdmin API to the loopback adapter. Client bearer tokens, cookies and forwarded identity are removed. The C++ Unix socket and the service token stay out of Apache. Local access remains the default.
**alpha-0.8.13 WebAdmin Management:** [Account, role and ban management](WEBADMIN_MANAGEMENT.md) shows which maintenance areas the current grant allows and links to the existing account, role and ban pages. The page does not perform those writes. Configuration writes, live hub administration and browser kick or mute stay outside this page.
**alpha-0.8.12 WebAdmin Users:** [Online users and moderation actions](WEBADMIN_USERS.md) publishes the ready-session directory and a bilingual page of kick, mute and unmute decisions. The page does not dispatch those commands, create bans or reveal CID, IP, account or profile fields. Browser kick controls remain on an authenticated Direct Connect client.
**alpha-0.8.11 WebAdmin Dashboard:** [Main panel and statistics](WEBADMIN_DASHBOARD.md) adds a fixed read-only summary of existing process counters to the authenticated status snapshot and a bilingual WebAdmin page. Account lists, ban records, audit rows, configuration writes and live hub administration stay outside this panel.
**alpha-0.8.10 Admin API:** [Proven administrative access](ADMIN_API.md) requires an active account password before maintenance reads and writes. The process token remains the transport credential. Configuration writes and live hub administration remain pending.
**alpha-0.8.9 Audit Logs:** [Successful maintenance history](AUDIT_LOGS.md) records completed account, ownership and ban maintenance in schema v9. An active account with audit.read lists that history. Live kick and mute, refusals, passwords and free-text reasons stay out. Configuration writes and hub administration remain pending.

**alpha-0.8.8 Moderation Rules:** [Shared permission and target checks](MODERATION_RULES.md) decide kick, mute, unmute and account, CID and IP bans, including revocation. Schema v8 is unchanged.

**alpha-0.8.7 Mute:** [Live public/private chat moderation](MUTE.md) implements users.mute with authenticated hierarchy, session flags, +mute/+unmute and private aggregate monitoring. Schema v8 is unchanged. This supersedes earlier mute-pending statements.

**alpha-0.8.6 Temporary Bans:** [Relative duration, automatic expiry and server-clock UI](TEMPORARY_BANS.md) extend Ban Core while preserving schema v8 and SQL migrations 1–8. Use the new duration/filter API contract described there.

**alpha-0.8.5 Ban Core:** [Persistent account/CID/IP admission blocks](BAN_CORE.md) add explicit schema v8, proven create/revoke, startup enforcement and expiry. Admin Core now has ten maintenance actions; users.ban/blocks.manage are implemented. This supersedes earlier schema-v7/ban-pending statements.

**alpha-0.8.4 Kick:** [Live authenticated kick](KICK.md) adds the private +kick chat command, strict hierarchy, terminal notices, immediate routing cleanup and aggregate counters. Maintenance actions and schema v7 remain unchanged.

**alpha-0.8.3 Admin Core:** [Central maintenance command dispatch](ADMIN_CORE.md) now serves the account API and owner CLI, with entry-point isolation, fixed outcomes, private discovery and process counters. Existing proof/transaction rules and schema v7 remain.

**alpha-0.8.2 Operators:** [Staff directory and local ownership provisioning/transfer](OPERATORS.md) supersede earlier ownership-bootstrap/transfer limitations. Schema v7 and ordinary Owner protections remain.

**alpha-0.8.1 Permissions:** [the seven-role permission model](PERMISSIONS.md) adds exact capability keys, explicit rank checks, constrained VIP delegation, Guest-aware kicks and transactional maintenance role assignment. Explicit migration v7 broadens stored account roles without changing earlier SQL. Mute, bans, audit browsing and configuration writes remain policy definitions with engines pending. Global limits still apply to Owner.

**alpha-0.7.15 Configuration:** [database and login startup policy](DATABASE_AUTH_CONFIGURATION.md) adds shared effective timeout resolution, strict disabled-mode authentication bounds, a redacted --show-configuration preview and a bilingual read-only Configuration page. Secrets/private paths stay outside public snapshots; changes require validation and restart. CI verifies four logical CPUs per runner and derives parallelism from nproc, without a fixed test-worker cap or waiting for remote completion.

**alpha-0.7.14 WebAdmin Accounts:** [local account maintenance](WEBADMIN_ACCOUNTS.md) adds paginated checked metadata, disabled registration, activation, disable/reset and password changes through a separate non-serving executable. Stop serving hubs before writes; fresh administrator proof and revision checks protect changes. Schema v6 and startup authentication snapshots remain. CI uses four workers per command and is not awaited.

**alpha-0.7.13 Database Resilience:** [explicit recovery and independent timeouts](DATABASE_RESILIENCE.md) add one owner-thread reconnect attempt, fresh password-file validation and protection against SQL replay or live transaction replacement. Schema v6 and startup authentication snapshots remain. CI uses GitHub-hosted Ubuntu 24.04 runners; remote completion is not awaited. This entry supersedes earlier descriptions of reconnect as unavailable.

**alpha-0.7.12 Anti-Bruteforce:** [shared login attempt limits](ANTI_BRUTEFORCE.md) add bounded global/IP/account token buckets, automatic refill and generic terminal refusals across ADC/ADCS and reconnects. Success does not refund attempts; all roles are limited. Counters remain in memory and do not change account data.

**alpha-0.7.11 Account Management:** [trusted password change, disable and reset](ACCOUNT_MANAGEMENT.md) now compose atomic account/credential writes with fresh password proof, revision checks and ADC credential removal. Perform these operations with the hub stopped, then explicitly reenroll/reactivate as appropriate and restart. No live reload, new SQL migration or public management endpoint.

**alpha-0.7.10 Roles:** [validated account roles](ROLES.md) now bind to ready authenticated sessions, seed Search/Transfers grants and authorize staff revocation of lower-role sessions. Global limits and explicit connection restrictions still apply. Stored role changes require restart; no new SQL migration or public administration endpoint.

**alpha-0.7.9 Auth Sessions:** [connection-bound account sessions](AUTH_SESSIONS.md) add atomic readiness binding, trusted account-filtered lookups and per-connection revocation. Closing clears authentication immediately. No new SQL migration, automatic role grants or browser sessions are added. CI uses [GitHub-hosted runners](GITHUB_CI.md) and is not awaited.

**Current increment — alpha-0.7.5:** [Registration](REGISTRATION.md) atomically creates registered/disabled accounts and separately stored, randomly salted OpenSSL scrypt password verifiers. Migration v4 adds credential storage; validated Unicode inputs, fixed KDF costs and rollback prevent partial accounts. Authentication/session grants and public API/WebAdmin registration remain planned. CI retains the pinned Proxmox Ubuntu 24.04/22.04 workflow without merging github-runner-v0.1.

**Current increment — alpha-0.7.2:** [Database Migrations](DATABASE_MIGRATIONS.md) adds an ordered compiled SQL catalog, exact checksums, read-only `--database-status`, preserved v1 upgrade and durable applying/applied history for future non-bootstrap changes. Migrations remain explicit; no application schema or automatic DDL replay.

**Increment delivered in alpha-0.7.1:** [Database Core](DATABASE_CORE.md) adds typed prepared values, bounded owning results, write metadata and thread-owned RAII transactions, with redacted errors, rollback-only failure handling and connection retirement without replay. Existing local-socket credentials/migrations and Ubuntu-only CI remain.

**Increment delivered in alpha-0.6.13:** [Transfer Configuration](TRANSFER_CONFIGURATION.md) adds an exact bounded `allowed_protocols` startup policy, shared Core CTM/RCM enforcement and read-only effective limits/protocols in the authenticated API and bilingual Connections page. Defaults preserve existing forwarding; changes require restart.

**Increment delivered in alpha-0.6.12:** [WebAdmin Transfer Monitoring](WEBADMIN_TRANSFERS.md) adds observed CTM/RCM, refusal and queue-copy rates, a bounded chart and accessible interval table on Connections. Browser-local observations reset after failures, detected restarts or long gaps; shared polling, pause/manual refresh, local clear and EN/PL support preserve cumulative statistics and credential isolation.

**Increment delivered in alpha-0.6.11:** [Transfer Statistics](TRANSFER_STATISTICS.md) adds fixed aggregate CTM/RCM outcome, queue/error and peer-report counters, the authenticated API extension and bilingual Connections monitoring. [Debian 13 CI](CI_DEBIAN13.md) adds a sequential custom-runner Debug/Release matrix and Node 24 action runtimes while retaining Node 22 for WebAdmin.

**Current increment — alpha-0.6.10:** [Transfer Limits](TRANSFER_LIMITS.md) adds shared CTM/RCM source and aggregate target count/byte budgets, bounded private refusals and monotonic refill. Settings are validated at startup; session-lifetime cleanup and existing security checks remain.

**Current increment — alpha-0.6.9:** [Error Handling](ERROR_HANDLING.md) consolidates connection diagnostics and immutable shared STA routing with fixed ready-session audiences. Status preparation, recipient queues, terminal diagnostics and completed STA validation isolate memory faults; routed peer fatal status retains hub sessions. Native, mixed ADC/ADCS and ncdc reset/error/recovery cases cover the behavior.

**Increment delivered in alpha-0.6.8:** [Connection Security](CONNECTION_SECURITY.md) adds startup CTM/RCM and guest permissions, trusted ready-lifetime grants and preparation/dispatch/echo authorization rechecks. Shared binary IP and port validation preserve managed-profile behavior. Native, mixed ADC/ADCS and real ncdc cases cover denials, spoofing and recovery.

**Increment delivered in alpha-0.6.7:** [Transfer Tokens](TRANSFER_TOKENS.md) unifies bounded opaque token validation and owning values across Core, CTM/RCM and peer errors. Token bytes, including Unicode/edge spaces/backslashes, survive immutable routing and real ncdc boundary handshakes. Independent requests may reuse tokens; no token registry or inferred correlation is added.

**Increment delivered in alpha-0.6.6:** [Client Capabilities](CLIENT_CAPABILITIES.md) unifies owning IPv4/IPv6 TCP modes and advisory peer protocols. Shared explicit CTM/RCM selection preserves wire intent; a separate trusted-originator helper chooses CTM or RCM and refuses passive/passive. Live INF changes, stale-plan rechecks and real ncdc mode transitions are covered.

**Increment delivered in alpha-0.6.5:** [Protocol Negotiation](PROTOCOL_NEGOTIATION.md) gives Transfer Core owning protocol values and explicit ADC/1.0 plaintext / ADCS/1.0 TLS intent. Pure preference/capability selection never downgrades required TLS. Live CTM/RCM preserve the selected name and extension compatibility; peer failure replies retain opaque tokens. Real ncdc tests cover certificate-checked TLS peer handshakes over both hub transports, alongside the terminal-backpressure regression.

**Increment delivered in alpha-0.6.4:** [Connection Routing](CONNECTION_ROUTING.md) consolidates D/E CTM and RCM admission and delivery into one SID router, one immutable packet type and one bounded queue path. Recipients are fixed ready-session lifetimes: D targets only, E targets then independently echoes to the source. Tokens never select recipients; existing roles, wire validation, errors and privacy remain. Mixed-pair tests use the same token without crossing audiences.

**Increment delivered in alpha-0.6.3:** [ADC RCM](ADC_RCM.md) adds owning D/E reverse-request validation and live target/echo routing through Transfer Core, with per-family passive-source/active-target checks, stable lifetime/address admission and shared bounded queues. CTM/RCM share wire-field validation. Real ncdc covers RCM→CTM and peer handshakes in both roles; no pending-token registry or file-data handling is claimed.

**Increment delivered in alpha-0.6.2:** [ADC CTM](ADC_CTM.md) adds owning D/E CTM validation and live target/echo routing over Transfer Core, with checked source TCP activity, lifetime/endpoint rechecks, shared bounded queues and real ncdc client-handshake interoperability. Protocol/port/token/extensions retain their checked spelling and privacy. RCM and file-data handling remain later stages.

**Increment delivered in alpha-0.6.1:** [Transfer Core](TRANSFER_CORE.md) adds an independent C++20 connection-coordination engine: bounded owning intents, active/passive IPv4/IPv6 roles and immutable session-bound one-attempt plans with cancellation and dispatch rechecks. Shared Users address classification preserves Search behavior. Live CTM/RCM routing and file data transfer remain future work.

**Increment delivered in alpha-0.5.13:** [WebAdmin Search Monitoring](WEBADMIN_SEARCH.md) adds browser-local query/result/refusal/queue rates, a bounded traffic chart and accessible recent-interval table. First samples, gaps, detected resets and failures start fresh baselines; pause/manual refresh and local clear preserve cumulative counters. Existing API/settings and credential isolation remain.

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

**Increment delivered in alpha-0.4.12:** [WebAdmin chat statistics](WEBADMIN_CHAT.md) adds a dedicated read-only Chat page showing all nine aggregate counters, separate message/recipient-queue sections, US/PL descriptions, refresh/error/restart behavior and safe export.

**Increment delivered in alpha-0.4.11:** [Chat Logging](CHAT_LOGGING.md) adds payload-free, opt-in rate-limited outcome logs and saturating aggregate counters for client B/D/E MSG, published through the authenticated API and US/PL WebAdmin. Queue acceptance is distinct from transport delivery.

**Increment delivered in alpha-0.4.10:** [Chat Configuration](CHAT_CONFIGURATION.md) groups startup chat settings and adds decoded-text limits, independent public/private/action switches and optional/custom welcome. Defaults preserve previous behavior; recoverable policy refusals retain protocol validation and Anti-Flood.

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

In alpha-0.2.7, dc24h_adc factors intrinsic Tiger identity verification out of INF into tiger_identity.hpp/cpp. TigerCid represents canonical public bytes; VerifiedIdentity can be produced only by successful pair verification and stores no PID. Direct decode/hash output avoids optional by-value private buffers; noncopyable local PID/derived buffers are cleansed on scope exit. INF consumes this verifier and carries its public result across ordinary updates. Provider-failure wrappers belong only to an isolated test executable. See [ADC_IDENTITY.md](ADC_IDENTITY.md).

In alpha-0.2.6, dc24h_adc adds an immutable public ClientInfo snapshot, InfValidator, Tiger/base32 helpers and ClientSession composition over SidSession. Validation builds a bounded candidate before changing state, verifies PID/CID with Libgcrypt TIGER1, binds declared IP to trusted numeric peer context and discards PD. Admission/uniqueness remain external decisions. No socket, SQL, logging or HTTP coupling is added; Network still uses only the parser. See [ADC_INF.md](ADC_INF.md).

In alpha-0.2.5, dc24h_adc adds Sid, SidPool, SidLease and SidSession. One reactor-owned hub pool spans future ADC/ADCS sessions; bounded bitmaps replace per-session random collision guesses. A move-only reservation owns each active value and SidSession releases it on terminal events/destruction. Shared ownership keeps allocator state alive without coupling it to sockets, Core or Database. Parser and state syntax use the common codec. See [ADC_SID.md](ADC_SID.md); live transport attachment remains planned.

In alpha-0.2.4, `dc24h_adc` adds `SupportNegotiation` for bounded peer/local capabilities and TIGR selection, plus `ProtocolConnection` composing support with the state machine. A valid initial HSUP supplies the protocol approval; the first completed ISUP must exactly match the generated local advertisement. Peer updates are transactional in NORMAL; the session hash is immutable. The receive-only Network remains unattached. See [ADC_SUP.md](ADC_SUP.md).

In alpha-0.2.3, `dc24h_adc` also provides a hub-side `ConnectionStateMachine` with no socket, logger, database or Core dependencies. It stores only a four-character SID and fixed handshake flags. Network still uses the parser alone; [ADC_STATE.md](ADC_STATE.md) defines the explicit integration boundary and approval contract.

In alpha-0.2.2, the independent ADC library also validates BASE argument layouts and exposes named-field/integer helpers. Network uses this stricter syntax through the existing decoder; no Core/database/frontend dependency or dispatch/output path is added. See [ADC_PROTOCOL.md](ADC_PROTOCOL.md).

The executable composes independent C++20 libraries: `dc24h_core`, `dc24h_network`, `dc24h_adc`, `dc24h_adcs`, `dc24h_logging`, `dc24h_i18n`, `dc24h_configuration`, `dc24h_security`, `dc24h_database` and `dc24h_api`. Public headers live in `include/dc24h/`; implementations live under the corresponding `src/` directory. Network depends on ADC and ADCS. The parser has no dependency on Core or sockets. OpenSSL handles remain inside ADCS.

Core has a single-use `created → running → stopping → stopped` lifecycle. Without transport, `run()` waits on a condition variable. With transport, it runs the blocking reactor on the calling thread. `request_stop()` is thread safe, idempotent and retained before startup. Calling `run()` again throws. The owner must join all callers before destruction; the logger and transport must outlive Core.

Before creating threads, main blocks SIGINT/SIGTERM with `pthread_sigmask`. A `std::jthread` uses `sigtimedwait` to request stop synchronously. Its cancellation poll is 100 ms. The mask remains blocked through process exit to protect cleanup from repeated signals. SIGPIPE is ignored by the executable so broken TLS writes become peer errors. Library embedders must arrange equivalent SIGPIPE handling.

Network owns nonblocking sockets, epoll and eventfd using RAII. All client operations run on one reactor thread; only stop flags, counters and eventfd wakeups cross threads. Client event tokens never reuse socket descriptor numbers. TLS contexts outlive sessions, and sessions are destroyed before their sockets. Partial startup failures close resources before propagating to Core, which enters `stopped`. Readiness is advertised only after every listener starts.

[Network](NETWORK.md), [ADC](ADC_PROTOCOL.md), [ADCS](ADCS_TLS.md) and [API](API.md) define module contracts. Users, Chat, Search, Transfers, Security/authentication and administrative mutation APIs remain planned. WebAdmin is a separate React/TypeScript/Vite application.

## Configuration and logging — alpha-0.1.5

`dc24h_configuration` owns Settings, bounded INI parsing, CLI composition and cross-field validation. main validates before creating the logger, signal adapter, transport and Core. Logger file-open failures abort startup with code 1; runtime writes remain best effort. See [CONFIGURATION.md](CONFIGURATION.md).

## Logging — alpha-0.1.6

Startup now reports only non-secret listener counts/limits before moving Network options into the transport. The reactor supplies fixed warning categories and typed connection events. Logger records are mutex-serialized and bounded UTF-8; there is no extra logging thread or unbounded queue. See [LOGGING.md](LOGGING.md).

## Security — alpha-0.1.7

The independent C++20 `dc24h_security` target implements validated limits and clock-driven peer/frame quotas without logging, sockets or Core dependencies. Network owns one peer limiter and each connection owns its frame/timing state. Stop and rollback release active admissions in the reactor thread; telemetry exposes counts, not addresses. This module is resource policy, not user authentication. See [SECURITY.md](SECURITY.md).

## Database — alpha-0.1.8

`dc24h_database` hides Connector/C handles behind a noncopyable RAII connection. Configuration owns its options; main optionally connects before Network startup and destroys the database after Core/Network shutdown. SQL remains outside the reactor and Core. Explicit check/migration modes do not create the logger or transport. No automatic migrations or reconnect are performed. See [DATABASE.md](DATABASE.md).

## Local API — alpha-0.1.9

`dc24h_api` owns strict bounded HTTP parsing, Bearer checks, a nonblocking Unix socket and one RAII worker. It depends on Logging/Security, not Core, Network or Database. Main supplies a thread-safe snapshot callback and an infrastructure-failure stop callback. API starts before Core run and is stopped/joined before its callback targets are destroyed, including on Network startup failure. JSON only reports typed counters/flags; the API does not run SQL or perform administrative mutations. The shared private-file reader moved into Security and preserves Database error contracts. See [API.md](API.md).

## WebAdmin — alpha-0.1.10

The frontend is independent of C++ build/runtime. Browser code validates the API schema and exposes monitoring views; its only persisted setting is language. The optional Vite development adapter holds credentials server-side, restricts requests to loopback/same-origin and forwards only two read-only routes over the same-UID Unix socket. A production build is static and contains no adapter or token. Apache can serve those assets; production authenticated gateway/session/role design remains separate. CMake optionally installs prebuilt assets with `DC24H_INSTALL_WEBADMIN=ON`, default OFF. See [WEBADMIN.md](WEBADMIN.md).

## Apache2 HTTPS — alpha-0.1.11

A separate installed configuration example serves prebuilt static WebAdmin through TLS 1.2/1.3 and redirects HTTP to a fixed canonical hostname. It retains local access and private-route denial, without proxying the Unix API or changing C++ transports. Disposable tests load the shipped policy under an independent ServerRoot; certificate provisioning and host service activation remain operator responsibilities. See [APACHE2_HTTPS.md](APACHE2_HTTPS.md).

## Threat model — alpha-0.9.1

The trust boundaries of the hub listener, the ADC session, the private Unix sockets, the loopback adapter and the local Apache examples are recorded in [Threat model](THREAT_MODEL.md). This increment does not move those boundaries.

## Security sessions — alpha-0.8.15

The maintenance access gate still holds four in-memory slots. A proof now returns a public session id and a CSRF secret with the grant. Writes check both secrets on that slot. A per-session counter limits that grant to 24 requests in 10 seconds. The serving hub's status and health routes are unchanged. See [Security sessions](SECURITY_SESSIONS.md).

## Apache2 reverse proxy — alpha-0.8.14

Another installed example keeps that TLS policy and proxies an allowlisted WebAdmin API to the loopback HTTP adapter. Client credentials and forwarded identity are removed first. The Unix API socket and the service token stay outside Apache. The hub's admission identity is unchanged. See [APACHE2_PROXY.md](APACHE2_PROXY.md).
