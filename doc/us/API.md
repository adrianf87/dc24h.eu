# Local administrative API — alpha-0.1.9

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

**Current increment — alpha-0.7.2:** [Database Migrations](DATABASE_MIGRATIONS.md) adds an ordered compiled SQL catalog, exact checksums, read-only `--database-status`, preserved v1 upgrade and durable applying/applied history for future non-bootstrap changes. Migrations remain explicit; no application schema or automatic DDL replay.

**Increment delivered in alpha-0.7.1:** [Database Core](DATABASE_CORE.md) adds typed prepared values, bounded owning results, write metadata and thread-owned RAII transactions, with redacted errors, rollback-only failure handling and connection retirement without replay. Existing local-socket credentials/migrations and Ubuntu-only CI remain.

**Increment delivered in alpha-0.6.13:** [Transfer Configuration](TRANSFER_CONFIGURATION.md) adds an exact bounded `allowed_protocols` startup policy, shared Core CTM/RCM enforcement and read-only effective limits/protocols in the authenticated API and bilingual Connections page. Defaults preserve existing forwarding; changes require restart.

**Increment delivered in alpha-0.6.12:** [WebAdmin Transfer Monitoring](WEBADMIN_TRANSFERS.md) adds observed CTM/RCM, refusal and queue-copy rates, a bounded chart and accessible interval table on Connections. Browser-local observations reset after failures, detected restarts or long gaps; shared polling, pause/manual refresh, local clear and EN/PL support preserve cumulative statistics and credential isolation.

**Increment delivered in alpha-0.6.11:** [Transfer Statistics](TRANSFER_STATISTICS.md) adds fixed aggregate CTM/RCM outcome, queue/error and peer-report counters, the authenticated API extension and bilingual Connections monitoring. [Debian 13 CI](CI_DEBIAN13.md) adds a sequential custom-runner Debug/Release matrix and Node 24 action runtimes while retaining Node 22 for WebAdmin.

**Current increment — alpha-0.6.5:** [Protocol Negotiation](PROTOCOL_NEGOTIATION.md) gives Transfer Core owning protocol values and explicit ADC/1.0 plaintext / ADCS/1.0 TLS intent. Pure preference/capability selection never downgrades required TLS. Live CTM/RCM preserve the selected name and extension compatibility; peer failure replies retain opaque tokens. Real ncdc tests cover certificate-checked TLS peer handshakes over both hub transports, alongside the terminal-backpressure regression.

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

Implemented: an opt-in, read-only HTTP/1.1/JSON API on a Linux Unix-domain stream socket. It is independent of ADC/ADCS and does not expose a TCP port. This is an administrative API foundation, not a complete WebAdmin backend. No user management, role system, mutations, configuration updates, SQL queries, remote HTTP/TLS endpoint or production browser authentication is implemented.

## Access and configuration

Every request requires both a connection from the process effective UID (`SO_PEERCRED`) and `Authorization: Bearer TOKEN`. The token is exactly 64 hexadecimal characters (generate 32 random bytes), read once at startup from a private regular file. It is an opaque service credential, not a hub user password or role. Token comparison uses `CRYPTO_memcmp` for equal-length credentials. Changing the file requires a restart; there is no token reload/revocation service. Never put the token in source, INI values, URLs or logs. The server has no token CLI/environment option.

| INI key under `[api]` | Default / contract |
| --- | --- |
| `enabled` | `off`; enabling requires `socket` and `token_file` |
| `socket` | absent; pathname only, up to 107 bytes, final component up to 64 bytes |
| `token_file` | absent; file path, relative to the INI directory |
| `max_connections` | 16; range 1..128 active API clients, independent of ADC limits |
| `request_timeout` | 2; range 1..30 seconds, absolute accept-to-completion deadline |

INI socket paths resolve against the INI directory and must then be absolute. The socket's immediate parent must already exist, be owned by the effective UID and have no group/other permissions (normally 0700). Its final directory component cannot be a symlink. Parent directories must remain trusted. Socket creation is anchored through `/proc/self/fd` to the checked directory without changing process umask/current directory. The socket is set to 0600 before listening. No group access or cross-UID gateway is supported yet, including Apache workers running as a different user. Do not weaken permissions to connect Apache; a separately designed authenticated gateway is future work.

The token file follows the shared `security::read_private_secret` contract: effective-UID owner, regular file, no group/other permission bits, exactly one hard link, no final symlink, 1..4096 bytes before token-format validation. A single terminal LF/CRLF is removed. Directories/FIFOs, NUL/control characters, empty/oversized files and malformed tokens fail startup. Temporary read buffers and the stored application token are wiped when released; request/connector/OS copies are not claimed to be fully erased. Database password behavior is preserved through the same shared reader.

