# Security — alpha-0.1.7

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

**alpha-0.7.7:** see [Password Security](PASSWORD_SECURITY.md) for strict v1 verifier decoding, fixed scrypt work, constant-time key comparison, sensitive-buffer erasure and a trusted SELECT-only service. The complete operation is not claimed constant-time; caller authorization, concurrent KDF limits and public session admission remain separate.

**Current increment — alpha-0.7.5:** [Registration](REGISTRATION.md) atomically creates registered/disabled accounts and separately stored, randomly salted OpenSSL scrypt password verifiers. Migration v4 adds credential storage; validated Unicode inputs, fixed KDF costs and rollback prevent partial accounts. Authentication/session grants and public API/WebAdmin registration remain planned. CI retains the pinned Proxmox Ubuntu 24.04/22.04 workflow without merging github-runner-v0.1.

**Current increment — alpha-0.7.2:** [Database Migrations](DATABASE_MIGRATIONS.md) adds an ordered compiled SQL catalog, exact checksums, read-only `--database-status`, preserved v1 upgrade and durable applying/applied history for future non-bootstrap changes. Migrations remain explicit; no application schema or automatic DDL replay.

**Increment delivered in alpha-0.7.1:** [Database Core](DATABASE_CORE.md) adds typed prepared values, bounded owning results, write metadata and thread-owned RAII transactions, with redacted errors, rollback-only failure handling and connection retirement without replay. Existing local-socket credentials/migrations and Ubuntu-only CI remain.

**Increment delivered in alpha-0.6.13:** [Transfer Configuration](TRANSFER_CONFIGURATION.md) adds an exact bounded `allowed_protocols` startup policy, shared Core CTM/RCM enforcement and read-only effective limits/protocols in the authenticated API and bilingual Connections page. Defaults preserve existing forwarding; changes require restart.

**Current increment — alpha-0.6.11:** [Transfer Statistics](TRANSFER_STATISTICS.md) adds fixed aggregate CTM/RCM outcome, queue/error and peer-report counters, the authenticated API extension and bilingual Connections monitoring. [Debian 13 CI](CI_DEBIAN13.md) adds a sequential custom-runner Debug/Release matrix and Node 24 action runtimes while retaining Node 22 for WebAdmin.

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

This stage adds local resource controls to the existing TCP/ADCS transport and ADC syntax parser. The executable still has no authenticated users, session identity, role checks or complete ADC login. Run as an unprivileged user; loopback remains the default listener and public/wildcard addresses require explicit configuration.

## Implemented limits

| Setting | Default | Allowed range |
| --- | --- | --- |
| Active connections per canonical source IP | 128 | 1..65535 |
| Admissions per IP per fixed minute window | 256 | 1..100000 |
| Decoder frame results per connection per fixed second window | 1024 | 1..100000 |
| Invalid frame results per connection lifetime | 512 | 1..65535 |
| Absolute TLS handshake deadline | 10 seconds | 1..3600 |
| Absolute unfinished ADC frame deadline | 10 seconds | 1..3600 |

