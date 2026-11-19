# Versions

**alpha-0.9.16 Stress testing:** [Overload and sustained operation](STRESS.md) repeats the existing frame, peer, login, chat and session limits for a fixed number of rounds. Refusals stay on. Occupancy returns to zero. Schema v9 is unchanged.
**alpha-0.9.15 Memory safety:** [AddressSanitizer, UBSan and ThreadSanitizer](MEMORY_SAFETY.md) are separate build modes. The hub behavior stays the same. Schema v9 is unchanged.
**alpha-0.9.14 Optimization:** [C++20 memory and routing](OPTIMIZATION.md) publishes each routed frame in one allocation and queues the welcome list from the encoding already measured. Protocol bytes stay the same. Schema v9 is unchanged.
**alpha-0.9.13 Profiling:** [Alpha 0.8 bottleneck profile](PROFILING.md) ranks a fixed in-process set of permission, moderation and role decisions. This is not a sampling profiler or a capacity claim. Schema v9 is unchanged.
**alpha-0.9.12 Benchmark:** [Alpha 0.8 performance measurements](BENCHMARK.md) times a fixed in-process set of permission and moderation decisions. This is not a capacity claim or a load test. Schema v9 is unchanged.
**alpha-0.9.11 Monitoring:** [CPU, memory, sessions and latency](MONITORING.md) publishes one process sample and session gauges on the existing bearer-only status route. This is not a host monitor or a stored history. Schema v9 is unchanged.
**alpha-0.9.10 Logging:** [Secure logs and event recording](LOGGING_SECURITY.md) keeps a log file private to its owner and records fixed security outcomes without peer data. This is not a production audit. Schema v9 is unchanged.
**alpha-0.9.9 System security:** [Debian and Ubuntu service hardening](SYSTEM_SECURITY.md) ships one systemd example that runs the hub as a locked system user with no new privileges and a strict filesystem. The build does not install or start that service. Schema v9 is unchanged.
**alpha-0.9.8 Database security:** [MariaDB backup and failure handling](DATABASE_SECURITY.md) writes one consistent owner-only snapshot when the operator asks, and restores it only into empty tables of the same schema. A lost acknowledgement is not replayed. Off-host copies stay with the operator. Schema v9 is unchanged.
**alpha-0.9.7 API security:** [Permissions, sessions and CSRF](API_SECURITY.md) requires the session id and the CSRF secret on every maintenance call except the password proof. A grant alone no longer reads. Copying all three values from page memory still works until the slot ends. Schema v9 is unchanged.
**alpha-0.9.6 Account security:** [Passwords, GPA/PAS and secret protection](ACCOUNT_SECURITY.md) refuses an ADC password identical to the account password and decrypts that password only after the account attempt budget and an account ban check. Online guessing, a captured PAS with require_tls off, and buckets cleared by restart remain. Schema v9 is unchanged.
**alpha-0.9.5 TLS security:** [ADCS certificate and cipher policy](TLS_SECURITY.md) checks the hub leaf before listen and limits ADCS to TLS 1.2 and 1.3 AEAD. Trust, hostname, HTTP/2, OCSP and automatic renewal stay separate. Schema v9 is unchanged.
**alpha-0.9.4 Traffic limits:** [Spam and excessive presence traffic](TRAFFIC_LIMITS.md) caps ready-session INF updates before fanout. Chat, search and transfer budgets stay. Schema v9 is unchanged. A bandwidth cap and a public gateway remain separate work.
**alpha-0.9.3 Network security:** [TCP connection protection, timeouts and buffer limits](NETWORK_SECURITY.md) caps the listen backlog, each accepted socket's kernel send and receive buffers, and the TCP user timeout. The existing application timeouts and output queues stay. Schema v9 is unchanged. A bandwidth cap and a public gateway remain separate work.
**alpha-0.9.2 ADC security:** [Parser validation, fuzzing and protocol control](ADC_SECURITY.md) checks every parsed client frame against the hub command profile before session state changes. A deterministic in-process fuzz campaign locks the parser, the stream decoder and that profile together. Schema v9 is unchanged. Coverage-guided fuzzing and a public gateway remain separate work.
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

