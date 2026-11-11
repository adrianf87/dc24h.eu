# Chat Validation — alpha-0.4.8

**Current increment — alpha-0.4.9:** [Anti-Flood](ANTI_FLOOD.md) adds enabled per-connection B/D/E message and decoded-byte buckets, configurable in `[chat]`. Excess messages are dropped before fanout with bounded recoverable notices; existing validation and general resource limits remain.

Based directly on main `b20cc83fa46a48642e6939a8ceb11a354339a4dd`, after the Hub Messages merge. Earlier branches/history remain intact. Code and protocol text use EN/US UTF-8; operator help and documentation also support Polish.

## Shared admission gate

`adc::validate_chat(core, sender, message)` in `chat_validation.hpp` and `chat_validation.cpp` centralizes client chat admission. PublicChat and PrivateChat call it before preparing a plan, including when called directly without MessageRouter. The router selects an adapter by comparing the enum scope; it no longer separately serializes/validates every message before the adapter. It never dereferences an unchecked optional SID.

The function performs no session/queue mutation and returns a borrowed `MsgView` or a typed `ChatValidationError`. Its result is data, not a reusable authorization token. The input Message must outlive its view and remain unchanged. There is no cached validation bit, asynchronous lookup or new persistent registry.

The [ADC specification](https://adc.sourceforge.io/ADC.html) defines NFC UTF-8, four-character base32 SIDs and MSG in NORMAL. The implementation applies these protocol rules together with the existing application text/queue limits and ready-session ownership checks.

| Check | Result |
| --- | --- |
| Wrong command/kind, absent text vector, inconsistent positional count, excessive parameter count | `invalid_message` |
| Empty, oversized decoded, malformed UTF-8, non-NFC or forbidden ASCII control text | `invalid_text` |
| Missing/malformed routed source SID or local reserved source AAAA | `invalid_sid` |
| Missing/malformed D/E target SID | `invalid_sid` |
| Invalid complete header, flags, unknown-field encoding or encoded wire limit | `invalid_message` |
| Missing or non-ready sender lifetime | `invalid_state` |
| Valid source SID owned by a different connection | `wrong_source` |
| Valid unsupported F/H/I chat scope | `unsupported_type` |
| Supported B/D/E with a ready matching sender | Borrowed decoded text and PM/ME values |

Structural guards precede access to text/SIDs. Text is bounded before normalization or serialization. Routed source and D/E target syntax are checked explicitly, followed by the full MSG codec. H/I cannot supply a client sender. For FMSG, the source binding is checked before returning unsupported scope, retaining the existing spoofing guard. Live incoming IMSG remains rejected by the connection state machine before this gate; trusted system IMSG stays in [Hub Messages](HUB_MESSAGES.md).

## Text and SID policy

Text is nonempty and at most 16384 decoded bytes. Decoded LF is allowed; other ASCII controls and DEL are rejected. Valid text retains its exact bytes, including leading/trailing/repeated spaces, newlines, backslashes and flag-like words. NFC is checked, not silently applied; malformed input is neither repaired nor truncated. This is protocol validation, not moderation, Unicode presentation filtering or markup rendering.

The existing full codec also limits the entire encoded body to 16384 bytes plus final LF and 128 total parameters. Headers, flags, separators and escape expansion count. Unknown fields still require valid syntax/UTF-8/NFC even though routed output drops them. Known PM/ME duplicate/value rules remain in the shared flag codec.

SID spelling is case sensitive base32 with exactly four characters. Source AAAA is reserved by this hub and cannot become a client origin, even in a manually constructed object. A syntactically valid target such as an unavailable AAAA/ZZZZ is not a grant of membership; normal private preparation returns `recipient_unavailable` rather than looking up a replacement by nickname or file descriptor.

The caller supplies a local SessionId lifetime, independent of the wire SID. Core checks that this lifetime exists, is fully ready and owns the claimed source. Accepted, negotiating, identified-but-welcome-pending, closing and closed senders are refused. Rename does not change the source binding.

## Recipient and dispatch checks

After shared validation, public/private adapters retain their scope and PM conversation rules. PM-marked broadcast and unsupported groups never become public messages. Core::prepare_direct selects a ready, identified target only after private PM policy; this keeps group-refusal precedence and lifetime binding. Missing/pending/closing/closed recipients get the existing recoverable ISTA 140, without a private copy or echo.

The borrowed view alone does not select a target or authorize delivery. Core preparation checks state again while creating an owning plan; dispatch still checks engine domain, sender readiness/source, target identity and each selected lifetime just before the sink. Closing/removing a sender after successful validation prevents later publication. Old plans cannot reach reused SID/fd owners, later joins or another Core domain. All modules retain the single reactor-thread ownership contract.

PublicChat/PrivateChat preserve their public error enums. Shared invalid text/SID/object errors map to invalid_message, invalid_state and wrong_source retain their corresponding errors, and valid unsupported scopes remain unsupported_type. MessageRouter keeps its existing errors and LoginHub keeps sanitized diagnostic descriptions.

## Live behavior and failures

Live input first passes generic parsing and the existing connection state machine. Invalid generic syntax, UTF-8/NFC, escapes or oversized wire frames are discarded under existing quotas; a subsequent legal message can recover. NORMAL/source checks and malformed known-field failures keep the existing fatal ISTA 244/240 plus bounded own-IQUI close path. Unsupported scopes/groups remain recoverable 125. This layer does not weaken those earlier gates or introduce payloads into diagnostics.

Validation allocations may throw before publication. Existing LoginHub handling closes only the source on preparation failure; per-recipient queue faults remain isolated in dispatch. The validator itself has no side effects. No new dependency, configuration, SQL migration, HTTP endpoint, API field, history or moderation is added. Message bodies remain absent from logs, persistence and administrative exports. US/PL help and release metadata are updated.

## Verification

`chat.validation` covers text controls/UTF-8/NFC, exact encoded bounds, literal whitespace/commands, malformed complete objects, source/target syntax and missing optionals, reserved source, source spoofing, all sender phases and non-ready target phases. It also covers direct adapter/router gates, unsupported-scope precedence, rename, allocation rollback and validation followed by source close/dispatch refusal. Existing lifetime, domain, PM/ME, queue fault and pinned-output tests remain enabled.

`chat.validation.process` exercises both real source transports, generic malformed-text/SID recovery with ordered observer fences, preserved valid Unicode/whitespace/newlines, exact/oversized frames, pre-NORMAL B/D/E rejection, forged sources and log/API privacy. Real ncdc ADC/ADCS receives an additional Unicode/newline/ME0 message while public/private/action/welcome/reconnect checks remain.

```sh
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug -DDC24H_REQUIRE_INTEGRATION_TESTS=ON
cmake --build build-debug --parallel 4
ctest --test-dir build-debug --output-on-failure --parallel 3
ctest --test-dir build-debug -R '^(chat.validation|adc.ncdc|adcs.ncdc)' --output-on-failure
```

Current validation is recorded in [CHANGELOG.md](../../CHANGELOG.md). Remote CI, the full OS/client matrix, production load and long-running fuzzing require separate evidence.

Local verification on 2026-11-11: **84/84 CTest suites passed in Debug, Release and ASan/UBSan**, with strict integration-tool detection and without disabling leak detection. This includes real ncdc ADC/ADCS, Chat Validation, chat/session/queue regressions, disposable MariaDB and Apache HTTPS. Builds emitted no compiler warnings. WebAdmin build/typecheck, 4 Node tests, 3 Chromium tests and real C++ API integration passed. Metadata, UTF-8/language parity, local links, modified Python syntax, US/PL help and Git whitespace checks passed. Remote CI, the full OS/client matrix, production load and long-running fuzzing remain unverified.
