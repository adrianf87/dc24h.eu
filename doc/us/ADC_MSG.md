# ADC MSG — alpha-0.4.1

**Current increment — alpha-0.4.2:** [Chat Core](CHAT_CORE.md) adds a separate protocol-independent message engine and live public BMSG delivery, including sender echo and /me. Private/group/feature/hub chat, moderation and history remain separate increments.

Branch alpha-0.4.1 starts directly from main `b528248dbe6984c6554821d758f0f5d0cbc285fb`, after the alpha-0.3.13 Statistics merge. Existing code/history and previous branches are preserved. Development/runtime default language is EN/US UTF-8; PL documentation and existing runtime catalogs remain available.

## Scope and protocol

This increment implements a dedicated codec for the BASE `MSG text` command described in [ADC section 5.3.5](https://adc.sourceforge.io/ADC.html#_msg). It composes with the existing generic parser, stream decoder, serializer, SID type and state machine. It does not deliver public/private chat, select recipients, echo messages, resolve PM groups or grant permission to send. No database, API JSON, configuration, WebAdmin feature or runtime translation key is added; release metadata is updated throughout.

| Header | Codec representation |
| --- | --- |
| `BMSG source text` | Broadcast header with source SID |
| `DMSG source target text` | Direct header with source and target SIDs |
| `EMSG source target text` | Echo header with source and target SIDs |
| `FMSG source selectors text` | Feature header preserving required/excluded selectors |
| `HMSG text` | Client-to-hub header |
| `IMSG text` | Hub-to-client header |

The codec rejects CMSG/UMSG, which are outside this hub profile. The generic syntax parser remains command-context neutral, so it may parse such a header before semantic validation rejects it. Direction, NORMAL and source ownership are separate state gates. No recipient existence or feature-match policy is inferred from syntax.

Text is one required nonempty decoded parameter, not a nickname-prefixed string. It is preserved without trimming or normalization. Whitespace-only text is representable using escaped spaces. Text that resembles a flag, such as `IMSG ME1`, remains text. UTF-8 and NFC are required by the shared ADC parser; invalid/non-NFC data is rejected rather than silently rewritten. Spaces, LF and backslash use the existing escapes. Raw ASCII controls including CR, NUL, TAB and DEL are rejected; an escaped LF is valid message content.

## BASE flags and extensions

| Field | Checked meaning |
| --- | --- |
| `PM<group-SID>` | Exactly four uppercase base32 SID characters, returned as optional typed Sid |
| `ME<integer>` | ADC signed decimal int64 with value 0 or 1, returned as optional bool |

Duplicate PM or ME is rejected, including repeated equal values and ME0 followed by ME1. Empty known values, invalid SIDs, noninteger/overflow/out-of-domain ME values are rejected. Decimal spellings such as ME01 and ME-0 retain their original wire spelling when parsing/serializing; the local builder emits canonical ME0/ME1. Absent ME and explicit false remain distinct. A PM group may differ from the originating SID for future group conversations; a syntactically valid group is neither authenticated nor resolved. Future normal-private-message policy must enforce the origin relationship where required by ADC.

Unknown syntactically valid named parameters are retained in input order, including repeated/bare unknown fields. They are opaque extension data, not trusted public metadata. No generic relay/sanitizer is supplied in this stage; a future chat dispatcher must choose its extension/privacy policy before forwarding. Message text and unknown values never enter diagnostic logs or the administrative API.

## C++ interface

`include/dc24h/adc/msg.hpp` defines:

- `parse_msg(body) -> variant<Message, ParseError, MsgError>`: accepts exactly one body without its terminating LF. Keepalive/non-MSG input is not a MSG. Generic syntax errors retain their existing ParseError; semantic errors use MsgError. The returned Message owns its strings and records positional_count=1.
- `validate_msg(message) -> variant<MsgView, MsgError>`: validates hand-built objects too, including header consistency, UTF-8/NFC, escaping budget and layout metadata. MsgView borrows only the decoded text; PM and ME are copied typed values. Destroying/mutating the Message invalidates the borrowed text.
- `serialize_msg(message) -> variant<string, MsgError>`: validates MSG semantics and returns one owning LF-terminated string using the shared serializer. Invalid input produces no partial result. Parsing then serializing preserves valid header, text, unknown fields and flag order.
- `make_msg(text, MsgOptions) -> Message`: checked local builder, default IMSG. Options contain header type, optional typed source/target/group SIDs, optional ME bool and feature selectors. Invalid local options throw invalid_argument; allocation failures propagate normally. No text is truncated and no state is mutated.

All inputs retain the existing application limits: 16384 encoded body bytes excluding final LF, 128 parameters including the text, and 128 feature selectors. These are local resource limits, not limits imposed by the protocol. Header/separators and the expansion of every escaped byte count toward the frame ceiling. A valid serialized frame is at most 16385 bytes including LF. There is no separate chat policy text-length limit in this increment. Validation does not claim to be allocation-free; the shared serializer reuses the parser as a consistency check.

## Live integration

ConnectionStateMachine checks direction/type, NORMAL and bound source SID before validating MSG semantics. A successful MSG observation does not mutate login state. Malformed known flags return StateError::invalid_message, mapped by the existing LoginHub path to fatal ISTA 240 with offending FOURCC, own IQUI and bounded cleanup. A wrong state retains 244; a forged source retains 240. Generic malformed syntax is discarded under existing parser/security quotas and can recover at the next LF.

Valid MSG still receives the existing recoverable ISTA 125 “Command not implemented” from LoginHub. This confirms syntax/state support without advertising implemented chat delivery. No message is relayed to an observer, and PM metadata does not create a conversation or reserve a user. Existing per-peer input/output limits, user/session counts and readiness remain in force.

## Tests

`adc.msg` covers official examples, all six supported headers, PM/ME values, extension ordering, ownership and borrowed views, builders, hostile objects, Unicode/escaping, exact encoded limits, parameter bounds, fragmented streams, oversize recovery, EOF and state/source precedence. Its bounded --roundtrip test mode validates actual captured ncdc frames through the codec.

`adc.msg.process` tests real ADC/ADCS sockets with fragmented Unicode messages, B/D/E/F/H headers, PM/ME, syntax recovery, fatal semantic errors, spoofed source, pre-NORMAL rejection, cleanup and observer/log privacy. Actual ncdc tests issue public text, `/me` and `/msg` over ADC/ADCS; plaintext capture is parsed and serialized byte-for-byte, while the encrypted path verifies continued operation without exposing plaintext. See the [ncdc command manual](https://dev.yorhel.nl/ncdc/man).

Full CTest and WebAdmin regressions remain required before publishing. Local validation does not establish production load capacity or all supported OS versions.
