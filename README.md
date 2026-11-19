# dc24h.eu

Direct Connect Hub server implemented in C++20 for Linux (Debian and Ubuntu).

**Current development version: alpha-0.9.10 (Logging security).**

CI on `main`: [GitHub-hosted Ubuntu 24.04](doc/us/GITHUB_CI.md) runs C++ Debug, C++ Release and WebAdmin · [PL](doc/pl/GITHUB_CI.md).

Completed increments (older bullets describe their original delivery scope):

- **alpha-0.9.10 Logging security**: a log file stays mode 0600 and owned by the hub user. Fixed security outcomes are recorded without peer data. This is not a production audit. See [Logging security](doc/us/LOGGING_SECURITY.md).
- **alpha-0.9.9 System security**: one systemd example runs the hub as a locked system user with an empty capability set and a strict read-only filesystem. The build does not install or start that service. See [System security](doc/us/SYSTEM_SECURITY.md).
- **alpha-0.9.8 Database security**: one explicit command writes a consistent owner-only snapshot and restores it only into empty tables of the same schema. A lost acknowledgement is not replayed. Off-host copies stay with the operator. See [Database security](doc/us/DATABASE_SECURITY.md).
- **alpha-0.9.7 API security**: every maintenance call except the password proof requires the session id and the CSRF secret from the same slot. A grant alone no longer reads. Copying all three values from page memory still works until the slot ends. See [API security](doc/us/API_SECURITY.md).
- **alpha-0.9.6 Account security**: enrollment refuses an ADC password identical to the account password, and the reactor decrypts that password only after the account ban check and the per-account attempt budget. Online guessing and a captured PAS with require_tls off remain separate. See [Account security](doc/us/ACCOUNT_SECURITY.md).
- **alpha-0.9.5 TLS security**: the hub refuses an expired, not-yet-valid or weak leaf certificate before listen, and offers only TLS 1.2 and 1.3 AEAD. Clients still check the name and the trust chain. HTTP/2, OCSP and automatic renewal remain separate work. See [TLS security](doc/us/TLS_SECURITY.md).
- **alpha-0.9.4 Traffic limits**: each ready session may send 8 INF updates at once, then 4 per second. A denial is one recoverable status per second and does not disconnect or relay. Chat, search and transfer budgets stay. A bandwidth cap remains separate work. See [Traffic limits](doc/us/TRAFFIC_LIMITS.md).
- **alpha-0.9.3 Network security**: the listen backlog is 128. Each accepted socket the hub keeps gets kernel send and receive buffers of 65536 bytes and a TCP user timeout equal to the idle timeout. Existing application timeouts and output queues stay. A bandwidth cap remains separate work. See [Network security](doc/us/NETWORK_SECURITY.md).
- **alpha-0.9.2 ADC security**: the hub command profile is one function, called before session state changes. A deterministic in-process fuzz campaign locks the parser, the stream decoder and that profile. Unknown extensions still parse. Coverage-guided fuzzing remains separate work. See [ADC security](doc/us/ADC_SECURITY.md).
- **alpha-0.9.1 Threat model**: a written analysis of assets, actors, trust boundaries and residual risk for the hub and local WebAdmin. It adds no control, schema change or public gateway. See [Threat model](doc/us/THREAT_MODEL.md).
- **alpha-0.8.15 Security sessions**: each in-memory maintenance grant carries a public session id and a separate CSRF secret. Writes require both on that same slot. Each session accepts 24 requests in 10 seconds. Reads stay grant-only. The values stay in process memory, not in SQL, cookies or web storage. A public authenticated gateway remains separate work. See [Security sessions](doc/us/SECURITY_SESSIONS.md).
- **alpha-0.8.14 Apache2 HTTPS proxy**: an allowlisted reverse proxy to the loopback WebAdmin adapter, with client bearer tokens, cookies and forwarded identity removed. The service token and the C++ Unix socket stay out of Apache. Local access remains the default. See [Apache2 HTTPS proxy](doc/us/APACHE2_PROXY.md).
- **alpha-0.8.13 WebAdmin Management**: a maintenance catalog of account, role and ban areas for the current grant, and a bilingual page that links to the existing flows. The page does not perform those writes. Configuration writes, live hub administration and browser kick or mute stay outside it. See [WebAdmin Management](doc/us/WEBADMIN_MANAGEMENT.md).
- **alpha-0.8.12 WebAdmin Users**: a bearer-only directory of ready sessions and a bilingual page of kick, mute and unmute decisions. The page shows the client command and does not send it, create bans, or reveal CID, IP, account or profile fields. See [WebAdmin Users](doc/us/WEBADMIN_USERS.md).
- **alpha-0.8.11 WebAdmin Dashboard**: a fixed read-only summary of existing process counters on the authenticated status API, and a bilingual main panel. Account lists, ban records, audit rows, configuration writes and live hub administration stay outside it. See [WebAdmin Dashboard](doc/us/WEBADMIN_DASHBOARD.md).
- **alpha-0.8.10 Admin API**: a password proof issues a short-lived in-memory grant. Maintenance reads and writes require that grant in addition to the process token. Status and health stay token-only. Configuration writes and live hub administration remain pending. See [Admin API](doc/us/ADMIN_API.md).
- **alpha-0.8.9 Audit Logs**: schema v9 records successful account, ownership and ban maintenance. An active account with audit.read lists that history. Refusals, passwords, proofs, free-text reasons and live kick or mute stay outside the table. See [Audit Logs](doc/us/AUDIT_LOGS.md).
- **alpha-0.8.8 Moderation Rules**: one allocation-free check of the actor permission and the operation target for kick, mute, unmute, account/CID/IP bans and ban revocation. Schema v8 is unchanged. See [Moderation Rules](doc/us/MODERATION_RULES.md).
- **alpha-0.8.7 Mute**: independent live public/private chat mute, authenticated role hierarchy, +mute/+unmute, session lifetime and bilingual read-only monitoring. Schema v8 is unchanged. See [Mute](doc/us/MUTE.md).
- **alpha-0.8.6 Temporary Bans**: relative durations, automatic admission expiry, filtered history and server-clock WebAdmin countdown. Schema v8 is unchanged. See [Temporary Bans](doc/us/TEMPORARY_BANS.md).
- **alpha-0.8.5 Ban Core**: persistent account/CID/exact-IP blocks, permanent/expiring admission policy, proven maintenance create/revoke, WebAdmin and explicit schema v8. See [Ban Core](doc/us/BAN_CORE.md).

