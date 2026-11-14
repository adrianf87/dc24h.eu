# Error Handling — alpha-0.6.9

**alpha-0.6.9 local validation (2026-11-14):** All 165/165 CTest tests passed in Debug, Release and ASan/UBSan, including new native fault/status/disconnect cases, four real ncdc reset/error/recovery suites and previous positive plaintext/TLS peer handshakes. Final native CTM/RCM fatal-diagnostic allocation checks passed again in all three modes. No compiler warnings. WebAdmin build/typecheck, 12 Node tests, 10 Chromium tests and real C++ API integration passed. Metadata, UTF-8, language parity/local links, changed Python syntax, example INI and whitespace checks passed. GitHub Actions completion is not awaited; the full supported OS matrix and production load remain unverified.

Based directly on main `9efb6020755362d4c9ea537014a36cfbfed145b3` after Connection Security. The numbered branch preserves history and is published separately from main. This increment consolidates connection diagnostics and STA routing, and closes allocation-failure gaps in status handling and terminal output.

## Local diagnostics and peer reports

`adc/connection_errors.hpp` defines allocation-free `connection_failure(error, command)` and a bounded `ConnectionFailure` descriptor. `FailureAction` distinguishes a recoverable source reply, terminal source rejection, and immediate internal abort. Descriptions are static local strings; tokens, protocol names, SIDs, IPs and ports never become diagnostic text.

| Connection error | Local action |
| --- | --- |
| Invalid state / wrong source | Terminal 244 / 240 |
| Recipient unavailable | Recoverable 140, command-specific generic description |
| Inactive CTM source / incompatible RCM roles / invalid message | Recoverable 100 |
| Disabled / forbidden operation | Recoverable 125 |
| Foreign/reused plan, missing sink, unknown or inconsistent native error/command | Immediate source abort without another allocation |

Existing FC values and error precedence remain. A failed preparation has no original target delivery or E echo. No success notification is inferred from queue acceptance. Peer error 41 (unsupported protocol) and 42 (failed direct connection), with either recoverable or fatal severity, retain their checked PR/TO metadata and exact token bytes. Other/unknown status codes keep their earlier validation rules.

