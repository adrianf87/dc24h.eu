# Network

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

**Increment delivered in alpha-0.3.2:** [Session Manager](SESSION_MANAGER.md) centrally owns all registered lifecycle models, with bounded capacity, stable lookup, phase counts and ordered snapshot pages. Protocol peers borrow models; their queues and public routing indexes remain in ADC. Earlier descriptions retain their historical scope.

**Increment delivered in alpha-0.3.1:** [Session Core](SESSION_CORE.md) provides the independent Users model and stable lifetime IDs, with live readiness/closure/timeline integration. Earlier stage descriptions below retain their original delivery scope.

**Increment delivered in alpha-0.2.10:** [Negotiation security](NEGOTIATION_SECURITY.md) adds pending admission caps, cumulative pre-readiness input budgets and shared pre-INF/Tiger attempt limits. Earlier release descriptions below retain their original scope.

**Increment delivered in alpha-0.2.9:** [STA/QUI](ADC_STATUS.md) adds semantic status/quit validation, source-checked B/D/E/F status routing, precise errors and bounded terminal ISTA/own-IQUI output. Historical descriptions below retain the behavior of their original releases.

**Integration delivered in alpha-0.2.8:** [Guest login](ADC_LOGIN.md) attaches the state/SUP/SID/INF/Tiger stack to the actual C++ TCP/TLS reactor with bounded ordered output, nickname/CID admission, public presence and complete ncdc NORMAL/reconnect tests. Historical stage descriptions below retain their original scope; statements that transport attachment/output/login were still planned describe those earlier versions. Registered-user authentication, chat, search and transfer dispatch remain planned.

`network::TcpServer` uses level-triggered epoll on a single calling thread with nonblocking IPv4/IPv6 TCP sockets. There is no thread per client. Defaults: `127.0.0.1:1511`, 1024 active connections, 120 seconds idle timeout. Explicit listeners replace the default; ADCS-only configuration has no plaintext socket. Configure IPv4 and IPv6 separately: IPv6 uses IPV6_V6ONLY. Only numeric addresses are accepted; DNS and IPv6 zone identifiers are unsupported.

There are at most 16 listeners, shared across TCP and TLS. Port 0 requests an ephemeral port, reported by `Network listening on ...` or `ADCS listening on ...`. All ports must bind before readiness is logged; `Core running` alone does not prove readiness. Wildcard addresses require explicit options.

RAII owns all sockets; SOCK_NONBLOCK and SOCK_CLOEXEC are used. Excess clients are closed once accepted by the application; the kernel TCP handshake may have completed earlier. Counters track accepted/rejected connections and plaintext application bytes. TLS negotiation bytes are excluded. Peer FIN, reset and read errors release only that connection. Idle checks run about every 250 ms; scheduling may add delay. TLS handshake limits are in [ADCS_TLS.md](ADCS_TLS.md).

Each accept batch handles at most 64 clients, each client read turn at most 8 × 8192 bytes. Every ADC decoder buffers at most 16 KiB of unfinished frame body. OpenSSL has additional per-session buffers. Buffered decrypted data is explicitly rescheduled. These budgets preserve stop/peer progress during sustained traffic; they are not DDoS protection or bandwidth limits.

Transient accept errors retry. Descriptor/memory/watch pressure suspends listener watches for one second while existing clients and stop continue. Fatal reactor/listener errors propagate and produce exit code 1 after cleanup. Application limits do not change RLIMIT_NOFILE. Eventfd wakes epoll on stop; listeners close before clients, without waiting for peers or draining application messages. No output queues exist yet.

SIGINT/SIGTERM request idempotent graceful process cleanup and exit 0. SIGPIPE is ignored in main. SIGHUP keeps its default behavior and does not reload configuration. SIGKILL/SIGSTOP cannot be intercepted. ADC parsing produces counters, not responses or routing.

## Configuration source

Since alpha-0.1.5, listener/limit settings can come from INI and CLI. A CLI list replaces its transport while preserving the other explicit list. See [configuration](CONFIGURATION.md). Transport defaults and shutdown behavior are unchanged.

