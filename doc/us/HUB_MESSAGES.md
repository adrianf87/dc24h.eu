# Hub Messages — alpha-0.4.7

**Current increment — alpha-0.4.8:** [Chat Validation](CHAT_VALIDATION.md) centralizes client text, SID and sender readiness/ownership checks in both adapters. Recipient lifetime selection and dispatch revalidation remain authoritative; system IMSG stays separate.

Based directly on main `8d0da2516b007466828b00aafd4e7bf7a857b9f3`, after the Message Flags merge. Earlier branches and history are preserved. Code and system wire text use EN/US UTF-8; operator help and documentation also support Polish.

## Scope

`adc::HubMessages` provides trusted, in-process system message preparation and delivery. Its immutable `HubMessageFrame` contains an IMSG with one positional text value, no source/target SID, no feature selectors and no PM/ME fields. The [ADC specification](https://adc.sourceforge.io/ADC.html) identifies type I as hub-originated information and includes IMSG as a hub chat example. Client chat remains in MessageRouter; it is never promoted to system output.

`make_hub_message(text)` returns an immutable shared packet or a typed `HubMessageError`. It checks nonempty, bounded UTF-8/NFC text, permits decoded LF, rejects other ASCII controls/DEL, and serializes through the existing MSG codec. Spaces, LF and backslashes are escaped. The complete encoded body is limited to 16384 bytes plus final LF; escape expansion counts. Invalid text is neither normalized nor truncated. Literal strings such as `BMSG`, `PM` or `ME1` inside text remain text.

## Selection and dispatch

`HubMessages::prepare(text, optional<SessionId> target)` accepts trusted text, not a client Message or source identity. A target selects one ready session lifetime; no target selects an ordered snapshot of all ready sessions through bounded registry pages. Pending welcomes, anonymous, closing and closed sessions are excluded. Missing/non-ready targets return `recipient_unavailable` before publication. A broadcast with no ready users is a valid empty plan.

`HubDelivery` owns its frame, recipient IDs and a private service-domain token. The original service rechecks every selected lifetime during dispatch. Removed recipients are unavailable; later joins and reused SIDs do not inherit old plans. Another service, even over the same registry, cannot dispatch the plan. A retained token also prevents a new service at a reused address from accepting it. Moved-from plans have no frame and are rejected.

Dispatch allocates no storage after sink construction. It reports queued, unavailable and refused counts using `chat::DispatchReport`, without requiring a fake chat sender. Trusted callbacks may close sessions, but must not throw or retry partially dispatched plans. Queue acceptance is not delivery/read acknowledgment; cross-recipient atomicity is not promised.

`LoginHub::send_hub_message(text, optional<SessionId> target)` connects this service to existing bounded queues. This is a reactor-thread C++ API, not a new HTTP endpoint or a client command. Preparation errors leave queues unchanged; allocation before dispatch propagates to the trusted caller. Queue allocation failure closes only that recipient with `internal_error`; cap refusal uses `output_limit`. Other selected recipients continue. Packets are shared physically, while every queue retains independent FIFO, offset and full logical byte charges. Partial TCP/TLS writes and terminal-close cancellation keep their existing immutable-front rules.

## Live welcome

Each LoginHub prepares one shared fixed greeting:

```text
Welcome to dc24h.eu. Guest access is enabled.
```

After valid guest identity approval, the output order is hub IINF, existing public user BINF frames, own BINF, then the greeting IMSG. Any later presence/chat follows in the same FIFO. The greeting is queued before registry commit and public publication; its full wire size is reserved in the aggregate welcome budget. A failed/over-budget welcome does not publish a user or greeting.

Own BINF's final LF still marks readiness and releases pending-login capacity, exactly once. The trailing IMSG does not delay or duplicate that transition. A client therefore sees its own identity before system chat. Closing before completion cancels the unsent tail under existing rules. Reconnect receives one fresh greeting; existing users do not receive another user's greeting.

This fixed message contains no nickname, peer address, CID/PID, credentials or other user input. It remains English wire text, independent of operator `--language`; there is no client-language negotiation. US/PL help explains IMSG. Arbitrary system text is not logged, persisted or exported by this service. Trusted callers are responsible for selecting non-secret system content; this is not a content-redaction engine.

## Trust and limits

Client-origin IMSG remains an invalid direction: fatal ISTA 240 and final own IQUI, without relaying the supplied payload. HMSG remains an unsupported client-to-hub operation with the existing recoverable 125; no command interpreter or echo-as-system path is added. IMSG errors do not replace STA/QUI diagnostics.

No new configuration, external dependency, migration, API JSON field or WebAdmin feature is added. Remote administrator messaging, scheduled announcements, shutdown-drain notices, localization per client, message history and moderation remain future work. External code must use the same reactor owner thread; the registry is not made thread safe.

## Verification

`chat.hub` covers encoding, text/encoded limits, immutable packets, 270-recipient pagination, pending exclusion, typed missing-target failures, domain isolation, moved/stale plans, late joins, mutation/refusal during callbacks, allocation-free dispatch and preparation rollback. LoginHub checks exercise welcome order, targeted/public system delivery, shared packet addresses, partial pinned output, terminal tail cancellation, cap refusal isolation and charge cleanup. Existing queue allocation and transport suites also exercise the extended output variant.

`chat.hub.process` verifies fragmented login, exact own-INF/IMSG order, single welcome, reconnect, mixed ADC/ADCS, client IMSG impersonation rejection and log/API privacy with an unrelated observer. Shared process helpers assert the greeting explicitly. Real ncdc ADC/ADCS tests display the system welcome while retaining public/private/action/reconnect coverage; plaintext recording checks its actual IMSG frame.

```sh
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug -DDC24H_REQUIRE_INTEGRATION_TESTS=ON
cmake --build build-debug --parallel 4
ctest --test-dir build-debug --output-on-failure --parallel 3
ctest --test-dir build-debug -R '^(chat.hub|adc.ncdc|adcs.ncdc)' --output-on-failure
```

See [CHANGELOG.md](../../CHANGELOG.md) for current validation. Remote CI, the complete OS/client matrix and production-load testing require separate evidence.

Local verification on 2026-11-11: **82/82 CTest suites passed in Debug, Release and ASan/UBSan**, with strict integration-tool detection and without disabling leak detection. This includes real ncdc ADC/ADCS, Hub Messages, chat/session/queue regressions, disposable MariaDB and Apache HTTPS. Builds emitted no compiler warnings. WebAdmin build/typecheck, 4 Node tests, 3 Chromium tests and real C++ API integration passed. Metadata, UTF-8/language parity, local links, modified Python syntax, US/PL help and Git whitespace checks passed. Remote CI, the complete OS/client matrix and production load remain unverified.
