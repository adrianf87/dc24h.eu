# ADC Parser — alpha-0.2.2

**Current increment — alpha-0.3.1:** [Session Core](SESSION_CORE.md) provides the independent Users model and stable lifetime IDs, with live readiness/closure/timeline integration. Earlier stage descriptions below retain their original delivery scope.

**Increment delivered in alpha-0.2.10:** [Negotiation security](NEGOTIATION_SECURITY.md) adds pending admission caps, cumulative pre-readiness input budgets and shared pre-INF/Tiger attempt limits. Earlier release descriptions below retain their original scope.

**Increment delivered in alpha-0.2.9:** [STA/QUI](ADC_STATUS.md) adds semantic status/quit validation, source-checked B/D/E/F status routing, precise errors and bounded terminal ISTA/own-IQUI output. Historical descriptions below retain the behavior of their original releases.

**Integration delivered in alpha-0.2.8:** [Guest login](ADC_LOGIN.md) attaches the state/SUP/SID/INF/Tiger stack to the actual C++ TCP/TLS reactor with bounded ordered output, nickname/CID admission, public presence and complete ncdc NORMAL/reconnect tests. Historical stage descriptions below retain their original scope; statements that transport attachment/output/login were still planned describe those earlier versions. Registered-user authentication, chat, search and transfer dispatch remain planned.

Since alpha-0.2.7, [CID/PID verification](ADC_IDENTITY.md) is a dedicated shared module with TigerCid/VerifiedIdentity values, direct cleansed output buffers and a constant-time final digest comparison. INF snapshots retain only a public verified result. Basic verification introduced in alpha-0.2.6 remains compatible; live transport attachment and account/registry policy remain separate.

Since alpha-0.2.6, [ADC INF](ADC_INF.md) supplies ClientSession, combining this lower-level stack with intrinsic INF validation and Tiger PID/CID verification. It requires validated metadata before external identity admission and never retains/exports PD. Registry uniqueness and actual Network attachment remain planned; older APIs/fixtures retain their original scope.

Since alpha-0.2.5, [ADC SID](ADC_SID.md) supplies a shared hub pool and SidSession composition with checked ISID ownership and automatic reservation cleanup. Parser/state SID syntax uses the same codec; AAAA remains valid syntax but is reserved by the pool as local policy. The original lower-level APIs and historical fixtures remain available. Network still uses only the parser.

alpha-0.2.4 adds the independent [SUP semantics and state adapter](ADC_SUP.md). Parser behavior is unchanged. Existing hub ncdc tests still cover initial HSUP; a new isolated SUP fixture exchanges module-generated ISUP and a fixed test SID with ncdc, stopping at unapproved IDENTIFY. It is not the live Network reactor.

