# ADC Parser — alpha-0.2.2

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

**Increment delivered in alpha-0.5.5:** [Search Tokens](SEARCH_TOKENS.md) adds bounded session-lifetime/TO registration, local search generations, deadlines, cancellation and explicit active-search APIs in Core. Stateless preparation stays available; refused live SCH requests do not reserve tokens. Result correlation/routing and search administration remain later increments.

**Increment delivered in alpha-0.5.4:** [TTH Search](TTH_SEARCH.md) carries the full 192-bit file root from SCH into immutable Core queries/plans, descriptors and compiled filters. Hash-only and combined queries use exact equality and retain all additional criteria. Real ncdc ADC/ADCS TTH searches are tested; live request/result routing remains a later increment.

**Increment delivered in alpha-0.5.3:** [Search Filters](SEARCH_FILTERS.md) adds compiled Unicode case-insensitive name/path predicates, extension alternatives, inclusive/exact byte limits and file/directory types over checked descriptors. Filter-only queries and opaque NFC tokens now project to Search Core. Matching is allocation-free; routing/results, indexes and search administration remain later increments.

**Increment delivered in alpha-0.5.2:** [ADC SCH](ADC_SCH.md) adds owning typed SCH parsing and live ADC/ADCS reception after state/source checks. Known fields, selectors, limits and canonical TR are checked; valid requests receive an explicit routing-not-implemented response, malformed semantic requests are recoverably refused, and criterion-free requests are ignored. Routing/results remain later increments.

**Increment delivered in alpha-0.5.1:** [Search Core](SEARCH_CORE.md) adds an independent C++20 library for bounded immutable textual queries, ready-session/feature selection and lifetime/domain-bound dispatch. Live SCH/RES parsing, serialization and transport attachment remain future increments; the running server still returns an explicit unsupported response.

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

**Increment delivered in alpha-0.3.12:** [Limits](LIMITS.md) adds configurable session/user ceilings shared by ADC/ADCS, bounded by the connection cap. SessionManager checks capacity transactionally; LoginHub rejects a full user registry before welcome/list preparation with ISTA 211 and own IQUI. Closing reservations count until cleanup. Earlier descriptions retain their historical scope.

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