The [ADC specification](https://adc.sourceforge.io/ADC.html), sections 3.3, 5.1, 5.3.1 and 5.3.8–5.3.12, separates routed peer context from direct hub status. A routed D/E fatal STA is forwarded in peer context without closing a hub session. A valid fatal HSTA applies to its sending hub connection only; it receives no status reply loop. Successful/recoverable HSTA does not complete login or otherwise grant privileges. A malformed STA still causes the existing terminal protocol rejection.

## Immutable status routing

`adc/status_routing.hpp` adds `StatusRouter`, `StatusDelivery`, `SharedStatusFrame`, `StatusRoutingError` and `StatusDispatchReport`. The router borrows SessionManager on its owner thread. It validates a ready, source-bound B/D/E/F STA, applies the existing public field allowlist, builds one canonical output frame and captures a bounded immutable audience of exact ready lifetime IDs. A frame owns its sanitized Message, canonical wire and typed StatusCode. Codec consistency validation can also serialize temporary values during preparation.

D selects the exact target; E selects target then source, with a self-target appearing once. B selects ready users in increasing lifetime order. F selects the same ready population using public INF SU, with required/excluded selectors; SUP is independent. Recipient storage is bounded by current public membership and configured session/user limits. Payload/encoded limits remain those of the existing status codec.

Before every sink call, dispatch rechecks source readiness/ownership, recipient readiness and current F membership. It never adds a newly matching or replacement login. A departed target's old SID cannot retarget a prepared plan to a new lifetime. Losing F membership cancels a captured copy; gaining membership does not expand that audience. Missing D/E targets at admission return the existing source-only `ISTA 140 Status\srecipient\sunavailable`, without an original echo.

Copies share one attempt. Foreign-domain or missing-sink errors do not consume it; valid dispatch consumes before rechecks or callbacks. Cancellation is shared and idempotent. Captured immutable data/frame/attempt survive callback replacement of the caller's delivery object. Sink exceptions propagate with the attempt consumed, preventing replay after partial queue insertion. Dispatch and frame access allocate nothing after preparation and sink construction. Reports count queued/refused/canceled copies, not network delivery or successful transfer.

## Failure isolation and disconnects

LoginHub queues shared STA frames with independent offsets and full logical byte charges, using existing per-client/global bounds. A target queue refusal or allocation failure affects that recipient; an eligible E source echo remains independent. Preparation/validation/diagnostic allocation failure affects the sender. A source that stops being ready during dispatch cancels its remaining captured copies.

`receive` now encloses STA/CTM/RCM admission, validation and terminal diagnostics in a bad_alloc boundary. Explicit `close` also isolates terminal-notice allocation failure. STA validation during completed output is recipient-local. These paths force-close only the affected lifetime when a diagnostic cannot safely be allocated; no recursive error allocation or automatic retry occurs. This is targeted isolation, not a claim that every unrelated server allocation is contained.

Existing terminal behavior remains: preserve a partial/TLS-pinned front, cancel later ordinary output, attempt bounded terminal STA then own QUI when possible, and finalize identity/SID/queues exactly once on actual disconnect. EOF, reset, unusable transport, exhausted output or memory may prevent notice delivery. Shared frames remain valid for other recipients after one connection is removed. Graceful TLS close_notify/reload is still future work.

Connection security remains scoped to CTM/RCM; peer error reports can still explain a failure when a connection operation is denied. The hub does not infer a pending transfer from TO, manufacture a peer failure after a socket event it cannot observe, downgrade ADCS, retry a CTM/RCM or operate peer sockets/file bytes. No new dependency, INI key, SQL migration, endpoint, metric or WebAdmin control is added. Error payloads stay outside logs, SQL, API/WebAdmin and Chat/Search counters.

## Verification

`transfers.errors` checks diagnostic actions/codes, all STA scopes and severities, immutable ownership/allowlisting, self echo, fixed audiences, foreign/canceled/copied attempts, no-allocation dispatch, F changes, forced SID reuse, callback replacement and exception consumption. A one-shot allocation sweep over 400 LoginHub cases covers source preparation, target queue insertion and successful routing while preserving bystanders. Additional cases cover outgoing STA validation, failed terminal diagnostics, pinned fronts, output caps and complete queue/identity cleanup.

`transfers.errors.process` covers fragmented IPv4/IPv6 mixed ADC/ADCS CTM/RCM and D/E STA 141/142/241/242/unknown errors, preserved metadata, continued hub NORMAL, HSTA with no reply loop, own fatal departure, malformed metadata, reconnect/old SID rejection, TCP reset, recovery and API/log privacy. Earlier broad STA/QUI, timeout and TLS/backpressure tests remain enabled.

Four real ncdc suites (`adc.transfererrors.ncdc`, `adc.transfererrors.adcs.ncdc`, `adcs.transfererrors.ncdc`, `adcs.transfererrors.adcs.ncdc`) accept a real peer connection, observe CSUP or TLS ClientHello, reset it, verify a real unsupported-protocol 141 with its opaque token, then forward a peer 242 and prove chat/quit still work. ncdc 1.23.1 may handle a failed socket locally without sending a hub DSTA; the harness does not require an invented 142. Raw peers separately exercise 142/242 relay. TLS peer success/certificate verification remains covered by earlier suites; the new TLS case deliberately fails before a successful peer handshake.

See [STA/QUI](ADC_STATUS.md), [Connection Routing](CONNECTION_ROUTING.md), [Connection Security](CONNECTION_SECURITY.md), [Transfer Tokens](TRANSFER_TOKENS.md), [User Disconnect](USER_DISCONNECT.md) and [Testing](TESTING.md).