The global connection limit (default 1024), idle timeout (default 120 seconds), fixed 16384-byte frame body and 128-parameter parser limits remain in force. These are dc24h resource policies, not limits imposed by [ADC 1.0.4](https://dc-protocols.github.io/ADC.html). Small numbers of malformed frames are discarded and parsing can resynchronize; repeated invalid input can close the peer under the configured lifetime threshold.

Configure the six settings in `[security]` or with CLI overrides; see [CONFIGURATION.md](CONFIGURATION.md). Zero cannot disable protections. `--check-config` validates ranges without opening ports or a log. Real startup also validates programmatically supplied Network options.

## Admission and state bounds

After the kernel accepts a socket, the reactor checks the global active limit, then a shared per-IP limiter **before allocating a TLS session or adding a client watch**. All TCP/TLS listeners share that limiter. Pending TLS handshakes consume active slots. Close/error/timeout/stop release the slot; session initialization and watch failures roll back the active count too. A security admission already granted remains counted in its minute window even if later resource setup fails. Rejections do not extend the window or count as new admissions. The kernel TCP handshake can succeed before the application rejects the socket.

Canonical numeric source IPs come from the accepted sockaddr, not client-supplied ADC fields. IPv4-mapped IPv6 identities normalize to IPv4; listeners still use IPV6_V6ONLY. IPv4/IPv6 textual variants cannot create multiple keys for the same kernel address. Keys are stored only in memory, with at most 48 bytes each and 65536 map entries. An unseen source is rejected when the table is full. On admission, pruning runs at most every five seconds and removes inactive entries whose minute window expired; active entries remain. The map does not grow without bound and contains no persistent bans or account identities.

Minute and frame windows begin with their first admission/result and reset on demand; they are fixed windows, not rolling/token buckets. Adjacent-window bursts are possible. A NAT/proxy's clients share one IP budget; configure operational limits accordingly. Proxy headers and trusted reverse-proxy identity are not implemented.

## Frames and deadlines

Every decoder result consumes the per-connection frame budget: valid messages, valid unknown extensions, keepalives and parser errors. Oversize input emits one error and is discarded through its LF without growing the 16 KiB buffer. The Nth invalid result closes the connection when max_invalid_frames=N; valid frames do not reset that lifetime count. A result exceeding the frame-rate budget is excluded from normal ADC counters. The reactor feeds at most one frame segment per decoder call and stops processing the rest of the read after a quota breach. The crossing result may already have been parsed, but later frames in that read are not parsed. There is no queue of pending frames.

TLS has an absolute deadline from admission; partial handshake bytes do not refresh it. Its effective lifetime is bounded by both the configured handshake deadline and idle timeout. Each unfinished ADC frame has an absolute deadline from its first observed bytes, including the oversized discard state. More bytes do not renew it. Completing the frame clears its timer, and another partial frame in the same read gets its own start time. Timeout checks run before/after nonblocking I/O and before decoding segments, so late input cannot reset an expired deadline. The idle timer still measures application activity separately. Expiry is also scanned around the normal <=250 ms reactor poll; CPU/storage/scheduler delay can add latency.

A protocol frame may be fragmented across reads/TLS records. UTF-8/NFC/header/escape validation, NUL/control rejection and bounded parameter handling remain in the parser. EOF/close can report a final truncated frame once; this finalization is counted as invalid after the peer is already being removed, without consuming another live frame-rate slot.

## Observability and privacy

Atomic `security_rejected` counts per-IP/cache admission denials, also included in Network rejected. `security_closed` counts frame-quota/invalid-threshold closes and handshake/partial deadlines (including idle expiry during an unfinished TLS handshake). Ordinary idle expiry, peer I/O failure, EOF and normal stop do not increment it. The INFO shutdown line is `Security stopped; rejected=N closed=N`; atomic fields are sampled independently rather than transactionally.

Rate-limited warnings identify admission quotas, frame quotas and partial-frame deadlines without source addresses or payloads. Opt-in INFO lifecycle logs add `security_quota` and `partial_frame_timeout`, reusing `handshake_timeout` for TLS expiry. Counts and warnings can remain visible even when detailed peer events are disabled. No ADC text, nicknames, CID/PID, passwords/hashes or source IPs are logged. Listener readiness still shows configured local bind addresses. See [LOGGING.md](LOGGING.md).

## Other retained controls and limits

TLS >=1.2 protects only ADCS sockets. Loadable matching PEM credentials are checked before readiness; clients must validate trust/name/validity. No client certificates or password authentication exist. Keep keys outside Git with restricted permissions. INI reads and log files retain descriptor/type/size checks, collision protection and single-line bounded UTF-8 logging; use trusted directories. SIGINT/SIGTERM cleanup is synchronous, SIGPIPE is ignored by main, and descriptors use close-on-exec. [TLS](ADCS_TLS.md) and [configuration](CONFIGURATION.md) describe operational details.

These controls are not DDoS protection, a byte-bandwidth limiter, an authenticated audit trail or a security audit. Slow disk/stderr can block synchronous logging. Fatal reactor/allocation errors can still terminate the process. There is no per-subnet/global connection-attempt rate limiter, abuse sharing across processes, serializer/output backpressure, session semantic validation, accounts/roles/bans, hub password storage, application SQL persistence or role-based HTTP administrative authorization yet. Future modules must implement those explicitly. Tests and their actual OS/toolchain coverage are in [TESTING.md](TESTING.md).

## Database credentials — alpha-0.1.8

Use a dedicated local SQL account with minimal privileges. Passwords come only from a private owner-controlled regular file, never CLI/INI values. SQL parameters are bound; LOCAL INFILE and reconnect are disabled. Log paths cannot alias the password. Errors omit server text and secrets. Migrations run only explicitly with a cooperative lock and recorded SHA-256; they are not transactional backups or schema drift audits. See [DATABASE.md](DATABASE.md).

## Local administrative API — alpha-0.1.9

Same-effective-UID peer credentials, a private 0700 directory/0600 socket and a private random 64-hex-character Bearer token gate every read-only request. Token comparison is constant-time for equal lengths. No TCP listener, mutations, browser CORS or anonymous health route exists. A strict bodyless HTTP subset rejects duplicate/framing ambiguity; clients, headers, buffers and absolute request/response time are bounded. Request data/secrets are never logged or reflected. Cleanup checks inode identity and never deletes an existing endpoint at startup. Private directories and service-UID processes remain trusted; root/same-UID compromise, API rate limiting, web sessions, roles, gateway and request auditing are not solved here. See [API.md](API.md).

## WebAdmin foundation — alpha-0.1.10

Never put the API token in browser storage, JavaScript, URLs, VITE-prefixed variables or HTML. The optional development adapter validates private owner-controlled credential files outside the served root, blocks untrusted origin/host/site/method/path/body combinations, injects its own credential and normalizes bounded responses. It is loopback development tooling, not production browser authentication. The Apache2 sample serves only static local assets and denies API routes. No new server mutation surface, remote API exposure, web sessions or roles are introduced. See [WEBADMIN.md](WEBADMIN.md).

## Apache2 HTTPS example — alpha-0.1.11

TLS and browser authentication are separate controls. The template selects TLS 1.2/1.3 with AEAD ciphers, fixes the HTTP redirect hostname, retains Require local, denies private/hidden/secret paths and disables directory indexes/symlinks. HTTPS-only HSTS is short and host-scoped; CSP/no-sniff/referrer/frame headers apply to errors too. Certificate trust/name/key matching must be verified before use; syntax checks alone do not prove matching keys. The template never exposes the service token or activates a production gateway. See [APACHE2_HTTPS.md](APACHE2_HTTPS.md).

## Logging security — alpha-0.9.10

[Logging security](LOGGING_SECURITY.md) keeps a new log at mode `0600` and refuses an existing log that another user can read or that this user does not own. Fixed security outcomes are recorded without peer data. The lines are not an audit trail, so residual threat T11 stays open. A slow disk can still block the process, so residual threat T13 stays open. Schema v9 is unchanged. No status counter, WebAdmin page, INI key or Apache setting is added.

## System security — alpha-0.9.9

[System security](SYSTEM_SECURITY.md) adds one systemd example. The hub runs as user `dc24h` with an empty capability set, `NoNewPrivileges` and `ProtectSystem=strict`. The build does not install or start the service. Writable paths are the state, runtime and log directories created for that unit. This does not close residual threat T07. Schema v9 is unchanged. No status counter, WebAdmin page, INI key or Apache setting is added.

## Database security — alpha-0.9.8

[Database security](DATABASE_SECURITY.md) writes one consistent owner-only snapshot when the operator asks and restores it only into empty application tables of the same schema. A lost acknowledgement is not replayed. Off-host copies, a schedule and a replica stay with the operator, so residual threat T14 stays open. Schema v9 is unchanged. No status counter, WebAdmin page, INI key or Apache setting is added. `Require local` stays.

## API security — alpha-0.9.7

[API security](API_SECURITY.md) requires the session id and the CSRF secret on every maintenance call except the password proof. A grant alone returns 403 `csrf_required` and no maintenance rows. The role is loaded only after that binding matches. Copying all three values from page memory still works until the slot ends, so residual threats T04 and T05 stay open. Schema v9 is unchanged. No status counter, new WebAdmin page, INI key or Apache setting is added. `configuration.manage` and `hub.manage` stay unimplemented.

## Account security — alpha-0.9.6

[Account security](ACCOUNT_SECURITY.md) refuses an ADC password that matches the account password byte for byte, and decrypts the ADC password only after the account ban check and the per-account attempt budget admit the connection. An account-throttled or account-banned attempt receives no GPA. This does not close residual threats T02 or T03. Schema v9 is unchanged. No status counter, WebAdmin page, INI key or Apache setting is added.

## TLS security — alpha-0.9.5

[TLS security](TLS_SECURITY.md) checks the hub leaf certificate before listen and limits ADCS to TLS 1.2 and 1.3 AEAD suites. Denial stops startup and does not open a port. Hostname, trust and revocation stay with the client. HTTP/2, OCSP and automatic renewal remain unimplemented, so residual threat T15 stays open. Schema v9 is unchanged. No status counter, WebAdmin page, INI key or Apache setting is added.

## Traffic limits — alpha-0.9.4

[Traffic limits](TRAFFIC_LIMITS.md) caps ready-session INF updates at 4 per second after a burst of 8, before the update is applied or relayed. Denial returns one recoverable status per second and leaves the session open. Login INF is not charged. Chat, search, transfer and status budgets stay separate. This is not a bandwidth cap and it does not close residual threat T13. Schema v9 is unchanged. No status counter, WebAdmin page, language string, INI key or Apache setting is added.

## Network security — alpha-0.9.3

[Network security](NETWORK_SECURITY.md) caps the listen backlog at 128 and, on each accepted socket the hub keeps, sets kernel send and receive buffers to 65536 bytes and `TCP_USER_TIMEOUT` to the configured idle timeout. Protection runs after a session id is acquired and before TLS. A failed option closes that socket and releases the admission. Existing application timeouts, frame budgets and output queues stay. This is not a bandwidth cap and it does not close residual threat T13. Schema v9 is unchanged. No status counter, WebAdmin page, language string or Apache setting is added.

## ADC security — alpha-0.9.2

[ADC security](ADC_SECURITY.md) names the existing hub command profile as `control_message` and calls it from the connection state machine before state and sequence checks. Unknown commands and GET, GFI and SND stay unsupported. Outgoing PAS stays an invalid type. The parser still accepts unknown extensions and hub-legal frames. A fixed-seed in-process campaign checks the stream decoder, parser round-trip and agreement with that profile. It is not coverage-guided fuzzing and it does not close the alpha-0.9.1 residual threats. Schema v9 is unchanged. No new status counter, WebAdmin page or Apache setting is added.

## Threat model — alpha-0.9.1

[Threat model](THREAT_MODEL.md) records assets, actors, trust boundaries and sixteen residual threats for the hub and local WebAdmin. The analysis adds no protocol check, schema migration, Apache setting or WebAdmin control. Schema v9 is unchanged. A public authenticated gateway remains separate work.

## Security sessions — alpha-0.8.15

Each of the four in-memory maintenance grants now has a public session id and a separate CSRF secret. Maintenance writes require the grant, the session id and the CSRF secret from the same slot. A mismatch returns 403 `csrf_required`. alpha-0.9.7 requires that same binding on reads. Each session accepts 24 requests in 10 seconds and then returns 429 `rate_limited`. The process still accepts at most one maintenance POST per second. The budget is not taken from a client address or a proxy header. WebAdmin keeps the three values in page memory and clears them together. They are not written to SQL, cookies or web storage. `Require local` stays. See [Security sessions](SECURITY_SESSIONS.md).

## Apache2 reverse proxy — alpha-0.8.14

The separate proxy example terminates TLS and forwards an allowlisted `/admin-api/v1` surface to the loopback adapter. Client bearer tokens, cookies and forwarded-identity headers are removed before that hop. The canonical HTTPS origin is rewritten to the adapter's loopback origin; every other origin is denied. The service token and the C++ Unix socket are not in the Apache file. `Require local` stays in place. The hub still derives admission identity from the accepted sockaddr and still does not trust proxy headers. See [APACHE2_PROXY.md](APACHE2_PROXY.md).

## ADC state boundary — alpha-0.2.3

The independent state module rejects premature commands, repeated login steps, inbound hub-only types and spoofed source SIDs. Explicit approvals require a pending request; parsing INF/PAS alone never grants NORMAL. It stores no PID, PAS, nickname or payload and performs no logging. This is protocol sequencing, not authentication or a deployed security boundary: the receive-only Network does not invoke it. Future dispatch must validate feature/hash compatibility, PID/CID, required fields, IP and nickname/CID uniqueness and passwords before approval, remove PD before forwarding, apply budgets/timeouts, and close on rejection. See [ADC_STATE.md](ADC_STATE.md).

## SUP policy — alpha-0.2.4

The composed SUP/state module now checks initial BASE/common TIGR, keeps local and peer advertisements separate, bounds feature/token storage and rolls back failed updates. The selected hash cannot be removed or changed. Unknown peer features are inert metadata and are never reflected into the hub response. Additional local features require caller implementations; the default advertisement is BASE/TIGR only. Selection is not Tiger computation, PID/CID validation, password verification or TLS configuration. Production transport does not invoke the module yet. No peer text or credentials enter errors/logs. See [ADC_SUP.md](ADC_SUP.md).

## SID ownership — alpha-0.2.5

The shared pool guarantees uniqueness among live reservations within one hub, including pending ISID sends. Move-only private leases prevent arbitrary release by a copied SID and moved-from/stale reset cannot release a new owner. Codec validation covers all 20 bits; local policy excludes zero/AAAA from client allocation. SIDs are deterministic public routing identifiers, not secrets, authorization or session cookies. Actor/source checks remain required. Exhaustion yields a fixed error and no partial reservation. Pool/session mutation and final shared-owner destruction belong to one thread. Cancel old queued output and registry references before release/reuse. The module is not yet a live transport security boundary. See [ADC_SID.md](ADC_SID.md).

## INF validation — alpha-0.2.6

Initial NI/ID/PD, canonical hash encoding, Tiger(PID)==CID, numeric/text/feature bounds and peer-IP matching are checked before commit. PD is borrowed from caller input, never stored in ClientInfo and removed from public snapshots; decoded temporary PID bytes are cleansed. Original caller-owned frames are not erased by this API and must never be logged/forwarded. Client privilege claims beyond CT 0/1 are rejected; CT1 is informational, not authorization. Unknown fields are ignored; duplicate fields fail, including unknown names. Libgcrypt algorithm unavailability fails validation without substituting another hash or changing global crypto policy. Global nickname/CID uniqueness, account authentication, roles and actual transport attachment remain outside this module. See [ADC_INF.md](ADC_INF.md).

## Dedicated CID/PID verifier — alpha-0.2.7

The shared verifier borrows encoded strings, decodes PID directly into a noncopyable local buffer, hashes into a second cleansed buffer and compares exactly 24 bytes with CRYPTO_memcmp. Every normal return cleans both owned buffers; caller input, registers and arbitrary compiler/library copies are not promised erased. Constant-time applies to the final byte comparison, not syntax checks or the whole handshake. Public success contains only CID, not an account credential, PID, role or global uniqueness proof. Deterministic algorithm/context/read failures are tested with executable-local linker wrappers; production has no injected provider and no FIPS/global-mode changes. See [ADC_IDENTITY.md](ADC_IDENTITY.md).
