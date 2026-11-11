# Architecture

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
