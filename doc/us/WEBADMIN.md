# WebAdmin — alpha-0.4.12

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

**Current increment — alpha-0.6.13:** [Transfer Configuration](TRANSFER_CONFIGURATION.md) adds an exact bounded `allowed_protocols` startup policy, shared Core CTM/RCM enforcement and read-only effective limits/protocols in the authenticated API and bilingual Connections page. Defaults preserve existing forwarding; changes require restart.

**Increment delivered in alpha-0.6.12:** [WebAdmin Transfer Monitoring](WEBADMIN_TRANSFERS.md) adds observed CTM/RCM, refusal and queue-copy rates, a bounded chart and accessible interval table on Connections. Browser-local observations reset after failures, detected restarts or long gaps; shared polling, pause/manual refresh, local clear and EN/PL support preserve cumulative statistics and credential isolation.

**Increment delivered in alpha-0.6.11:** [Transfer Statistics](TRANSFER_STATISTICS.md) adds fixed aggregate CTM/RCM outcome, queue/error and peer-report counters, the authenticated API extension and bilingual Connections monitoring. [Debian 13 CI](CI_DEBIAN13.md) adds a sequential custom-runner Debug/Release matrix and Node 24 action runtimes while retaining Node 22 for WebAdmin.

**Current increment — alpha-0.6.5:** [Protocol Negotiation](PROTOCOL_NEGOTIATION.md) gives Transfer Core owning protocol values and explicit ADC/1.0 plaintext / ADCS/1.0 TLS intent. Pure preference/capability selection never downgrades required TLS. Live CTM/RCM preserve the selected name and extension compatibility; peer failure replies retain opaque tokens. Real ncdc tests cover certificate-checked TLS peer handshakes over both hub transports, alongside the terminal-backpressure regression.

**Increment delivered in alpha-0.6.4:** [Connection Routing](CONNECTION_ROUTING.md) consolidates D/E CTM and RCM admission and delivery into one SID router, one immutable packet type and one bounded queue path. Recipients are fixed ready-session lifetimes: D targets only, E targets then independently echoes to the source. Tokens never select recipients; existing roles, wire validation, errors and privacy remain. Mixed-pair tests use the same token without crossing audiences.

**Increment delivered in alpha-0.6.3:** [ADC RCM](ADC_RCM.md) adds owning D/E reverse-request validation and live target/echo routing through Transfer Core, with per-family passive-source/active-target checks, stable lifetime/address admission and shared bounded queues. CTM/RCM share wire-field validation. Real ncdc covers RCM→CTM and peer handshakes in both roles; no pending-token registry or file-data handling is claimed.

**Increment delivered in alpha-0.6.2:** [ADC CTM](ADC_CTM.md) adds owning D/E CTM validation and live target/echo routing over Transfer Core, with checked source TCP activity, lifetime/endpoint rechecks, shared bounded queues and real ncdc client-handshake interoperability. Protocol/port/token/extensions retain their checked spelling and privacy. RCM and file-data handling remain later stages.

**Increment delivered in alpha-0.6.1:** [Transfer Core](TRANSFER_CORE.md) adds an independent C++20 connection-coordination engine: bounded owning intents, active/passive IPv4/IPv6 roles and immutable session-bound one-attempt plans with cancellation and dispatch rechecks. Shared Users address classification preserves Search behavior. Live CTM/RCM routing and file data transfer remain future work.

**Increment delivered in alpha-0.5.13:** [WebAdmin Search Monitoring](WEBADMIN_SEARCH.md) adds browser-local query/result/refusal/queue rates, a bounded traffic chart and accessible recent-interval table. First samples, gaps, detected resets and failures start fresh baselines; pause/manual refresh and local clear preserve cumulative counters. Existing API/settings and credential isolation remain.

**Increment delivered in alpha-0.5.12:** [Search Configuration and Statistics](SEARCH_CONFIGURATION.md) exposes 19 cumulative hub SCH/RES counters, a lazy stored-token-window gauge and all 16 effective startup search settings through the authenticated read-only API and bilingual WebAdmin Search page. Queue acceptance is distinct from delivery; direct client UDP is excluded. No payload/history or configuration write endpoint is added.

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

Implemented: a React 19 / TypeScript / Vite project in `webadmin/`, with a responsive read-only panel, EN/US and Polish catalogs, five hash-based views (overview, connections, chat, security, database), manual/5-second refresh, bounded in-session observations and JSON snapshot export. Configuration and event-log controls are explicitly disabled as future work. No account/login/role UI, mutations, persistent metrics or fabricated dashboard values are included.