Disabled mode opens no API resources and does not read the token. `--check-config` checks settings and path collisions, but does not read API tokens, inspect runtime directory permissions or create sockets. Database check/migration modes also never start the API. Normal API startup failure returns code 1 before ADC listeners are opened; syntax/path collisions return code 2. Logs cannot target the token or API socket, including existing hard-link and symlink aliases. The socket cannot target the configuration, TLS credentials, database password/socket, token or configured log.

## Local example

Run these commands as the same dedicated account that will run dc24h; provision writable trusted directories first. The token commands create/replace a credential, so run them during initial setup or a deliberate rotation.

```sh
install -d -m 0700 .runtime
install -m 0600 /dev/null .runtime/admin.token
openssl rand -hex 32 > .runtime/admin.token
```

Add to `config/dc24h.ini` (replace an existing `[api]` section, do not duplicate keys):

```ini
[api]
enabled = on
socket = ../.runtime/admin.sock
token_file = ../.runtime/admin.token
max_connections = 16
request_timeout = 2
```

Then start with `./build/dc24h --config config/dc24h.ini`. Use curl's header input from stdin to avoid putting the credential in its command-line arguments:

```sh
{ printf 'Authorization: Bearer '; cat .runtime/admin.token; } |
  curl --silent --show-error --fail --unix-socket .runtime/admin.sock \
    --header @- http://localhost/api/v1/status
```

This is HTTP over a local Unix socket; `localhost` is the HTTP Host value, not a TCP connection. No token is sent to an external service. Curl is optional for manual use, not a build dependency.

## Versioned endpoints

Both routes require authentication. JSON keys/protocol errors remain EN/US regardless of operator language.

| Method / path | Result |
| --- | --- |
| `GET /api/v1/health` | 200 `{"status":"ok"}`: the API worker can process this request; not a database or full ADC readiness probe |
| `GET /api/v1/status` | 200: bounded snapshot below |

```json
{
  "api_version": "v1",
  "server": {"name": "dc24h.eu", "version": "alpha-0.4.4", "core_running": true, "uptime_seconds": 3},
  "network": {"active_connections": 0, "accepted_connections": 1, "adc_frames": 1, "adcs_handshakes": 0, "security_rejected": 0, "security_closed": 0, "rejected_connections": 0, "closed_connections": 1, "received_bytes": 20, "sent_bytes": 32, "peak_connections": 1},
  "users": {"online": 0, "peak_online": 0, "successful_logins": 0},
  "database": {"enabled": false}
}
```

Counters are independently sampled atomics, not one transactional snapshot. `core_running` reports the Core lifecycle only, not complete ADC login/readiness. Uptime uses a steady clock from transport/API composition after optional database setup; it is not wall-clock process age. `database.enabled` reports configured startup connection presence, not ongoing SQL availability. No request runs SQL. Peer addresses, nicknames, frames, file paths, secrets and administrative settings are never returned. Unknown paths (including query strings) give 404 after authentication; other methods give 405 with `Allow: GET`. HEAD is unsupported (405), with no response body.

Missing/incorrect authentication gives 401 and `WWW-Authenticate: Bearer realm="dc24h-admin"` before route/method lookup. Error bodies are fixed JSON such as `{"error":"unauthorized"}`, `not_found`, `method_not_allowed`, `bad_request` and `headers_too_large`; no input or exception text is echoed. The token's length/format is not secret, and no remote brute-force resistance is claimed for processes already running as the service UID.

## HTTP subset and resource limits

The parser requires HTTP/1.1, origin-form paths, CRLF endings, one nonempty Host and ASCII header syntax. Header names are case-insensitive; duplicate names, whitespace before a colon, folded lines, controls and non-ASCII bytes are rejected. There are at most 8192 request bytes, 32 headers and 1024 bytes in the request line. Oversized headers give 431; malformed requests give 400. The maximum-size complete header remains valid. Content-Length is absent or exactly `0`; every Transfer-Encoding, Expect, Upgrade, nonzero/ambiguous length or already received trailing body/pipelined data is rejected. If extra bytes arrive after the first request was dispatched, the connection is closed without dispatching another request. There is no keep-alive, body parser, compression, CORS or HTTP upgrade. One connection serves at most one request and always closes.