- **alpha-0.8.4 Kick**: authenticated `+kick <nickname> [reason]`, strict role hierarchy, private terminal ADC notices, immediate routing/search cleanup and read-only WebAdmin counters. See [Kick](doc/us/KICK.md).

- **alpha-0.8.3 Admin Core**: central maintenance dispatch for eight account/ownership actions, fixed result classification, safe discovery and process counters in WebAdmin. See [Admin Core](doc/us/ADMIN_CORE.md).

- **alpha-0.8.2 Operators**: SQL-filtered staff directory, first-Owner provisioning and dual-proof atomic ownership transfer through a private local CLI. Schema v7 is unchanged. See [Operators](doc/us/OPERATORS.md).

- **alpha-0.8.1 Permissions**: seven-role hierarchy, seventeen exact capabilities, constrained VIP delegation, stronger-role enforcement, transactional maintenance role assignment and explicit schema v7. See [Permissions](doc/us/PERMISSIONS.md).

- **alpha-0.7.15 Configuration**: validated database/login policy, shared effective timeouts, a redacted CLI preview and bilingual read-only WebAdmin startup configuration. See [Database and Login Configuration](doc/us/DATABASE_AUTH_CONFIGURATION.md).

- **alpha-0.7.14 WebAdmin Accounts**: paginated account metadata and confirmed registration, activation, disable, reset and password changes through a separate local maintenance API. See [WebAdmin Accounts](doc/us/WEBADMIN_ACCOUNTS.md).

- **alpha-0.7.13 Database Resilience**: independent connection/read/write timeouts, explicit single-attempt reconnect with fresh credentials, transaction-scope guards and real MariaDB outage/restart tests. See [Database Resilience](doc/us/DATABASE_RESILIENCE.md).

- **alpha-0.7.12 Anti-Bruteforce**: bounded global/IP/account token buckets shared by ADC/ADCS and reconnects, generic refusal, automatic refill and fixed private log events. See [Anti-Bruteforce](doc/us/ANTI_BRUTEFORCE.md).

