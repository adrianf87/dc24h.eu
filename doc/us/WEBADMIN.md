# WebAdmin — alpha-0.4.12

**Current increment — alpha-0.6.11:** [Transfer Statistics](TRANSFER_STATISTICS.md) adds fixed aggregate CTM/RCM outcome, queue/error and peer-report counters, the authenticated API extension and bilingual Connections monitoring. [Debian 13 CI](CI_DEBIAN13.md) adds a sequential custom-runner Debug/Release matrix and Node 24 action runtimes while retaining Node 22 for WebAdmin.

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