**alpha-0.7.8:** [ADC Authentication](ADC_AUTHENTICATION.md) implements account-backed GPA/PAS with separate AES-256-GCM encrypted ADC passwords and an external private key, while preserving scrypt. Explicit migration v6 and active exact nickname/CID mappings are required; ADCS is the default. The bounded startup directory requires restart for changes/revocations. Public provisioning/reset/rotation and role grants remain future work. This entry supersedes earlier stage descriptions of authentication as planned.

**alpha-0.7.7 Password Security** starts directly from main `75c82eea4d5799460bebe93ed1b6accef175ce25`. It preserves schema v5 and existing Registration/Identity Mapping, adds protected credential checking and updates all release fixtures. The branch is not merged automatically.

**alpha-0.7.6 Identity Mapping** starts from main `d279245e3636d631c159ee8df53623c293fb36e8`. It retains alpha-0.7.1–0.7.5, adds schema v5 and [identity mapping](IDENTITY_MAPPING.md), and runs tests on the single Contabo runner. The numbered branch is not merged automatically.

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

| Version | Branch | Scope |
| --- | --- | --- |
| alpha-0.7.3 | alpha-0.7.3 | Account Model: checked persistent metadata; authentication and persistence remain planned |
| alpha-0.7.4 | alpha-0.7.4 | Account Repository: MariaDB storage, exact names and revision conflicts |
| alpha-0.7.5 | alpha-0.7.5 | Registration: atomic account/credential creation with salted scrypt |
| alpha-0.1.1 | alpha-0.1.1 | Core |
| alpha-0.1.2 | alpha-0.1.2 | Network |
| alpha-0.1.3 | alpha-0.1.3 | ADC parser |
| alpha-0.1.4 | alpha-0.1.4 | ADCS/TLS; reviewed chatgpt-alpha-0.1.4 history retained |
| alpha-0.1.5 | alpha-0.1.5 | Configuration: INI, CLI, paths and file logging |
| alpha-0.1.6 | alpha-0.1.6 | Logging: startup, connections, warnings and errors |
| alpha-0.1.7 | alpha-0.1.7 | Bounded IP admission, frame quotas and absolute deadlines |
| alpha-0.1.8 | alpha-0.1.8 | MariaDB connection and migration metadata baseline |
| alpha-0.1.9 | alpha-0.1.9 | Local read-only administrative HTTP/JSON API |
| alpha-0.1.10 | alpha-0.1.10 | React/TypeScript/Vite WebAdmin layout and local read-only adapter |
| alpha-0.1.11 | alpha-0.1.11 | Apache2 static HTTPS template and isolated TLS/access tests |
| alpha-0.2.1 | alpha-0.2.1 | Project: alpha-0.1 audit and alpha-0.2 preparation |
| alpha-0.2.2 | alpha-0.2.2 | ADC Parser: BASE arguments, named fields and bounded scalar syntax |
| alpha-0.2.3 | alpha-0.2.3 | ADC State: independent hub connection state machine |
| alpha-0.2.4 | alpha-0.2.4 | SUP: feature/hash negotiation and state integration |
| alpha-0.2.5 | alpha-0.2.5 | SID: generation, RAII reservations and session binding |
| alpha-0.2.6 | alpha-0.2.6 | INF: client information and Tiger identity validation |
| alpha-0.2.7 | alpha-0.2.7 | CID/PID: dedicated Tiger verification and failure handling |
| alpha-0.2.8 | alpha-0.2.8 | Login: guest NORMAL, bounded TCP/TLS output, public presence |
| alpha-0.2.9 | alpha-0.2.9 | STA/QUI: protocol diagnostics, routing and bounded close |
| alpha-0.2.10 | alpha-0.2.10 | Security: pending sessions, cumulative login and identity-work limits |
| alpha-0.3.1 | alpha-0.3.1 | Session Core: Users lifecycle, stable local IDs and snapshots |
| alpha-0.3.2 | alpha-0.3.2 | Session Manager |
| alpha-0.3.3 | alpha-0.3.3 | User Core |
| alpha-0.3.4 | alpha-0.3.4 | Session Manager: central identity registry |
| alpha-0.3.5 | alpha-0.3.5 | User Core: checked public values and ADC projection |
| alpha-0.3.6 | alpha-0.3.6 | Nick Manager: checked names, bounded ownership and atomic rename |
| alpha-0.3.7 | alpha-0.3.7 | User Registry: SID/CID indexes and session-owner cleanup |
| alpha-0.3.8 | alpha-0.3.8 | User Login: validated ADC identity to managed session registration |
| alpha-0.3.9 | alpha-0.3.9 | User List: initial public membership synchronization |
| alpha-0.3.10 | alpha-0.3.10 | INF Updates: validated updates, cancellation and incremental distribution |
| alpha-0.3.11 | alpha-0.3.11 | User Disconnect: QUI, lifecycle finalization and resource cleanup |
| alpha-0.3.12 | alpha-0.3.12 | Limits: connections, sessions, users and cleanup-bound capacity |
| alpha-0.3.13 | alpha-0.3.13 | Statistics: online, peak, logins, connections, API/WebAdmin |
| alpha-0.4.1 | alpha-0.4.1 | ADC MSG: parser, PM/ME, checked serialization and live validation |
| alpha-0.4.2 | alpha-0.4.2 | Chat Core: independent engine, lifecycle-bound public chat and bounded delivery |
| alpha-0.4.3 | alpha-0.4.3 | Public Chat: dedicated BMSG adapter, shared immutable frames, independent queue accounting |
| alpha-0.4.4 | alpha-0.4.4 | Private Chat: DMSG/EMSG, PM origin, lifetime-bound targets and isolated queues |
| alpha-0.4.5 | alpha-0.4.5 | Message Routing: shared MSG policy, engine domains and lifetime-safe delivery |
| alpha-0.4.6 | alpha-0.4.6 | Message Flags: shared PM/ME codec and canonical chat projection |
| alpha-0.4.7 | alpha-0.4.7 | Hub Messages: trusted IMSG, ready recipients and ordered guest welcome |
| alpha-0.4.8 | alpha-0.4.8 | Chat Validation: shared text/SID/session gate and direct adapter checks |
| alpha-0.4.9 | alpha-0.4.9 | Anti-Flood: per-connection chat count/byte buckets, INI and bounded recovery |
| alpha-0.4.10 | alpha-0.4.10 | Chat Configuration: checked startup policy, text/channel/action limits and optional/custom welcome |

