# Development

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

**Current increment — alpha-0.5.6:** [Search Routing](SEARCH_ROUTING.md) forwards checked SCH over real ADC/ADCS: B/F broadcasts and lifetime-bound D/E delivery share one immutable packet with independent bounded queues. Supplied TO is registered in Search Core; absent TO remains valid. Source/state/feature checks, one-attempt dispatch and token cleanup are enforced. RES/result routing remains a later increment.

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

The alpha-0.2 baseline and prioritized acceptance gates are recorded in [the alpha-0.1 audit](AUDIT_ALPHA_0.1.md). Numbered branches still start from current main. `alpha-0.2` marks the reviewed audit commit. Use `DC24H_REQUIRE_INTEGRATION_TESTS=ON` for full validation; keep project.metadata, npm lock metadata, runtime/frontend version and both documentation trees aligned.

Create each new release branch from the current `main`, preserving existing history and code. alpha-0.1.4 was created from main and merged the reviewed `chatgpt-alpha-0.1.4` history; fixes and validation follow on the release branch. Main is not automatically changed by this work.

Keep each module in its own `src/` and `include/dc24h/` directory. Use C++20 without compiler extensions, RAII, explicit ownership and bounded data structures. Network operations stay in the reactor thread; new modules must define their startup/shutdown dependencies. UI and database logic must not be coupled into Core. Public headers describe lifetime requirements.

Default code and documentation language is EN/US UTF-8. Polish documentation lives in `doc/pl`; English in `doc/us`. Only the root index links to both trees; duplicate root module files were removed. Keep the ordered built-in catalogs in `languages/us/messages.hpp` and `languages/pl/messages.hpp` aligned. Translate operator text only, never protocol payloads. Internal/system diagnostics without catalog entries use the English fallback.

Before significant commits: build, run appropriate CTest suites, update documentation and CHANGELOG, inspect Git diff and exclude secrets/build artifacts. Meaningful checks must also work with Release assertions disabled. Every stage gets a descriptive commit. Include source version and commit SHA in bug reports. Do not claim planned modules or unverified OS/CI results are complete.

alpha-0.1.5 starts from main after the explicit alpha-0.1.4 merge. The configuration/logging sources and tests from chatgpt-alpha-0.1.5 were reviewed and adapted, preserving main TLS and i18n fixes. No en/ duplicate language directory is imported.

alpha-0.1.6 starts from main after alpha-0.1.5. The logging-specific commits in chatgpt-alpha-0.1.6 were reviewed and adapted; main's safer INI/file handling, TLS scheduling and US/PL support are retained. Use typed ConnectionEvent values and keep all payload/peer data out of operational logs.

alpha-0.1.7 starts from main after the alpha-0.1.6 merge. The security sources/tests in chatgpt-alpha-0.1.7 were reviewed and adapted. Retain bounded admission state, canonical source identities, one release per slot and absolute per-frame deadlines; never log the source keys or payloads. Security is independent of the planned authentication module.

## Database increment

alpha-0.1.8 starts from main after alpha-0.1.7. Keep SQL handles and credentials out of Core/Network, use bound values, and keep applied SQL migrations immutable. Extend migration recovery before adding non-idempotent DDL. Disposable database tests must use their own data directory/socket and `--no-defaults`; never connect to a developer database.

## API increment

alpha-0.1.9 starts from main after the alpha-0.1.8 merge. Preserve the narrow bodyless HTTP contract and local-only access controls. Do not add mutations, body parsers, Apache exposure or browser token storage without explicit authorization/role/audit design. Providers must be thread safe, bounded and outlive API destruction. Use disposable private Unix sockets/tokens in tests, and verify both ncdc counters and MariaDB status through the real API.

## WebAdmin increment

alpha-0.1.10 starts from main after alpha-0.1.9. Keep browser-safe schema/formatting separate from the server-only Vite adapter, maintain EN/PL key parity, never include secrets or node_modules/dist/test results in Git, and update package-lock with dependency changes. Verify the frontend build, unit/browser/live C++ integration and existing server tests. Administrative mutations and production web authentication require a separate reviewed design.

## Apache2 increment

alpha-0.1.11 starts from main after alpha-0.1.10. Keep host activation, real certificates and service reloads separate from example authoring/testing. Test the exact shipped TLS/access/header policy with temporary paths/ports and no host config. A successful apache2 -t is insufficient to claim certificate/key correspondence; exercise real initialization. Keep both documentation languages and version-sensitive fixtures aligned.

## ADC Parser increment

alpha-0.2.2 starts from main after the explicit alpha-0.2/alpha-0.2.1 merge. Keep BASE argument syntax separate from authorization, identity checks and execution. Preserve opaque extension layouts, field order and duplicates. NamedParameterView borrows a decoded token; never retain it after mutation/destruction. Add release-active syntax fixtures and full-result stream comparisons for new grammar rules, then verify TCP, TLS and both ncdc transports.

## ADC State — alpha-0.2.3

This branch starts from main at `4c98e10` after the parser merge. Maintain the [state contract](ADC_STATE.md), one noncopyable instance per future connection, explicit semantic approvals, unchanged-state rejection and terminal disconnect. Keep state tests active in Release; update the command/state and direction/source matrices with every new command. Never infer authentication from a parsed frame or advance on an enqueued response. Transport integration/output, validation and unique SID allocation remain separate acceptance work.

## SUP — alpha-0.2.4

Branch from main at `1dd0b9f` after the ADC State merge. Keep support parsing, protocol sequencing and identity/hash computation separate. Prefer ProtocolConnection over manual low-level protocol approval for future transport integration. New hash algorithms require an explicit registry/metadata and selection/reply-order tests; unknown feature names are never guessed to be hashes. Preserve transactional updates, immutable session hash, bounded cumulative support and deterministic local replies. ncdc fixture success is not live hub login. See [ADC_SUP.md](ADC_SUP.md).

## SID — alpha-0.2.5

Branch from current main at `350a43f`. Use one shared SidPool for one hub and unique move-only leases; never add release-by-SID or copyable ownership. Preserve allocation-free acquire/release, the 20-bit codec, full namespace and word-boundary tests, and one-thread lifetime rules. SidSession supersedes arbitrary caller-assigned ISID for future transport integration. Test cleanup before bind, terminal responses, destruction and reuse; do not call completed-send observation for queue insertion. Do not infer user identity from SID ownership. See [ADC_SID.md](ADC_SID.md).

## INF — alpha-0.2.6

Branch from main at `8552406` after the SID merge. Build with libgcrypt20-dev; extend known fields only with explicit type/domain/budget and cancellation tests. Preserve canonical base32/TIGER1 byte order, source/IP binding, immutable CID, no PD in retained/exported metadata and candidate-before-state commit. Use ClientSession for future attachment; lower-level state admission alone does not validate INF. Keep registry uniqueness/admission separate and never auto-grant NORMAL from a parsed INF. New cryptographic dependencies must be present in both CI jobs and installation guides. See [ADC_INF.md](ADC_INF.md).

## CID/PID — alpha-0.2.7

Branch from main at `9d3443a` after the INF merge. Use verify_tiger_identity for private PID input; do not duplicate string-encoded digest comparisons in field consumers. Extend public verified values without adding PID storage. Preserve exact TIGER1/canonical-tail semantics, direct failure clearing, cleanup on all exits and no fallback/global crypto changes. Linker fault wrappers are test-only. Tests must cover malformed encodings, mismatch, unavailable provider, recovery and actual ncdc pairs. See [ADC_IDENTITY.md](ADC_IDENTITY.md).