- **alpha-0.7.11 Account Management**: trusted atomic password change, administrator disable/reset with fresh proof and revision checks, ADC credential invalidation and explicit maintenance restart/reenrollment. See [Account Management](doc/us/ACCOUNT_MANAGEMENT.md).

- **alpha-0.7.10 Roles**: validated account roles in ready sessions, fixed search/transfer grants and staff session-revocation hierarchy; global policies and explicit session restrictions remain enforced. See [Roles](doc/us/ROLES.md).

- **alpha-0.7.9 Auth Sessions**: managed connection/account binding after PAS and final welcome LF, ready-only trusted lookups, bounded account-filtered pages and immediate connection revocation/cleanup. See [Auth Sessions](doc/us/AUTH_SESSIONS.md).

- **alpha-0.7.8 ADC Authentication**: account-backed GPA/PAS, separate AES-256-GCM encrypted ADC passwords with an external private key, unchanged scrypt credentials and ADCS by default. See [ADC Authentication](doc/us/ADC_AUTHENTICATION.md).
- **alpha-0.7.7 Password Security**: strict fixed-cost verifier decoding, credential checks without secret-bearing results, constant-time key comparison and cleansed owned buffers. See [Password Security](doc/us/PASSWORD_SECURITY.md).

- **alpha-0.7.6 Identity Mapping**: immutable account/nickname/CID bindings in MariaDB, exact-pair resolution, conflict-safe writes and explicit schema v5. See [Identity Mapping](doc/us/IDENTITY_MAPPING.md).

- **alpha-0.7.5 Registration**: atomic registered/disabled account and salted scrypt verifier creation through a trusted C++ API; bounded Unicode inputs, fixed costs and fault-tested rollback. See [Registration](doc/us/REGISTRATION.md).

- **alpha-0.7.4 Account Repository**: MariaDB account metadata storage, generated IDs, byte-exact unique names, safe row imports and transactional revision checks. See [Account Repository](doc/us/ACCOUNT_REPOSITORY.md).

