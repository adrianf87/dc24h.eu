# Message Routing — alpha-0.4.5

**Current increment — alpha-0.4.6:** [Message Flags](MESSAGE_FLAGS.md) centralizes PM/ME decoding and canonical output for the codec and both chat adapters. Absent ME, ME0 and ME1 remain distinct; PM never grants delivery authority.

Based directly on main `0c627eb1430ab0c750674a8463e64060c96c8a86`, after the alpha-0.4.4 Private Chat merge. Existing alpha-0.4.1–alpha-0.4.4 branches and history are retained. The server and code use EN/US UTF-8; documentation and operator catalogs also support Polish.

## Scope and ownership

`adc::MessageRouter` in `adc/message_routing.hpp` and `message_routing.cpp` is the common admission/dispatch entry point for client MSG. It composes the existing [Public Chat](PUBLIC_CHAT.md), [Private Chat](PRIVATE_CHAT.md), [MSG codec](ADC_MSG.md) and protocol-independent [Chat Core](CHAT_CORE.md). LoginHub owns one router and a single chat output adapter. TCP/TLS sockets, output caps, offsets, deadlines and fault cleanup remain with their existing owners. STA routing, presence/INF/QUI, search and transfer handling do not pass through this MSG service.

The complete message object is validated before accessing its header. Source-bearing scopes then require a ready sender and exact session/source ownership before scope selection. The live connection state machine still runs first and enforces direction, NORMAL, source and malformed-field rules. Source-less H/I scopes cannot be distributed by the router.

| Input | Selection / outcome |
| --- | --- |
| BMSG without PM | Snapshot of ready recipients including sender |
| DMSG, optional PM equal to source | Ready target only |
| EMSG, optional PM equal to source | Ready target then sender; self-message once |
| BMSG with PM | `private_broadcast`; live recoverable ISTA 125 |
| D/E with another PM origin | `unsupported_group`; live recoverable ISTA 125 |
| F/H/I or other unsupported MSG scope | No delivery; live client modes retain existing 125/state rejection |
| Missing, pending or closing private target | `recipient_unavailable`; live recoverable ISTA 140 |

Unsupported/private scopes never become broadcasts. Unsupported group PM is rejected without a membership lookup or echo. Unknown fields are removed by the allowlisted public/private projections. Text and optional ME/PM retain their established semantics and encoded bounds from the codec; no payload is interpreted as another command. Fatal state/source/known-field failures keep the existing ISTA 244/240 and own-IQUI close path.

The B/D/E recipient rules follow [ADC 1.0.4, message types and MSG](https://adc.sourceforge.io/ADC.html). Group and feature-chat delivery remain outside this increment.

## Engine domains and lifetimes

Each Chat Core creates one private shared routing-domain token. Broadcast and Directed plans retain that token along with their immutable data and selected session lifetime IDs. Dispatch rejects a foreign domain with `chat::Error::foreign_plan` before any sink call. Identical SID/session ID values in another registry or another Core cannot authorize a plan. Retaining the token also prevents address reuse after Core destruction from accepting old plans. The token does not extend the Core or registry lifetime; only a live engine can dispatch. Core construction can allocate and is no longer `noexcept`.

The original Core validates the sender again at dispatch and resolves every recorded recipient just before enqueue. Private dispatch also verifies the original target lifetime and SID before any target copy or echo. Removed/replaced targets get no content or echo. A renamed live user remains routable. Public plans exclude late joins and skip pending/closing/removed recipients. A moved-from adapter plan has no packet and is rejected.

A trusted sink may close/remove sessions. Traversal retains no registry iterator or borrowed model across callbacks. Once a publication has started, refusal/closure of one recipient (including the sender) does not cancel the already admitted attempt for remaining recipients. There is no cross-client atomicity or retry guarantee. All modules remain confined to the reactor owner thread.

## Queue dispatch and failures

`RoutedMessage` selects a PublicBroadcast or PrivateDelivery; `RoutedFrame` selects an immutable shared public/private packet. The router serializes no per-recipient text and allocates no storage during dispatch after sink construction. LoginHub uses one queue callback for both packet types. Each selected recipient retains independent FIFO/offset/full logical byte charge under the existing per-client/global caps. Shared physical storage does not reduce logical output charges. Partial TLS sends keep their immutable front address and remaining length stable.

Queue allocation failure closes only the affected recipient with `internal_error`; queue cap refusal closes it with `output_limit`. Other selected recipients continue. Preparation or diagnostic allocation failure closes the source without a partial publication. DispatchReport counts accepted, unavailable and refused recipients; accepted means queued, not received/read. Trusted sinks must not throw, and callers must never retry a partially dispatched plan. Completion, terminal cancellation and disconnect retain existing charge/resource cleanup.

No new configuration, dependency, SQL migration, API JSON field, WebAdmin feature, runtime translation key, account permission, history, moderation or dedicated rate policy is added. Existing input quotas/output bounds remain. Message bodies and unknown fields remain absent from logs, database and administrative exports. ADCS protects each transport; this is hub-mediated routing and does not introduce end-to-end encryption.

## Verification and current environment

`chat.routing` verifies B/D/E recipient matrices, self-deduplication, allowlisted frames, shared packet identity, allocation-free dispatch, unsupported/private scopes, malformed objects, missing/pending senders, target SID reuse, missing/closing sources, moved plans, cross-registry collisions and reconstruction of a Core at the same address. Preparation allocation sweeps check registry rollback. A callback mutation case closes the source, replaces another SID and verifies continued isolated delivery to the remaining recorded lifetime.

`chat.routing.process` adds interleaved B/D/E bursts on four mixed real ADC/ADCS connections, exact per-recipient FIFO, escaped command-like text, unsupported-mode recovery, pending-welcome exclusion, reconnect, stale SID refusal, forged source rejection and log/API privacy. Existing `adc.ncdc` and `adcs.ncdc` exercise real ncdc public/private/action/reconnect traffic through the new router.

Local verification on 2026-11-11: all 78/78 CTest suites passed in Debug, Release and ASan/UBSan with strict integration-tool detection and without disabling leak detection. This includes actual ncdc ADC/ADCS public/private/action/reconnect traffic, mixed-transport routing, disposable MariaDB, Apache HTTPS and authenticated API checks. Builds emitted no compiler warnings. WebAdmin typecheck/production build, 4 Node tests, 3 Chromium tests and real C++ API integration also passed. Metadata, UTF-8/language parity, local Markdown targets and Git whitespace checks passed. Production load, remote CI and the full OS/client matrix remain unverified.

Reproduce the strict integration suite:

```sh
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug -DDC24H_REQUIRE_INTEGRATION_TESTS=ON
cmake --build build-debug --parallel 2
ctest --test-dir build-debug --output-on-failure
npm --prefix webadmin run build
npm --prefix webadmin test
PLAYWRIGHT_CHROMIUM_EXECUTABLE=/usr/bin/chromium npm --prefix webadmin run test:e2e
PLAYWRIGHT_CHROMIUM_EXECUTABLE=/usr/bin/chromium npm --prefix webadmin run test:integration -- ../build-debug/dc24h
```