Responses carry Content-Length, JSON UTF-8 content type, Date, `Connection: close`, `Cache-Control: no-store` and `X-Content-Type-Options: nosniff`. Header/body buffering is bounded; no user-controlled strings are reflected. The worker uses nonblocking sockets and one bounded poll loop. Each peer gets at most one read/write attempt per pass; accept processing is limited to 16 attempts per pass. Excess clients or different-UID peers are closed without an HTTP response. The absolute request/response deadline never slides when more bytes arrive; expiry closes the peer. There is no API admission rate limiter or DDoS claim. Trusted callbacks and synchronous logs must not block indefinitely.

## Ownership, shutdown and failures

`api::Server` owns one worker thread and socket resources; `start()` is single-use. Constructor reads the token but does not bind. Destructor requests worker stop, joins it, closes peers/listener and removes only the recorded socket inode through the retained directory fd. It will not remove a replacement file/symlink/socket. Existing endpoints, including stale sockets, are never overwritten or unlinked at startup: stop any existing process and remove a verified stale socket manually. SIGKILL cannot run cleanup.

Main constructs API after Core/Network composition, with thread-safe status and failure callbacks. The API worker never owns Core or invokes a lifecycle mutation from HTTP. SIGINT/SIGTERM and Network-start failures stop/join the API before dependencies are destroyed. A fatal API infrastructure/provider exception logs only `Local API worker failed`, invokes the owner stop callback and makes process exit fail. Per-client malformed input, deadlines and I/O failures only close that peer. Startup/worker logs omit requests, headers, token and paths; there is no request audit log yet.