The default panel shows disconnected/loading/error states and dashes for unavailable counters. Only a validated API response creates live values; failed refreshes clear the displayed snapshot and chart. The last-received timestamp identifies the most recent successful sample. A database flag reports startup configuration, not live SQL health. Network counters are independent snapshots; observations are limited to the last 30 successful samples in browser memory and reset after detected server uptime decrease. ADC guest login is implemented; account authentication and browser administrative sessions remain separate future work.

## Requirements and build

Use Node.js `^20.19.0 || >=22.12.0` and npm with package-lock v3 support. The checked-in lock pins dependencies; `npm ci` is the reproducible install. Default C++ builds do not require Node, npm or browser packages. Files are UTF-8; language catalogs are under `webadmin/src/languages/us` and `pl`. English is the default; the language preference alone is saved in localStorage (with a safe fallback if storage is blocked).

```sh
cd webadmin
npm ci
npm run build
npm run dev
```

Vite binds `127.0.0.1:5173` with a strict port, CORS disabled and restricted file serving. Without adapter configuration, the panel opens with unavailable values. It never asks the operator to paste an API token into the browser. `npm run preview` binds `127.0.0.1:4173` and serves the production assets, with the adapter intentionally disabled. Stop preview/dev with Ctrl+C; these commands are local development tools, not a production service.

## Optional local development adapter

Browsers cannot connect to Unix sockets. The Vite development plugin can broker a small, read-only API allowlist through Node's HTTP Unix-socket client. Start the C++ hub with its private API enabled as documented in [API.md](API.md), then run Vite as the **same effective UID**. Keep the socket directory private and owned by that UID, and use the existing private 64-hex-character token file. Both paths must be absolute; the token must be outside the WebAdmin root and cannot be a final-component symlink.

```sh
cd webadmin
DC24H_API_SOCKET=/absolute/private/runtime/admin.sock \
DC24H_API_TOKEN_FILE=/absolute/private/runtime/admin.token \
  npm run dev
```

These server-only variables carry paths, never token values. Do not use a `VITE_*` variable for any secret. The adapter reads the private token server-side at startup, validates ownership/permissions/link count/format and rejects token files inside the served root. Restart Vite after rotation. The browser uses relative `./admin-api/v1/status`, with no authorization token in requests, JavaScript, URLs or browser storage. Vite file-serving rules also deny token/password/key/certificate files and `.env`/Git metadata.

Only GET `/admin-api/v1/status` and `/admin-api/v1/health` are forwarded to the corresponding C++ `/api/v1/` endpoints. Other methods/paths, request bodies, non-loopback peers, unapproved Host values, foreign Origin and cross-site Fetch Metadata are rejected. Incoming Authorization is never trusted or forwarded; the server injects its own Bearer header. The adapter caps active upstream requests at 8, enforces a 2-second absolute deadline and a 16-KiB response cap, and returns fixed sanitized errors. It validates and normalizes upstream status JSON, dropping unknown fields. The browser independently validates the schema and safe nonnegative JavaScript integer counters, with a 4-second deadline and the same response cap. Very large counters beyond `Number.MAX_SAFE_INTEGER` are rejected rather than rounded silently.

The plugin is a **local development adapter**, not production web authentication. Local machine access remains trusted; it does not provide browser sessions, user identities, roles, CSRF protection for future writes, TLS termination, request audit or a cross-UID Apache gateway. Do not expose Vite to a network or disable its host/file restrictions. No C++ socket permission weakening is needed.

## Layout and behavior

The graphite sidebar, light workspace and blue controls form a small reusable layout. Icons are bundled from Lucide; no remote scripts/fonts/images are fetched. The panel has responsive navigation, visible keyboard focus, a skip link, semantic tables/status, named controls and reduced-motion handling. EN/PL changes labels and document language; protocol identifiers remain unchanged. Export is enabled only for a current validated snapshot and contains the allowlisted fields plus `observed_at`, never credentials or settings. Documentation links follow the selected language.

## Apache2 and optional installation

`npm run build` emits static assets in `webadmin/dist`, using relative asset paths and hash navigation. No Node server, token or development plugin is bundled into production assets. To include the prebuilt assets in a CMake installation:

```sh
npm --prefix webadmin ci
npm --prefix webadmin run build
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DDC24H_INSTALL_WEBADMIN=ON
cmake --build build-release --parallel 2
sudo cmake --install build-release --prefix /usr/local
```

