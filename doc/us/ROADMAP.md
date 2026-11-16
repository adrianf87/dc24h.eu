# Roadmap

**alpha-0.7.9 Auth Sessions:** [connection-bound account sessions](AUTH_SESSIONS.md) add atomic readiness binding, trusted account-filtered lookups and per-connection revocation. Closing clears authentication immediately. No new SQL migration, automatic role grants or browser sessions are added. CI uses [GitHub-hosted runners](GITHUB_CI.md) and is not awaited.

**alpha-0.7.8:** [ADC Authentication](ADC_AUTHENTICATION.md) implements account-backed GPA/PAS with separate AES-256-GCM encrypted ADC passwords and an external private key, while preserving scrypt. Explicit migration v6 and active exact nickname/CID mappings are required; ADCS is the default. The bounded startup directory requires restart for changes/revocations. Public provisioning/reset/rotation and role grants remain future work. This entry supersedes earlier stage descriptions of authentication as planned.

**Delivered in alpha-0.7.7:** [Password Security](PASSWORD_SECURITY.md) adds fixed-format, fixed-cost scrypt verification through a trusted read-only C++ service, constant-time key comparison and cleansed owned buffers. A credential match does not activate accounts or admit sessions; public authentication, reset and rehashing remain future stages.

**Delivered in alpha-0.7.6:** [Identity Mapping](IDENTITY_MAPPING.md) adds immutable, unique account/nickname/CID bindings and exact-pair lookup in explicit schema v5. Binding is a trusted management operation; password authentication, session grants and revocation remain future stages. [Contabo CI](CONTABO_CI.md) uses Ubuntu 24.04 and six parallel workers.

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