References: [RFC 9112 HTTP/1.1 framing](https://www.rfc-editor.org/rfc/rfc9112.html), [RFC 9110 HTTP semantics](https://www.rfc-editor.org/rfc/rfc9110.html), [Linux Unix sockets and peer credentials](https://man7.org/linux/man-pages/man7/unix.7.html).

## Internal C++ interfaces

`core::Server(Logger&, TcpServer* = nullptr)` uses non-owning dependencies. `run()` is blocking and single-use; `request_stop()` is thread safe and idempotent, including before startup; `state()` returns the mutex-protected lifecycle state. Network exceptions enter `stopped` before propagating.

`network::Options` includes plaintext/TLS endpoint vectors, TLS credential paths, maximum connections and idle timeout. `parse_endpoint` accepts numeric IPv4:port or bracketed IPv6:port. `TcpServer` validates limits in its constructor, then opens listeners in `run()`. Stop wakes eventfd without closing sockets from another thread. `statistics()` independently samples atomic active/accepted/rejected/received_bytes, ADC counters and ADCS handshake/incomplete counters; it is not a transactional snapshot.

`adc::parse_line`, `recognize_command`, `Message`, `ParseResult` and `StreamDecoder::feed/finish/buffered_bytes` are defined in `dc24h/adc/parser.hpp`. See [ADC_PROTOCOL.md](ADC_PROTOCOL.md) for ownership, limits and callback semantics.

`adcs::Context` owns the credential-bearing TLS context. `adcs::Connection` owns a session referencing that context and a non-owning fd. Both dependencies must outlive the connection. `handshake()` and `receive()` return `IoResult {status, bytes}` with complete/want_read/want_write/closed/error; `pending()` reports buffered decrypted bytes. Socket readiness and SIGPIPE disposition are the caller's responsibility. No send API exists yet.

All owners must stop and join callers before destroying dependencies. `Logger(Options = {})` and `i18n::parse_language/translate/help` provide built-in messages. Future administrative mutations and Apache integration need role checks and audit records beyond the local read-only foundation.

## Configuration and Logger

`configuration::Settings` contains network/logging/database/API options and parsing provenance (`source_file`, `implicit_plain_listener`). `load_file` parses syntax and individual fields, but does not validate all cross-field requirements. Call `validate(Settings)` on composed settings. `parse_arguments(argc, argv)` loads the explicit file, applies CLI and validates; `CommandLine` adds check/help/version flags. `logging::Options` contains level, file_path (empty = stderr), language and connections. `Logger(Options = {})` opens a file at construction; `Logger(Language)` remains available. The logger is noncopyable and must outlive callers. There is no mutation/reload API.

## Logger — alpha-0.1.6

`Level` includes info/warning/error/off. `info`, `warning` and `error` accept internal non-secret text and never throw. `connection(ConnectionEvent, uint64_t id, bool secure)` accepts only typed lifecycle values, preventing arbitrary event/payload strings. `connection_enabled()` reports the immutable opt-in and INFO threshold. A shared 64-event/second-window budget and destructor/rollover suppression summary are protected by the logger mutex. `Options` order is level, file_path, language, connections, preserving earlier aggregate initializers. Complete record bounds and limitations: [LOGGING.md](LOGGING.md).

## Security — alpha-0.1.7

`network::Options::security` holds six validated `security::Limits` fields. `security::validate` rejects out-of-range values. `PeerLimiter(Limits)` is reactor-confined: `admit(canonical_key, steady_time)` grants an active slot and minute admission; call `release(key)` exactly once on every granted slot after failure/close. Key length and map size are bounded; `tracked_peers()` exposes the current map count. `FrameLimiter(maximum)::admit(steady_time)` provides a connection-local fixed second budget. Caller times must be monotonic. `Statistics` adds independent atomic security_rejected/security_closed counts. No limiter state is shared between server instances/processes. Details: [SECURITY.md](SECURITY.md).

## WebAdmin foundation — alpha-0.1.10

The read-only React panel and optional same-UID local Vite development adapter now exist; see [WEBADMIN.md](WEBADMIN.md). The private C++ API contract and permissions are unchanged. Static Apache serving does not provide an authenticated API gateway.

Since alpha-0.2.2, `Message::positional_count` identifies known BASE argument layouts (unknown commands remain opaque). `parse_named_parameter` returns borrowed name/value views; `parse_integer` returns optional signed 64-bit values. The same parse_line/StreamDecoder now reject malformed BASE argument syntax; see the complete [parser contract](ADC_PROTOCOL.md).

## ADC state API — alpha-0.2.3

`dc24h/adc/state.hpp` exposes `ConnectionStateMachine`, `ConnectionState`, `Direction`, `IdentityMode`, `StateError` and `StateResult`. `observe`, `keepalive`, `approve_protocol`, `approve_identity`, `approve_password` and terminal `disconnect` are detailed in [ADC_STATE.md](ADC_STATE.md). Results contain fixed error categories, never peer payloads. These are C++ module APIs; no HTTP routes/counters or live session enforcement are added.

## SUP negotiation API — alpha-0.2.4

`dc24h/adc/sup.hpp` exposes SupportPolicy, SupportNegotiation, SessionHash, SupportError and ProtocolConnection. The adapter returns optional `variant<StateError, SupportError>`; all rejected events preserve state/support. It exposes only const negotiated metadata and no manual protocol approval. Initial reply Message/string values own their data. No HTTP API routes or schema/counters change. See [ADC_SUP.md](ADC_SUP.md) for lifetimes, limits, errors and intended transport integration.

## SID API — alpha-0.2.5

`adc/sid.hpp` exposes Sid value parsing/encoding, SidPool and move-only SidLease. `adc/sid_session.hpp` composes pool ownership with protocol support/state in SidSession; it returns optional `variant<StateError, SupportError, SidError>`. Only the reserved SID may be committed. Preparation/encoding does not advance the protocol; completed-send observation does. Pool counters are reactor-confined, not HTTP telemetry. No API HTTP routes/schema change. See [ADC_SID.md](ADC_SID.md) for lifetime, capacity and release ordering.

## INF API — alpha-0.2.6

`adc/identity_hash.hpp` supplies Tiger/192 and canonical 39-symbol base32 helpers. `adc/inf.hpp` supplies InfValidator, read-only ClientInfo and ClientSession composition. Snapshots expose optional borrowed field views, checked integer values and an owning public BINF Message without PD. `observe` returns optional `variant<StateError, SupportError, SidError, InfError>`; failed input preserves state/metadata/reservations. Identity approval additionally requires validated INF but still delegates registry/admission and account decisions. HTTP routes/schema/counters remain unchanged. See [ADC_INF.md](ADC_INF.md).

## CID/PID API — alpha-0.2.7

`adc/tiger_identity.hpp` exposes TigerCid::parse/text/bytes and verify_tiger_identity(cid, pid, hash), returning variant<VerifiedIdentity, IdentityError>. Only public CID is retained. `ClientInfo::identity()` returns an optional owning copy of that result. `decode_tiger_base32` and `tiger192_into` write a caller-owned 24-byte destination and clear it on failure; existing optional-return helpers remain compatible. The final comparison uses CRYPTO_memcmp. No HTTP route/schema or authentication API changes. See [ADC_IDENTITY.md](ADC_IDENTITY.md).


## alpha-0.2.8

In alpha-0.2.8 the API v1 schema was unchanged (alpha-0.3.13 adds the fields in [Statistics](STATISTICS.md)). active_connections counts transport sockets, adc_frames counts parsed syntax; neither represents logged-in users. Guest login/readiness is internal reactor state and opt-in logging. See [ADC_LOGIN.md](ADC_LOGIN.md).

Current additive status fields, sampling and compatibility: [Statistics](STATISTICS.md).