Reference: [ADC 1.0.4](https://adc.sourceforge.io/ADC.html), line grammar and BASE command descriptions. The independent C++20 `dc24h_adc` library parses syntax; it does not execute commands or implement a session.

## Owned result and argument layout

`parse_line(string_view)` accepts one frame body **without LF** and returns `KeepAlive`, `Message` or `ParseError`. Empty LF is keepalive. A Message owns type, original command, recognized kind, optional routing source/target, ordered feature selectors and unescaped parameters. Input buffers may be released immediately. Unknown commands retain their original name and every parameter.

`Message::positional_count` is set for known BASE commands. Parameters before that index are positional; the remainder are named. SUP uses all parameters as positional feature updates. Unknown command layouts remain `nullopt`: a token beginning with NI must not be guessed to be a field in an unknown extension.

`parse_named_parameter(decoded_token)` validates the two-byte uppercase/alphanumeric name prefix and returns optional `NamedParameterView {name, value}`. These two string_views borrow the token's storage and must not outlive it or survive string/vector mutation. It does not decode again or validate the value. Bare names have empty values, supporting cancellation. Unknown names, repeated names/search terms and their order are preserved; the parser does not choose a first/last value or deduplicate them.

`parse_integer(decoded_token)` returns an optional signed 64-bit decimal integer. Minus, leading zeroes and negative zero are accepted. Plus, whitespace, fractions, exponents, non-ASCII digits, embedded NUL, trailing data and overflow are rejected. Zero is a successful result. This helper does not enforce port/range/transfer policy.

## BASE argument syntax

| Commands | Leading positional arguments | Additional checks |
| --- | --- | --- |
| STA | 2 | First argument has three ASCII digits; unknown status values remain syntactically valid |
| SUP | One or more, variadic | Every token has AD/RM plus a four-character feature identifier; no arbitrary named suffix |
| SID, QUI | 1 | Positional SID has four base32 characters |
| MSG, GPA, PAS | 1 | Positional text/data retained; hash correctness/length is not checked |
| INF, SCH, RES | 0 | All tokens must have a valid named prefix; no required application fields imposed |
| CTM | 3 | Port has bounded integer syntax; protocol and token remain strings |
| RCM | 2 | Protocol and token remain strings |
| GET, SND | 4 | Transfer namespace is ASCII letters/digits; offset/count have bounded integer syntax |
| GFI | 2 | Transfer namespace is ASCII letters/digits |

Except for SUP, all tokens following the fixed positional prefix must be named parameters. Positional strings that look like field names remain positional. Existing parameters stay in one owning vector, avoiding duplicate copies. For example, `HMSG NItext PMABCD` has positional_count 1; `BINF ABCD NI` has positional_count 0 and a bare field. These examples describe parser syntax, not authenticated users or allowed state transitions.

Known malformed commands that alpha-0.1 accepted as generic tokens now fail: missing mandatory arguments, malformed named prefixes, SUP updates, positional SID/status/numeric/namespace syntax. New error categories are missing_parameters, invalid_named_parameter, invalid_support, invalid_status_code, invalid_integer, invalid_transfer_type and too_many_features. Existing errors retain their enum order. Raw framing/UTF-8/header/escape errors are checked before argument syntax. When several errors occur, callers must not rely on a diagnostic precedence beyond this order.

## Framing, Unicode and local limits

B/C/D/E/F/H/I/U headers are supported. Required routing SIDs, variable-length base32 U CIDs and signed F feature selectors are checked. CID length is not tied to TIGR. One ASCII space separates tokens; empty/trailing/double separators are rejected. Only `\s`, `\n` and `\\` escapes are decoded. utf8proc checks UTF-8/NFC; non-normalized text is rejected without rewriting it.

Application policy limits, not protocol maxima:

- 16384 frame-body bytes, excluding LF; 128 parameters, excluding routing.
- 128 F-header selectors, checked before allocating selector entries. This new independent bound does not affect SUP's parameter budget.
- Signed 64-bit numeric positional values. No raw ASCII controls/DEL, including NUL, tab and CR; CRLF is rejected.

`StreamDecoder::feed(bytes, sink)` synchronously delivers results in order without retaining messages. A sink must not reenter its decoder; exceptions propagate. Instances are confined to one thread. A malformed completed frame produces one result and resumes at the next LF. Oversized input emits one immediate error and discards until LF, retaining at most 16 KiB. `finish()` reports a partial frame once, resets and permits reuse; already counted oversized frames are not counted again. `in_progress()` includes the discard state for absolute deadlines.

## Integration and limits of this stage

Every TCP/TLS connection uses the same stricter parser automatically. Network counters count accepted syntax; unknown commands remain an accepted subset. Invalid argument frames use the existing invalid/result quotas, generic sanitized warning and recovery path. No payload, PID or PAS value enters logs. Security can close a peer after quota breaches. See [SECURITY.md](SECURITY.md) and [ADCS_TLS.md](ADCS_TLS.md).

The independent [ADC State module](ADC_STATE.md) now implements command/type/state gates, but Network does not invoke it yet. No serializer, outbound queue, SID allocator, identity/hash validation, authentication, routing, chat/search/transfer execution or UDP listener exists. Port domains, transfer offset/count meaning, field value semantics and mandatory INF/RES identity fields belong to future consumers. Parsing GET/SND does not enter DATA mode or consume binary data. Real ncdc tests still cover initial HSUP only; no ISUP/ISID response or full login is claimed.

## Verification

`adc.syntax` checks all 15 BASE layouts, missing arguments, literal positional text, bare/repeated/unknown fields, scalar errors/boundaries, opaque extensions, selector/parameter budgets and result ownership. Its stream corpus compares all decoded headers/values/layouts/errors across every two-chunk split and bytewise input, including Unicode and escapes. Existing deterministic malformed-byte/chunking/oversize/EOF tests remain in `adc.parser`.

TCP and real TLS regressions mix malformed BASE arguments with valid frames and opaque extensions, checking counters, recovery and secret-free logging. Existing ncdc ADC/ADCS regressions still exercise actual client traffic. Run the complete suite as described in [TESTING.md](TESTING.md).