| Stage | Implemented scope |
| --- | --- |
| alpha-0.7.3 Account Model | Independent checked account IDs/names, role/status, UTC audit times and immutable candidate updates |
| alpha-0.7.4 Account Repository | Implemented: generated IDs, read/write metadata, exact unique names, revisions and explicit schema v3 |
| alpha-0.7.5 Registration | Implemented: trusted C++ registration, fixed least-privilege account defaults, salted scrypt and atomic schema v4 writes |
| alpha-0.1.1 Core | Lifecycle, start/stop, synchronous system signals, logging |
| alpha-0.1.2 Network | Nonblocking TCP, IPv4/IPv6, multiple listeners and clients, resource limits |
| alpha-0.1.3 ADC | Bounded stream syntax parser, commands and data validation |
| alpha-0.1.4 ADCS | TLS credentials, nonblocking encrypted inbound transport, tests, US/PL documentation and operator messages |
| alpha-0.1.5 Configuration | INI, CLI overrides, path/limit validation and append-file logging |
| alpha-0.1.6 Logging | Startup summaries, WARN, typed connection events, bounded UTF-8 and rate budgets |
| alpha-0.1.7 Security | Bounded IP admission, frame quotas and absolute deadlines |
| alpha-0.1.8 Database | Optional local MariaDB connection, prepared queries and explicit migration metadata |
| alpha-0.1.9 API | Local authenticated HTTP/JSON status and health; read-only |
| alpha-0.1.10 WebAdmin | React/TypeScript/Vite read-only layout, EN/PL, local development adapter, static Apache example |
| alpha-0.1.11 Apache2 | Static HTTPS example, canonical redirect, TLS/security policy and disposable Apache tests |
| alpha-0.2.1 Project | Audit of alpha-0.1, bounded consistency fixes, strict CI integration gate and alpha-0.2 baseline |
| alpha-0.2.2 ADC Parser | BASE argument syntax, named fields, scalar helpers and TCP/TLS recovery |
| alpha-0.2.3 ADC State | Independent hub state machine, explicit approvals and command/direction/source gates |
| alpha-0.2.4 SUP | Feature AD/RM, BASE/common TIGR, bounded support and state composition |
| alpha-0.2.5 SID | 20-bit codec, bounded shared pool, RAII leases and managed SID sessions |
| alpha-0.2.6 INF | Bounded metadata, Tiger identity verification, peer-IP checks and public PD-free snapshots |
| alpha-0.2.7 CID/PID | Dedicated Tiger verifier, public identity result, cleansed buffers and provider-failure coverage |
| alpha-0.2.8 Login | Live guest NORMAL, ordered bounded output, nickname/CID registry, public presence and ncdc reconnect |
| alpha-0.2.9 STA/QUI | Semantic errors, status routing, final own QUI and bounded terminal drain |
| alpha-0.2.10 Security | Pending admission, cumulative negotiation budgets and pre-verification quotas |
| alpha-0.3.1 Session Core | Users model, stable lifetime identity, snapshots and actual connection lifecycle |
| alpha-0.3.2 Session Manager | Central ownership, bounded registry, stable lookup, phase counts and snapshot pages |
| alpha-0.3.3 User Core | Public guest identity/profile, lifecycle ownership and validated INF projection/updates (integrated after Session Manager) |
| alpha-0.3.4 Session Manager | Central public identity indexes and managed lifecycle/profile mutations |
| alpha-0.3.5 User Core | Checked public names/profile text, immutable rename values and verified ADC projection |
| alpha-0.3.6 Nick Manager | Bounded nickname ownership, validation, transactional rename and lifecycle cleanup |
| alpha-0.3.7 User Registry | Bounded SID/CID ownership, read-only User lookup and transactional admission/cleanup |
| alpha-0.3.8 User Login | Verified guest registration, typed failures and identity-bound final-welcome readiness |
| alpha-0.3.9 User List | Deterministic initial synchronization, bounded owned welcome and FIFO presence updates |
| alpha-0.3.10 INF Updates | Transactional metadata/profile/nickname updates and sanitized incremental fanout |
| alpha-0.3.11 User Disconnect | Finalization, resource cleanup and bounded public QUI after release |
| alpha-0.3.12 Limits | Configurable connection/session/user ceilings and lifecycle-safe admission/reuse |
| alpha-0.3.13 Statistics | Lifecycle counts, online/peak, connection counters, API and WebAdmin |
| alpha-0.4.1 ADC MSG | MSG codec and state validation; chat routing remains planned |
| alpha-0.4.2 Chat Core | Public BMSG/action delivery; private/group policy and moderation remain planned |
| alpha-0.4.3 Public Chat | Dedicated public BMSG policy and shared-frame fanout; private/group chat remains planned |
| alpha-0.4.4 Private Chat | Private DMSG/EMSG delivery; group chat, history and moderation remain planned |
| alpha-0.4.5 Message Routing | Central MSG router, engine-domain binding and safe lifetime/queue dispatch |
| alpha-0.4.6 Message Flags | Shared typed PM/ME validation and canonical public/private projection |
| alpha-0.4.7 Hub Messages | System IMSG preparation/dispatch, bounded shared output and guest welcome |
| alpha-0.4.8 Chat Validation | Shared client text/SID/sender checks, recipient state and dispatch revalidation |
| alpha-0.4.9 Anti-Flood | Enabled B/D/E count/byte limits, recoverable drops and INI policy |
| alpha-0.4.10 Chat Configuration | Grouped INI settings, client policy and configurable greeting with unchanged readiness |
| alpha-0.4.11 Chat Logging | Payload-free bounded outcome logs and aggregate API/WebAdmin counters |
| alpha-0.4.12 WebAdmin | Dedicated read-only chat statistics with all aggregate counters |
| alpha-0.5.1 Search Core | Checked textual query model and ready-session dispatch library; live SCH/RES remain planned |
| alpha-0.5.2 ADC SCH | Typed field parsing and ADC/ADCS reception; routing/results remain planned |
| alpha-0.5.3 Search Filters | Unicode name/path, extension, size and type predicates; routing/results remain planned |
| alpha-0.5.4 TTH Search | Exact file-root predicates, owning SCH/Core/descriptor projection and hash-only queries; routing/results remain planned |
| alpha-0.5.5 Search Tokens | Bounded lifetime/TO registry, deadlines and generation-bound active Core plans; live result correlation/routing remains planned |
| alpha-0.5.6 Search Routing | Live B/F/D/E SCH delivery over ADC/ADCS, shared bounded queues and active TO; RES remains planned |

## Historical acceptance plan before alpha-0.2.8

alpha-0.2.2 delivers the requested fuller parser. The earlier serializer proposal remains planned; subsequent stage numbers will be assigned when their scope is requested.

These are planned acceptance gates, not implemented features. Define the final scope before starting each numbered stage.