- **alpha-0.7.3 Account Model**: independent checked persistent-account metadata, Unicode names, roles/status, UTC audit times and immutable candidates with allocation-safe ownership. See [Account Model](doc/us/ACCOUNT_MODEL.md).
- **alpha-0.7.2 Database Migrations**: ordered compiled SQL catalog, exact checksums, legacy metadata upgrade, read-only schema status and durable partial-migration detection without blind DDL replay. See [Database Migrations](doc/us/DATABASE_MIGRATIONS.md).
- **alpha-0.7.1 Database Core**: typed MariaDB parameters, bounded owning results, write metadata and owner-thread RAII transactions, with deterministic conflict/loss/fault tests and database-enabled real ncdc. See [Database Core](doc/us/DATABASE_CORE.md).
- **alpha-0.6.13 Configuration**: exact bounded peer-protocol policy, existing count/byte limits and all effective transfer settings in the authenticated API and US/PL Connections page. See [Transfer Configuration](doc/us/TRANSFER_CONFIGURATION.md).
- **alpha-0.6.12 WebAdmin**: observed CTM/RCM request, refusal and queue-copy rates, bounded browser-local chart/table, restart/outage recovery and EN/PL controls. See [WebAdmin Transfer Monitoring](doc/us/WEBADMIN_TRANSFERS.md).
- **alpha-0.6.11 Transfer Statistics**: saturating CTM/RCM outcomes, queue/error and peer-report counters in the authenticated API and US/PL Connections page; Node 24 action runtimes and a Debian 13 custom-runner Debug/Release matrix. See [Transfer Statistics](doc/us/TRANSFER_STATISTICS.md) and [Debian 13 CI](doc/us/CI_DEBIAN13.md).
- **alpha-0.6.10 Transfer Limits**: shared CTM/RCM source and aggregate recipient count/byte budgets, bounded private notices, monotonic refill and real ncdc recovery. See [Transfer Limits](doc/us/TRANSFER_LIMITS.md).
- **alpha-0.6.9 Error Handling**: centralized connection diagnostics, immutable shared STA routing with fixed lifetime audiences, allocation/queue fault isolation and disconnect/reset recovery. See [Error Handling](doc/us/ERROR_HANDLING.md).
- **alpha-0.6.8 Connection Security**: shared endpoint/port checks, configurable CTM/RCM and guest permissions, trusted lifetime grants, dispatch/echo authorization rechecks and real ncdc denial/recovery tests. See [Connection Security](doc/us/CONNECTION_SECURITY.md).
- **alpha-0.6.7 Transfer Tokens**: one bounded opaque connection-token policy, owning typed values, exact CTM/RCM/error forwarding and real ncdc boundary-token handshakes. See [Transfer Tokens](doc/us/TRANSFER_TOKENS.md).
- **alpha-0.6.6 Client Capabilities**: owning per-family TCP modes and advisory peer protocols, shared CTM/RCM admission, pure originator path selection and live INF mode changes with real ncdc. See [Client Capabilities](doc/us/CLIENT_CAPABILITIES.md).
- **alpha-0.6.5 Protocol Negotiation**: typed ADC/1.0 and ADCS/1.0 intent, capability/preference selection with required-TLS refusal, lossless peer negotiation errors and real ncdc TLS peer handshakes. See [Protocol Negotiation](doc/us/PROTOCOL_NEGOTIATION.md).
- **alpha-0.6.4 Connection Routing**: one CTM/RCM SID router with immutable fixed recipient lifetimes, shared packets and queues; parallel pairs sharing a token stay isolated. See [Connection Routing](doc/us/CONNECTION_ROUTING.md).
- **alpha-0.6.3 ADC RCM**: owning Reverse Connect To Me validation and live DRCM/ERCM routing, per-family passive/active roles, shared queues and real ncdc RCM→CTM peer handshakes. See [ADC RCM](doc/us/ADC_RCM.md).
- **alpha-0.6.2 ADC CTM**: owning Connect To Me validation and live DCTM target / ECTM target+source routing through Transfer Core, with active-source checks, shared bounded queues and real ncdc peer handshakes. See [ADC CTM](doc/us/ADC_CTM.md).
- **alpha-0.6.1 Transfer Core**: independent checked connection intents, IPv4/IPv6 active/passive roles and lifetime-bound one-attempt plans with cancel/revalidation. Live CTM/RCM and file transfers remain future stages. See [Transfer Core](doc/us/TRANSFER_CORE.md).
- **alpha-0.5.13 WebAdmin**: browser-local search traffic rates, a bounded chart and accessible interval table, with pause/manual refresh, local clear and restart/outage recovery. See [WebAdmin Search Monitoring](doc/us/WEBADMIN_SEARCH.md).
- **alpha-0.5.12 Configuration**: 19 aggregate SCH/RES counters, tracked token windows and all 16 effective search settings in the authenticated read-only API and US/PL WebAdmin Search page. See [Search Configuration and Statistics](doc/us/SEARCH_CONFIGURATION.md).
- **alpha-0.5.11 Search Security**: independent query/result/recipient count and byte quotas, bounded denials, guest/global permissions and trusted session grants, configurable active-search windows and optional strict result tokens. See [Search Security](doc/us/SEARCH_SECURITY.md).
- **alpha-0.5.10 Active/Passive**: checked public endpoint projection, independent IPv4/IPv6 TCP/UDP modes and reply paths in Search Core/plans, dynamic INF transitions, passive hub fallback and actual ncdc UDP interoperability. See [Active/Passive](doc/us/ACTIVE_PASSIVE.md).
- **alpha-0.5.9 Result Routing**: target-only ADC/ADCS DRES delivery with ready session lifetimes, active owner-token/generation checks, tokenless compatibility and bounded output queues. Real ncdc interoperability covers both receiving and producing results. See [Result Routing](doc/us/RESULT_ROUTING.md).
- **alpha-0.5.8 ADC RES**: typed DRES parser and live ADC/ADCS validation, bounded names/sizes/slots/tokens/TTH, recoverable private errors and real ncdc compatibility. Result routing and correlation remain future stages. See [ADC RES](doc/us/ADC_RES.md).
- **alpha-0.5.7 Feature Routing**: compiled exact FSCH selectors, current public INF SU matching, SUP independence, live capability changes and positive/negative fanout verified with ADC/ADCS and ncdc. See [Feature Routing](doc/us/FEATURE_ROUTING.md).