This installs assets under `share/dc24h/webadmin` and a sample `etc/dc24h/apache2-webadmin.conf.example`. The option defaults OFF; ON requires an existing `webadmin/dist/index.html`. Always rebuild the frontend from the same checkout before installation; CMake does not execute npm or verify the provenance of prebuilt assets. Installation never activates a configuration or starts a service.

The [Apache2 example](../../config/apache2-webadmin.conf.example) is a **static loopback preview** at `127.0.0.1:8080`, with local-only access, disabled directory indexes, CSP/no-sniff/frame/referrer headers and explicit denial of `/admin-api/`. It requires mod_headers plus ordinary directory/authorization modules and a readable installed asset directory. Adjust paths/ports for your own installation and validate with `apache2ctl configtest` before explicitly enabling it. It deliberately does not forward the private API. The static panel therefore has no live data in that example. A production authenticated gateway, web sessions/roles, deployment remain future work; never ship the service token to browser JavaScript or expose the C++ private socket to another UID.

## Verification

```sh
cd webadmin
npm run build
npm test
npx playwright install --with-deps chromium
npm run test:e2e
npm run test:integration -- ../build/dc24h
```

On a workstation with Chromium already installed, set `PLAYWRIGHT_CHROMIUM_EXECUTABLE=/usr/bin/chromium` for browser/integration checks. Tests use disposable credentials/sockets. Unit tests cover schema/precision, normalization, private credentials, origin/host/method/path restrictions, token injection, overload, size/time bounds and sanitized failures. Browser tests cover missing/live/malformed/lost data, navigation, locale persistence, export and mobile overflow. The integration harness starts a disposable real C++ hub plus a separate Vite process and Chromium, verifies actual ADC counters and token isolation, checks desktop/mobile PL and cleans up all processes. Existing CTest/ncdc/MariaDB suites remain separate.

References: [Vite setup](https://vite.dev/guide/), [Vite server restrictions](https://vite.dev/config/server-options), [React with TypeScript](https://react.dev/learn/typescript).

## HTTPS example — alpha-0.1.11

A separate HTTP→HTTPS/TLS 1.2+ static-site template is now shipped and always installed with the C++ configuration examples. It keeps local access and API denial by default; it is not a production authenticated gateway. See [APACHE2_HTTPS.md](APACHE2_HTTPS.md) for certificate paths, customization, verified activation and automated tests.

## API security — alpha-0.9.7

Proved access still stores the grant, the session id and the CSRF secret in page memory. Every maintenance call sends all three. A read with only the grant is refused before the Unix socket. No new page is added. See [API security](API_SECURITY.md).

## Account security — alpha-0.9.6

WebAdmin is unchanged. Password enrollment and GPA/PAS secret handling stay inside the hub and the trusted C++ provisioning API. No new page, grant or status field is added. See [Account security](ACCOUNT_SECURITY.md).

## TLS security — alpha-0.9.5

WebAdmin is unchanged. The certificate and cipher policy runs inside the hub process. No new page, grant or status field is added. See [TLS security](TLS_SECURITY.md).

## Traffic limits — alpha-0.9.4

WebAdmin is unchanged. The presence budget runs inside the hub process. No new page, grant or status field is added. See [Traffic limits](TRAFFIC_LIMITS.md).

## Network security — alpha-0.9.3

WebAdmin is unchanged. TCP backlog, kernel buffer caps and the TCP user timeout run inside the hub process. No new page, grant or status field is added. See [Network security](NETWORK_SECURITY.md).

## ADC security — alpha-0.9.2

WebAdmin is unchanged. Parser validation and protocol control run inside the hub process. No new page, grant or status field is added. See [ADC security](ADC_SECURITY.md).

## Threat model — alpha-0.9.1

[Threat model](THREAT_MODEL.md) records what the browser, the loopback adapter and the local Apache examples are trusted to do. Page memory still holds a maintenance grant only after a proof. No new page or control is added.

## Security sessions — alpha-0.8.15

Proved access stores the grant, the session id and the CSRF secret in page memory. Every maintenance call sends all three. Forget clears all three. None of them is written to web storage. See [API security](API_SECURITY.md).

## HTTPS reverse proxy — alpha-0.8.14

A second installed example proxies an allowlisted `/admin-api/v1` surface to the loopback adapter and strips client bearer tokens, cookies and forwarded identity. The service token stays in the adapter. The C++ Unix socket is not an Apache upstream. Access remains local. See [APACHE2_PROXY.md](APACHE2_PROXY.md).