Identify an individual build with its version and Git commit SHA; compiling does not modify tracked source. Each significant change updates CHANGELOG.md. Alpha denotes development status, not complete hub functionality. Tags and production deployment are not created automatically.

Historical stage branches and commit history are preserved. alpha-0.1.4 retains the reference TLS branch through a merge and includes follow-up fixes.

## Historical version — alpha-0.2.7

CMake PROJECT_VERSION and npm package/lock root versions are **0.2.7**; runtime/WebAdmin identify **alpha-0.2.7**. Metadata checks enforce consistency. The branch starts from main at `9d3443a` after the INF merge and adds [dedicated CID/PID verification](ADC_IDENTITY.md). Existing Libgcrypt/OpenSSL requirements remain. Actual Network attachment, registry/admission and complete login remain planned. Main and historical branches are preserved.


## alpha-0.2.8


CMake/npm version 0.2.8; runtime alpha-0.2.8. Branch alpha-0.2.8 starts from main b8978491fa8e146527eba818f9bc1ab0a31aa90d. No automatic main merge or release tag.


## alpha-0.2.9

CMake/npm 0.2.9; runtime/WebAdmin alpha-0.2.9. Base: main `da1521502c4a8936b3427a7d38fea6b904686b39`. [STA/QUI](ADC_STATUS.md) extends live guest sessions without a new API schema, dependencies or SQL migrations. No automatic merge/tag.


## alpha-0.2.10

CMake/npm 0.2.10, runtime/WebAdmin alpha-0.2.10. Base: main `a3ca70d65a0fcbb563147e8a44a1ce2669a0ec53`. [Security](NEGOTIATION_SECURITY.md) adds local negotiation policy without new dependencies, SQL migrations or API schema changes. No automatic main merge or tag.


## alpha-0.3.1

CMake/npm 0.3.1, runtime/WebAdmin alpha-0.3.1. Base: main `5bc79d0a12eb37d0869651eb48b57d2bf338d7fe`. [Session Core](SESSION_CORE.md) adds dc24h_users without new dependencies, migrations, security settings or API schema changes. No automatic main merge or tag.