- **alpha-0.5.6 Search Routing**: real ADC/ADCS B/F/D/E SCH delivery, feature/lifetime checks, immutable shared packets, bounded queues and active-token lifecycle. Actual ncdc tests verify delivery to another client. RES/result routing remains a later stage. See [Search Routing](doc/us/SEARCH_ROUTING.md).
- **alpha-0.5.5 Search Tokens**: bounded session-lifetime/TO registration, local generations, expiration/cancellation and active-search Core APIs with rollback and allocation-free lookup/dispatch. Live result correlation/routing remains a later stage. See [Search Tokens](doc/us/SEARCH_TOKENS.md).
- **alpha-0.5.4 TTH Search**: exact 192-bit file-root predicates, hash-only and combined queries, lossless SCH/Core/descriptor projection and actual ncdc ADC/ADCS TTH tests. Live request/result routing remains a later stage. See [TTH Search](doc/us/TTH_SEARCH.md).
- **alpha-0.5.3 Search Filters**: compiled Unicode name/path, extension, size and type predicates over checked descriptors, filter-only Core queries and allocation-free matching. Routing/results remain later stages. See [Search Filters](doc/us/SEARCH_FILTERS.md).
- **alpha-0.5.2 ADC SCH**: typed owning search-request parsing, field/selector/TR validation and real ADC/ADCS reception with bounded, payload-free diagnostics. Routing/results remain later stages. See [ADC SCH](doc/us/ADC_SCH.md).
- **alpha-0.5.1 Search Core**: independent C++20 textual query validation, immutable lifetime-bound recipient plans, feature selection and bounded dispatch. Live SCH/RES adapters and search results remain future stages. See [Search Core](doc/us/SEARCH_CORE.md).
- **alpha-0.4.12 WebAdmin**: dedicated Chat statistics page with all nine aggregate counters, message/recipient queue definitions, US/PL navigation, live refresh, honest unavailable states and safe snapshot export. See [WebAdmin chat statistics](doc/us/WEBADMIN_CHAT.md).
- **alpha-0.4.11 Chat Logging**: opt-in payload-free outcome logs with a separate rate budget, saturating aggregate B/D/E chat counters, authenticated API and bilingual WebAdmin metrics. See [Chat Logging](doc/us/CHAT_LOGGING.md).
- **alpha-0.4.10 Chat Configuration**: checked startup `[chat]` settings for decoded text size, independent public/private/actions and optional/custom welcome, alongside existing Anti-Flood limits. Defaults retain previous behavior; disabled/oversized legal chat gets recoverable refusals. See [Chat Configuration](doc/us/CHAT_CONFIGURATION.md).

- **alpha-0.4.9 Anti-Flood**: enabled per-connection message/decoded-byte buckets for BMSG/DMSG/EMSG with configurable `[chat]` rates/bursts. Denied traffic has no fanout or echo; bounded recoverable notices permit later recovery. See [Anti-Flood](doc/us/ANTI_FLOOD.md).

- **alpha-0.4.8 Chat Validation**: shared text/SID/session validation for direct public/private adapter calls and routing. Ready sender ownership, bounded UTF-8/NFC text and complete message checks precede delivery plans; recipient and dispatch lifetime checks remain. See [Chat Validation](doc/us/CHAT_VALIDATION.md).

- **alpha-0.4.7 Hub Messages**: trusted system IMSG preparation and ready-session delivery, shared bounded output and a fixed guest welcome after own BINF. Clients cannot originate system messages. See [Hub Messages](doc/us/HUB_MESSAGES.md).

- **alpha-0.4.6 Message Flags**: shared typed PM/ME decoding and canonical parameter construction across the codec and public/private chat. Distinct absent/ME0/ME1 values, duplicate rejection and private-reply SID policy are covered by dedicated tests. See [Message Flags](doc/us/MESSAGE_FLAGS.md).

- **alpha-0.4.5 Message Routing**: central MSG scope selection and queue delivery, engine-bound plans and live session checks. Private/group/unsupported scopes cannot become public messages. See [Message Routing](doc/us/MESSAGE_ROUTING.md).

- **alpha-0.4.4 Private Chat**: DMSG target-only and EMSG target/sender delivery, deduplicated self-messages, source/PM checks and target lifetime binding. Private frames share immutable storage only between selected queues and never fall back to public delivery. See [Private Chat](doc/us/PRIVATE_CHAT.md).

- **alpha-0.4.3 Public Chat**: dedicated BMSG admission/projection/dispatch adapter with one immutable public frame shared by recipient queues. Independent offsets and logical charges preserve bounded FIFO delivery and pinned TLS output through partial sends, terminal close and source cleanup. See [Public Chat](doc/us/PUBLIC_CHAT.md).