| Proposed stage | Scope | Acceptance gate |
| --- | --- | --- |
| Delivered in alpha-0.2.8: ADC output | Serializer, escaping, header validation and encoded byte limits | Byte-exact grammar fixtures; parse/serialize cases, Unicode/boundary/error coverage; no writes yet |
| Delivered in alpha-0.2.8: Network output | Bounded per-client and aggregate queues, TCP/TLS partial writes, WANT_READ/WANT_WRITE and slow-reader policy | TCP/ADCS fragmented writes, queue limits, fairness under a stalled reader, peer failure and bounded shutdown; no unbounded allocation |
| Delivered in alpha-0.2.8: guest sessions | Wire integration of state/SUP/SID/INF modules, user registry, nickname/CID uniqueness and admission | Legal/illegal state transitions, SID uniqueness/release, spoofed-source and duplicate identity rejection; no PID leakage |
| Delivered in alpha-0.2.8: guest client integration | Complete guest login over ADC and ADCS, lifecycle/disconnect behavior and readiness semantics | Real ncdc reaches normal logged-in state on both transports; reconnect, concurrent users, malformed negotiation and existing regressions pass |

Full chat, search and transfer coordination remain separate future stages after session correctness. Registered users require a reviewed authentication/permissions/password design and application SQL migrations. Production web administration requires an authenticated Apache gateway, browser sessions/roles, CSRF policy and audit before exposure. Logging isolation/rotation, TLS close_notify/reload, measured load/fuzz/fault-injection and the remaining OS matrix remain explicit follow-up work.

## Branches and completion

alpha-0.2.2 starts from main at `ca464e02e714c1e81ed4c37279496168a6c47e23`, after both audit branches were merged. It does not change main or move the alpha-0.2 marker automatically.

alpha-0.2.1 originates from main after the alpha-0.1.11 merge. alpha-0.2 is prepared at the same audit commit as a line marker; it is not a substitute for the requested main-based numbered branches. Merge a reviewed numbered stage to main only on a separate instruction. Keep historical branches and immutable SQL migrations.

Every stage must pass strict integration CTest, frontend checks when affected, metadata/documentation checks and an updated changelog. Green initial-HSUP ncdc tests are transport evidence only until the complete-login gate is implemented. See [the audit and prioritized findings](AUDIT_ALPHA_0.1.md).

alpha-0.2.3 starts from main at `4c98e10` after the alpha-0.2.2 merge. Its [state module](ADC_STATE.md) completes the standalone sequencing gate; outbound transport, feature/identity/password validation, SID uniqueness and complete ncdc login remain planned. This stage does not merge itself into main.

alpha-0.2.4 starts from main at `1dd0b9f1a94626789d1ac9addad991a6eae78910`, after the explicit alpha-0.2.3 merge. The [SUP module](ADC_SUP.md) implements feature/hash selection and composes it with state approval. Generic output, live transport attachment, hash computation and complete login remain planned. This branch does not merge itself into main.

alpha-0.2.5 starts from main at `350a43f69b5134bd049923739cd606f798f846ec` after the SUP merge. [SID](ADC_SID.md) completes allocator/lease ownership and composition; live output/session attachment and user identity remain planned. This branch does not merge itself into main.

alpha-0.2.6 starts from main at `85524067756ac5fcbd177bb47f5d2cfe33fad063`. [INF](ADC_INF.md) completes intrinsic field/PID-CID validation and stack composition. Global registry/admission, registered-user passwords, live output and complete login remain planned. This branch does not merge automatically.

alpha-0.2.7 starts from main at `9d3443a4d889e745b6490a139435a8f44297cfba`. [CID/PID](ADC_IDENTITY.md) extracts and hardens the basic check introduced in INF; it does not add live login, global CID uniqueness, account authentication or file-tree hashing. This branch is not merged automatically.


## alpha-0.2.8 Login

Delivered: ordered output/serializer, guest sessions and registry, live TCP/TLS guest NORMAL and ncdc reconnect. Remaining: registered-user credentials/permissions, chat/search/transfer handlers, TLS close_notify/reload, production load/fuzz/fault-injection and remaining OS validation. Base: main b8978491fa8e146527eba818f9bc1ab0a31aa90d. The numbered branch is not merged automatically.