## Logging integration

With opt-in INFO logging, accepted/TLS-ready/close events include only an internal token and transport. Ordinary idle expiry and unfinished TLS handshake expiry have distinct events/categories. Invalid frames, limits, TLS failure and peer I/O warnings use independent five-second budgets; no frame contents or peer addresses enter logs. Existing counters remain unchanged. See [LOGGING.md](LOGGING.md).

## Security — alpha-0.1.7

The Network options include `security::Limits`. A single bounded `PeerLimiter` spans every plaintext/TLS listener, using canonical kernel source addresses kept only in memory. Admission is checked before TLS allocation/watch registration, and active slots are rolled back/released on setup failure or disconnect. Every client has independent frame-rate, invalid-count and absolute partial/TLS timing state. Deadlines are checked before/after I/O and per decoded segment, in addition to periodic expiry scans. INFO shutdown and `statistics()` add security_rejected/security_closed without changing existing counters. See [SECURITY.md](SECURITY.md) for defaults, fixed windows, counter meaning and limitations.

## Administrative socket — alpha-0.1.9

The optional local HTTP API uses a separate private Unix-domain stream socket and worker, not an ADC/ADCS listener. Its active-client/deadline policies are independent of TCP/TLS security counters. No HTTP port is opened and Apache2 is not involved in transport tests. See [API.md](API.md).

## State integration boundary — alpha-0.2.3

The new [ADC State library](ADC_STATE.md) is transport-independent and not yet attached to Client objects. ADC/ADCS wire behavior and syntax counters are unchanged. Attachment requires bounded ordered output, committed-send events, protocol/identity validation and a unique SID allocator; otherwise an inbound-only handshake would reject valid clients while being unable to reply. Existing frame/TLS deadlines remain in Network; a future session deadline must bound complete login independently of keepalives.

## SUP adapter — alpha-0.2.4

The [SUP/state adapter](ADC_SUP.md) completes feature/hash selection as a standalone C++ module. Network still uses the parser alone and sends no SUP response. The new ncdc SUP fixture connects real ncdc to Python-owned temporary sockets/TLS and the C++ module driver; existing ADC/ADCS tests continue to cover the actual C++ reactor. These tests demonstrate different integration boundaries. Attach ProtocolConnection only with bounded output, completed-send events, unique SID allocation, identity/hash computation and login deadlines.

## SID boundary — alpha-0.2.5

[SidSession](ADC_SID.md) now supplies allocator-backed ISID ownership in the independent protocol stack. One shared SidPool must cover every listener of one hub on its reactor thread. The actual Network Client does not attach SidSession yet; the wire path remains receive-only. Integration must commit ISUP, reserve a SID, send/commit ISID, cancel/drain old-session output and remove registry references before releasing a value. A SID may be reused after release; raw socket numbers and copied SID values are not ownership tokens. Bounded output, user registry/QUIT ordering, identity validation and deadlines remain future work.

## INF boundary — alpha-0.2.6

[ClientSession](ADC_INF.md) now validates intrinsic client information in the independent stack. Future Network attachment must pass the actual kernel peer address (not an INF claim), serialize decoded public snapshots with escaping, and enforce global nickname/CID admission and update uniqueness before committing/announcing user information. The receive-only reactor remains unchanged. The new ncdc fixture validates real PID/CID over Python-owned TCP/TLS; existing actual C++ transport fixtures remain separate. Ordered bounded output, registry/lifecycle dispatch and login deadlines are still required.

## CID/PID boundary — alpha-0.2.7

ClientSession/INF now uses the dedicated [Tiger verifier](ADC_IDENTITY.md), but the actual reactor remains receive-only. The new ncdc identity fixture rejects a tampered canonical CID before accepting the real pair, retains only the public result and stays in IDENTIFY. Its sockets/TLS are Python-owned; existing C++ ADC/ADCS transport tests remain separate. Successful PID/CID verification must still be followed by registry uniqueness/admission, bounded output and whole-login lifecycle work before live attachment.