- **alpha-0.4.2 Chat Core**: independent message engine with owned lifecycle-bound broadcast plans and live public BMSG delivery, including sender echo and /me. Unknown fields are removed; private intent is never downgraded to public. Existing bounded queues isolate slow/failing recipients. See [Chat Core](doc/us/CHAT_CORE.md).

- **alpha-0.4.1 ADC MSG**: dedicated MSG parsing/serialization, checked PM/ME flags, typed views/builders, exact escaped frame limits and live NORMAL/source validation. Actual ncdc public/action/private messages are tested; chat routing remains planned. See [ADC MSG](doc/us/ADC_MSG.md).

- **alpha-0.3.13 Statistics**: constant-time session lifecycle counts, online/peak users and successful logins; closed/rejected connections, peak peers and application byte totals exposed through the authenticated local API and US/PL WebAdmin. See [Statistics](doc/us/STATISTICS.md).

- **alpha-0.3.12 Limits**: configurable shared connection/session/user ceilings, typed admission gates and user-capacity rejection before welcome/list publication. Slots remain reserved through closing until cleanup. See [Limits](doc/us/LIMITS.md).

- **alpha-0.3.11 User Disconnect**: allocation-free lifecycle finalization, ordered release of output/protocol/SID and public identity reservations, then bounded public QUI. Failed notifications cannot retain the departing user or a usable stale recipient. See [User Disconnect](doc/us/USER_DISCONNECT.md).

- **alpha-0.3.10 INF Updates**: transactional ready-state INF updates commit ADC metadata, User/profile and nickname ownership together. Public incremental deltas carry only validated supplied fields and cancellations; unknown/private data is excluded. See [INF Updates](doc/us/INF_UPDATES.md).

- **alpha-0.3.9 User List**: deterministic initial public-user synchronization, complete welcome wire-budget preflight, lifecycle filtering and owned snapshots with own INF last. Mixed ADC/ADCS clients and real ncdc synchronize lists across joins, updates and reconnect. See [User List](doc/us/USER_LIST.md).

- **alpha-0.3.8 User Login**: explicit verified ADC guest-to-session registration, typed rejection, transactional public indexes and identity-bound welcome completion. Readiness is published once after the own-INF final LF is sent. See [User Login](doc/us/USER_LOGIN.md).

- **alpha-0.3.7 User Registry**: bounded SID/CID indexes with one registration per session lifetime, transactional admission and owner-specific cleanup. SessionManager resolves both keys to the same live, read-only User. See [User Registry](doc/us/USER_REGISTRY.md).

- **alpha-0.3.6 Nick Manager**: bounded, owner-indexed nickname reservations, shared checked name validation, transactional rename and safe release by connection lifetime. Integrated with SessionManager and tested over ADC/ADCS. See [Nick Manager](doc/us/NICK_MANAGER.md).

- **alpha-0.3.5 User Core**: checked UTF-8/NFC nickname values, shared ADC/User text rules, immutable rename candidates and explicit verified public INF-to-User projection. Real ncdc logs in and reconnects with a Polish nickname. See [User Core](doc/us/USER_CORE.md).

- **alpha-0.3.4 Session Manager**: central SID/CID/nickname indexes, const model lookup and managed lifecycle/profile mutations. Identity reservations remain consistent through admission, rename, close and removal. See [Session Manager](doc/us/SESSION_MANAGER.md).

- **alpha-0.3.3 User Core**: owning public guest identity and bounded typed profile, composed with session lifecycle and validated ADC INF updates/cancellations. See [User Core](doc/us/USER_CORE.md).

- **alpha-0.3.2 Session Manager**: bounded central ownership of live session models, stable lookup, phase counts and ordered snapshot pages. ADC/ADCS register and remove every connection through the manager. See [Session Manager](doc/us/SESSION_MANAGER.md).

- **alpha-0.3.1 Session Core**: independent Users lifecycle model, stable local session IDs, public identity snapshots, monotonic transition times and typed closure causes. Live ADC/ADCS uses this model for readiness, deadlines and cleanup. See [Session Core](doc/us/SESSION_CORE.md).

