# Transfer Tokens — alpha-0.6.7

**Current increment — alpha-0.6.8:** [Connection Security](CONNECTION_SECURITY.md) adds startup CTM/RCM and guest permissions, trusted ready-lifetime grants and preparation/dispatch/echo authorization rechecks. Shared binary IP and port validation preserve managed-profile behavior. Native, mixed ADC/ADCS and real ncdc cases cover denials, spoofing and recovery.

**alpha-0.6.7 local validation (2026-11-14):** All 153/153 CTest tests passed in Debug, Release and ASan/UBSan, including four real ncdc boundary-token scenarios over both hub transports and plaintext/TLS peer handshakes. No compiler warnings. WebAdmin build/typecheck, 12 Node tests, 10 Chromium tests and real C++ API integration passed. Metadata, UTF-8, language parity/local links, changed Python syntax, example INI and whitespace checks passed. GitHub Actions completion is not awaited; the full supported OS matrix and production load remain unverified.

Based directly on main `9df87da184aab7376de0587bbb76121eccc9fe4b` after Client Capabilities. This increment extracts a single connection-token policy and an owning typed value, and verifies exact token forwarding end to end. The numbered branch is published separately from main, preserving existing code/history.

## Token value and validation

`transfers/token.hpp` defines `max_token_bytes = 128`, `TokenError`, `validate_token(string_view)` and `Token::create(string_view)`. The factory returns `variant<Token, TokenError>`; the validator returns an optional typed error. `Token::value()` exposes the immutable owning string, and equality compares exact bytes. Factory input may be discarded or changed after success. Copying owns a separate string; moving transfers ownership under ordinary C++ moved-from rules.

| Decoded input | Result |
| --- | --- |
| Empty | `TokenError::empty` |
| More than 128 bytes | `TokenError::too_long` |
| Malformed UTF-8, non-NFC, control/format or line/paragraph separators | `TokenError::invalid_text` |
| 1..128 valid NFC UTF-8 bytes | Accepted without changes |

These are decoded **byte** bounds, not character counts or an ADC-wide maximum. Bounds precede normalization checks or owning copies. UTF-8/NFC validation uses the bounded shared text validator, which may allocate temporary storage. Allocation failures propagate; validation is not advertised as allocation-free. Empty/oversized factory input is rejected before copying. Owned string allocation failure also propagates without altering input or sessions.

Tokens are opaque and case-sensitive. Leading/trailing spaces, an all-space token, backslashes, composed Unicode, emoji, numeric-looking values and strings beginning with TO/PD remain valid if they meet the same policy. No trimming, normalization, case folding, numeric conversion, prefix interpretation or replacement token occurs. A trusted originator supplies its own token; the hub does not generate one in this stage.

## One policy across coordination

`Request` now owns a `Token` and adds `token_value()`. The existing `token()` string API and `max_token_bytes` access through request.hpp remain source-compatible. Existing request kind/family/port checks and error precedence remain; token factory errors map to `RequestError::invalid_token`. Native protocol validation now calls its factory once. Dependent C++ components must rebuild; no stable binary ABI is promised.

The shared ADC connection-field helper calls `validate_token` for CTM/RCM positional tokens and STA connection errors 41/42. Those STA errors accept the same opaque token in their named TO field; duplicate TO fields remain invalid. Other STA codes keep their existing field rules. Generic syntax/escaping/UTF-8 validation still precedes or surrounds semantic validation: a malformed native or wire frame may report a codec error before the token-specific error. No error-order or parser-policy change is introduced.

CTM/RCM retain original checked protocol/port/token/extension ordering and canonical escaping. An extra named TO extension never replaces the positional token. `ConnectionFrame::token()` exposes the exact decoded positional value from its immutable owning Message. Transfer Core's typed value and the queued frame therefore remain independent of later caller mutation. Dispatch and token access allocate nothing after preparation/sink construction.

## Forwarding and ownership

The [ADC specification](https://adc.sourceforge.io/ADC.html), sections 5.3.8–5.3.9 and 6.2, requires an RCM response to preserve the selected protocol/token and a connecting client to carry its token in CINF. Clients perform that peer handshake; the hub forwards CTM/RCM and peer failure messages and never receives peer CINF or file bytes.

D reaches the exact ready target lifetime; E also independently echoes to the source. Tokens do not select recipients, bind a SID, authorize a connection or identify a registered account. Independent requests and unrelated pairs may reuse identical bytes. The same bytes do not revive a departed SID or retarget a prepared plan. One-attempt plans still prevent replay of the same prepared operation, independently from token equality.

There is no pending-token registry, uniqueness policy, inferred RCM→CTM correlation, timeout, replay cache or token-based permission in this stage. Search tokens have their own existing namespace/policy and registry; connection tokens never enter it. Client-side checks for an expected peer token remain the clients' responsibility. Full peer authentication, production TLS pinning, NAT traversal, passive relay and file transfer remain future work.

Validation failure and recipient errors expose bounded generic status descriptions, not supplied token contents. Connection payloads stay outside logs, SQL, API/WebAdmin and Chat/Search counters. The existing ready/source/state/queue/endpoint checks remain unchanged.

## Verification

`transfers.tokens` checks typed errors, exact limits including multibyte boundaries and escaped expansion, whitespace/prefix/case preservation, ownership/copy/move, bounds before copying, allocation failure, parity across Request/CTM/RCM/peer errors, duplicate status TO, mutable caller isolation, immutable frame access, D/E audiences, allocation-free dispatch, copied-plan replay refusal and legal reuse in new requests.

`transfers.tokens.process` runs both address families with mixed ADC/ADCS peers. Parallel CTM and RCM pairs intentionally reuse token bytes. Both scopes, RCM→CTM, peer status replies, fragmented headers, named TO extensions, recoverable invalid tokens, reconnect/SID isolation, duplicate-status rejection and API/log privacy are checked.

Four new ncdc scenarios (`adc.tokens.ncdc`, `adc.tokens.adcs.ncdc`, `adcs.tokens.ncdc`, `adcs.tokens.adcs.ncdc`) exercise both hub transports and plaintext/certificate-checked TLS peer sockets. The real active client returns a 128-decoded-byte token containing edge spaces, backslash, Polish text and emoji in its CTM response; the raw peer completes CSUP/CINF using that token. An unsupported protocol then produces a real DSTA with the same exact TO value. Existing client-generated-token and other interoperability suites remain enabled.

See [Testing](TESTING.md), [Transfer Core](TRANSFER_CORE.md), [Connection Routing](CONNECTION_ROUTING.md), [Protocol Negotiation](PROTOCOL_NEGOTIATION.md), [ADC CTM](ADC_CTM.md) and [ADC RCM](ADC_RCM.md).