- **alpha-0.2.10 Security**: cumulative login input budgets, global/per-IP pending admissions and shared identity-attempt gates before Tiger validation. Tested with real ADC/ADCS and ncdc. See [negotiation security](doc/us/NEGOTIATION_SECURITY.md).

- **alpha-0.2.9 STA/QUI**: typed status/quit validation, precise protocol errors, bounded ISTA → own IQUI → close, safe B/D/E/F status routing and real ncdc rejection/recovery tests. See [STA/QUI](doc/us/ADC_STATUS.md).

- **alpha-0.2.8 Login**: complete guest ADC/ADCS login on the C++ hub, ordered bounded replies, unique nickname/CID admission, public user lists, INF updates and disconnect cleanup. Real ncdc reaches NORMAL and reconnects over both transports. See [guest login](doc/us/ADC_LOGIN.md).

- **alpha-0.2.7 CID/PID**: dedicated Tiger identity verifier, typed public CID/verified result, direct cleansed buffers, constant-time digest comparison and deterministic provider-failure tests. INF uses the shared verifier; Network attachment remains planned.

- **alpha-0.2.6 INF**: bounded client metadata, Tiger PID/CID verification, peer-address and field validation, transactional updates and public snapshots without PD. Composed ClientSession is tested with real ncdc; live transport attachment remains planned.

- **alpha-0.2.5 SID**: typed 20-bit base32 SID codec, bounded shared hub pool, move-only RAII reservations and SidSession composition with SUP/state. Real ncdc fixture uses allocated SIDs; live Network attachment remains planned.

- **alpha-0.2.4 SUP**: bounded feature AD/RM sets, BASE checks, TIGR session-hash selection, deterministic ISUP and composition with ADC State. Tested with real ncdc in an isolated fixture; Network attachment remains planned.

- **alpha-0.2.3 ADC State**: independent hub connection state machine, explicit validation gates, command/direction checks and source-SID binding. Transport wiring and full login remain planned.

- **alpha-0.2.2 ADC Parser**: BASE argument layouts, named-field syntax, bounded decimal integers, feature budgets and TCP/TLS recovery tests.

- **alpha-0.2.1 Project**: [alpha-0.1 audit](doc/us/AUDIT_ALPHA_0.1.md), prioritized alpha-0.2 acceptance plan, version/documentation checks and required CI integrations.

- **alpha-0.1.1 Core**: lifecycle, signal handling, graceful shutdown.
- **alpha-0.1.2 Network**: multiple nonblocking IPv4/IPv6 TCP listeners/clients.
- **alpha-0.1.3 ADC**: bounded stream parser, commands, UTF-8/NFC validation.
- **alpha-0.1.4 ADCS**: optional nonblocking TLS 1.2+ listeners using OpenSSL.
- **alpha-0.1.5 Configuration**: bounded UTF-8 INI, CLI overrides, port/limit/path checks and append-file logging.
- **alpha-0.1.6 Logging**: startup summaries, WARN, opt-in connection events, bounded single-line UTF-8 records and rate budgets.
- **alpha-0.1.7 Security**: bounded per-IP admissions, frame quotas and absolute TLS/unfinished-frame deadlines.
- **alpha-0.1.8 Database**: optional MariaDB connection, private password files, local Unix sockets, prepared queries and explicit migration metadata baseline.

- **alpha-0.1.9 API**: optional local HTTP/JSON status and health routes, private Unix socket, same-UID access and Bearer authentication.

- **alpha-0.1.10 WebAdmin**: responsive React/TypeScript/Vite read-only panel, EN/PL, safe local development adapter, tests and an Apache2 static-preview example.

- **alpha-0.1.11 Apache2**: customizable static WebAdmin HTTPS example, HTTP→HTTPS redirect, TLS 1.2/1.3, certificate paths and isolated HTTPS tests.

Guest login, public user sessions, public chat and direct/echo private chat are implemented. Group chat, registered-user authentication, moderation/history, search, transfer coordination and a production authenticated web gateway remain future milestones.

## Build

```sh
sudo apt-get install build-essential cmake libutf8proc-dev libgcrypt20-dev libssl-dev openssl python3 ncdc libmariadb-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
./build/dc24h --version
```

## Run

```sh
# Guest ADC hub on loopback:
./build/dc24h --listen 127.0.0.1:1511

# TLS only (replace credentials with your own):
./build/dc24h --adcs-listen 127.0.0.1:1512 \
  --tls-cert /etc/dc24h/tls/fullchain.pem \
  --tls-key /etc/dc24h/tls/privkey.pem
```

Use `--language pl` for Polish help and operator messages; `us` is the default.
Add explicit `--listen` options to enable mixed plaintext/TLS service.

Stop with Ctrl+C or SIGTERM. The key must not be committed to Git.
CMake 3.22+, C++20, OpenSSL and utf8proc development packages are required.

Documentation: [English (US)](doc/us/README.md) | [Polski](doc/pl/README.md) | [Module documentation index](doc/README.md) | [CHANGELOG.md](CHANGELOG.md).

License: [GPL-3.0](LICENSE).

## Configuration

```sh
cp config/dc24h.ini.example config/dc24h.ini
./build/dc24h --config config/dc24h.ini --check-config
./build/dc24h --config config/dc24h.ini --max-connections 512
```

Defaults, then explicit INI, then CLI. No file is loaded automatically. INI paths use its directory; CLI paths use the working directory. Use `--log-level info|warning|error|off` and `--log-file PATH` (or `-` for stderr). See [configuration](doc/us/CONFIGURATION.md) for complete precedence, format, limits and logging constraints.

Connection records are off by default; enable with `--log-connections on` or `[logging] connections=on` at INFO. Records use internal IDs and fixed events without client addresses or ADC payloads. See [logging](doc/us/LOGGING.md) for limits and suppression behavior.

Negotiation defaults: 256 pending logins/hub, 64/IP, 65536 plaintext bytes and 64 decoder results until completed login, 64 initial identity attempts/IP/minute and 128/hub/second. Configure these INI `[security]` options for deployment needs.

Terminal output has a 2-second absolute drain deadline (`[security] close_timeout`).

Login/output defaults: 30 seconds for full login, 10 seconds without output progress, 256 KiB queued wire bytes/client and 16 MiB/hub. The two deadlines are INI settings in `[security]`; byte limits are compiled defaults.

Security defaults: 128 active peers/IP, 256 admissions/IP/minute, 1024 decoder results/s/connection, 512 invalid frames/connection, and 10-second TLS/partial-frame deadlines. Frame bodies remain capped at 16 KiB with 128 parameters. Configure `[security]` or CLI; see [security](doc/us/SECURITY.md) for exact policies and limitations.

MariaDB is disabled by default. Configure `[database]` to enable connection checks before listener startup. Use `--check-database` to connect/ping or `--migrate-database` to explicitly create/verify migration metadata; startup never migrates. Install `mariadb-server` to run disposable integration tests. See [database documentation](doc/us/DATABASE.md) for setup, privileges and limits.

The administrative API is disabled by default. Enable `[api]` with a private Unix socket directory and a private 64-hex-character token file. Authenticated `GET /api/v1/health` and `GET /api/v1/status` are read-only on the hub process. The WebAdmin adapter holds the service token outside the browser. The Apache proxy example is local and allowlisted. Public web authentication and remote administrator access remain separate work. See [API setup and contract](doc/us/API.md).

## WebAdmin

```sh
cd webadmin
npm ci
npm run build
npm run dev
```

Requires Node.js `^20.19.0 || >=22.12.0`. Open the local URL printed by Vite. Without API configuration the panel shows unavailable values. For the optional real C++ connection, use the same service UID and server-only socket/token-file path variables; never paste a service token into the browser. See [WebAdmin US](doc/us/WEBADMIN.md) / [WebAdmin PL](doc/pl/WEBADMIN.md) for setup, Apache2 and optional CMake asset installation. Public browser sessions and remote administrator access remain separate work.

An Apache2 HTTPS template is included at [config/apache2-webadmin-https.conf.example](config/apache2-webadmin-https.conf.example) and installed with the configuration examples. Customize domain/certificate/asset paths before enabling it; default access is local and API routes remain denied. The separate [proxy example](config/apache2-webadmin-https-proxy.conf.example) forwards an allowlisted API to the loopback adapter and does not contain the service token. Setup: [HTTPS US](doc/us/APACHE2_HTTPS.md) / [HTTPS PL](doc/pl/APACHE2_HTTPS.md) and [proxy US](doc/us/APACHE2_PROXY.md) / [proxy PL](doc/pl/APACHE2_PROXY.md). No host site or certificate is activated automatically.
