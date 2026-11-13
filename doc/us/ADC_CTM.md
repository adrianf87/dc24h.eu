# ADC CTM — alpha-0.6.2

**Current increment — alpha-0.6.7:** [Transfer Tokens](TRANSFER_TOKENS.md) unifies bounded opaque token validation and owning values across Core, CTM/RCM and peer errors. Token bytes, including Unicode/edge spaces/backslashes, survive immutable routing and real ncdc boundary handshakes. Independent requests may reuse tokens; no token registry or inferred correlation is added.

**Increment delivered in alpha-0.6.6:** [Client Capabilities](CLIENT_CAPABILITIES.md) unifies owning IPv4/IPv6 TCP modes and advisory peer protocols. Shared explicit CTM/RCM selection preserves wire intent; a separate trusted-originator helper chooses CTM or RCM and refuses passive/passive. Live INF changes, stale-plan rechecks and real ncdc mode transitions are covered.

**Increment delivered in alpha-0.6.5:** [Protocol Negotiation](PROTOCOL_NEGOTIATION.md) gives Transfer Core owning protocol values and explicit ADC/1.0 plaintext / ADCS/1.0 TLS intent. Pure preference/capability selection never downgrades required TLS. Live CTM/RCM preserve the selected name and extension compatibility; peer failure replies retain opaque tokens. Real ncdc tests cover certificate-checked TLS peer handshakes over both hub transports, alongside the terminal-backpressure regression.

**Increment delivered in alpha-0.6.4:** [Connection Routing](CONNECTION_ROUTING.md) consolidates D/E CTM and RCM admission and delivery into one SID router, one immutable packet type and one bounded queue path. Recipients are fixed ready-session lifetimes: D targets only, E targets then independently echoes to the source. Tokens never select recipients; existing roles, wire validation, errors and privacy remain. Mixed-pair tests use the same token without crossing audiences.

**Increment delivered in alpha-0.6.3:** [ADC RCM](ADC_RCM.md) adds owning D/E reverse-request validation and live target/echo routing through Transfer Core, with per-family passive-source/active-target checks, stable lifetime/address admission and shared bounded queues. CTM/RCM share wire-field validation. Real ncdc covers RCM→CTM and peer handshakes in both roles; no pending-token registry or file-data handling is claimed.

**alpha-0.6.2 local validation (2026-11-13):** All **128/128 CTest suites** passed in Debug, Release and ASan/UBSan with required integration tools and default leak detection, including typed/native CTM faults/queues/lifetimes, fragmented IPv4/IPv6 ADC/ADCS, actual ncdc CTM-to-CSUP/CINF/CGET peer handshakes, prior Search/UDP, MariaDB and Apache HTTPS. No compiler warnings. WebAdmin build/typecheck, **12 Node tests**, **10 Chromium tests** and real C++ API integration passed. Metadata, UTF-8, language parity/local links, changed Python syntax, example INI and whitespace checks passed. Remote CI, full OS/client coverage and production load remain unverified.

Based directly on main `4513c4c6fda20269a78b2ad711ea16d204f412a9`, after the Transfer Core merge. Existing history/code are preserved; this feature branch is published separately from main.

## Implemented behavior

The running hub now validates and routes **DCTM** to its ready target and **ECTM** to target then sender, using [Transfer Core](TRANSFER_CORE.md). Bystanders receive no coordination frame. RCM remains explicitly unimplemented. A valid CTM has no hub success response; queue acceptance is not a successful connection or file transfer.

The [ADC specification](https://adc.sourceforge.io/ADC.html) defines CTM with positional protocol, port and token fields, sent by a TCP-active client. D delivery targets one peer; E also echoes the origin. Protocol names use viewable single-byte characters. [ADCS client-to-client encryption](https://adc.sourceforge.io/ADC-EXT.html) is separate from hub TLS.

```text
DCTM ABCD EFGH ADC/1.0 1515 opaque-token
ECTM ABCD EFGH ADCS/1.0 01515 opaque\stoken XXfuture
```

The hub forwards the original checked fields/order, numeric spelling, token and unknown named extensions, through canonical ADC escaping and one immutable shared frame. It neither inserts a token nor rewrites the requested protocol/port. Clients obtain the peer address from public INF, open their own connection and negotiate the requested protocol. The hub does not open a CTM port, proxy peer data, perform DNS/reachability probes or read client files.

## Typed codec

`adc/ctm.hpp` provides owning CtmRequest, parse_ctm and validate_ctm. The first preserves generic ParseError for raw syntax failures; semantic failures use CtmError. Native Message validation checks headers/layout, serializes/reparses through the shared codec, then validates fields, so inconsistent hand-built objects cannot bypass the wire grammar.

| Input | Rule |
| --- | --- |
| Scope/header | D or E CTM, exact command/kind, valid source and target SIDs, no feature selectors; inferred or explicit positional count 3. |
| Protocol | Required 1..32 bytes of visible ASCII (33..126; DEL is already forbidden by the codec). Unknown valid names are preserved. |
| Port | Parsed signed 64-bit integer, then restricted to 1..65535. No sign-plus syntax; legal leading zeros remain on the wire. |
| Token | Required 1..128 decoded bytes, UTF-8 already in NFC, without controls/format/line/paragraph separators. Valid spaces/backslashes survive escaping. |
| Extension fields | Shared named-parameter grammar; unknown fields and repeats retain ordering/content. A named TO extension never replaces the positional token. |
| Bounds | At most 128 parameters, 2048 total decoded parameter bytes, plus the existing 16384-byte frame limit. Bounds precede owning copies. |

These byte ceilings are local policy, not ADC-wide maxima. Transfer Core's trusted native Request remains protocol-neutral; the ADC adapter applies the stricter protocol-name wire rule. No protocol allowlist, TLS downgrade, peer-support claim or transfer-token registry is introduced. `ADC/1.0`, `ADCS/1.0` and unknown valid names are routed identically; peers decide support, TLS and any error response. CTM tokens are not Search tokens, credentials or hub-side connection authorization.

## Admission and routing

Existing NORMAL/readiness/source-ownership checks retain precedence. CtmRouter then validates the typed message, checks a usable TCP-active source path, resolves the ready target through Transfer Core, and refuses self-connections. Exact public SU TCP4/TCP6 and a usable numeric address are required. UDP features/ports, SUP, client privilege claims and hub ADCS transport do not grant TCP activity.

Transfer Core now offers allocation-free `tcp_address(owner, family)` for a ready lifetime. CtmRouter chooses IPv4 when active, otherwise IPv6, to bind its admission snapshot; if neither is available, it refuses the request. This is **internal admission metadata**: CTM has no address-family field, and the hub does not force which public endpoint the receiving client chooses. A plan does not silently switch its selected family after preparation. New requests use current INF state.

`adc/ctm_routing.hpp` defines CtmRouter, RoutedCtm and immutable SharedCtmFrame. The owning plan binds both session lifetimes, source/target keys and selected listener address. Copies share Transfer Core's one attempt; a retained router-domain token also blocks foreign/address-reused routers. Foreign/missing-sink errors do not consume it. All other dispatches consume before rechecking ready/source/target lifetimes and current selected listener activity/address. Disconnect, closing, SID reuse or changed endpoint cannot retarget an old plan. Cancellation is explicit; dispatch allocates nothing after sink construction, including with maximum-size protocol/token strings.

Target insertion occurs first. E echo is an independent copy, so it can still queue when the target queue refuses. Before echo, the source lifetime/key/selected endpoint are checked again; if the target sink closed/replaced the source or changed its connectivity, echo is canceled. A failed preparation or unavailable target produces no original echo. Reports count queued/refused/canceled copies, not client acceptance. There is no broadcast/fallback path.

LoginHub stores the shared packet in its existing bounded FIFO queues, with independent partial-send offsets and full logical byte charges. Appending CTM does not move a pinned TCP/TLS front. Preparation allocation failure isolates the source. Target enqueue cap/allocation failure isolates only that target; echo failures affect the echo recipient. Trusted callback exceptions propagate after a consumed attempt, so a caller cannot blindly retry partial work.

## Errors, privacy and limits

| Condition | Live behavior |
| --- | --- |
| Pre-login/unready source | Existing fatal 244. |
| Forged source SID | Existing fatal 240, before CTM work. |
| Semantic CTM failure, unsupported B/F scope or self-target | Recoverable `100 Invalid connection request FC<FOURCC>`. |
| Source lacks an eligible TCP-active path | Recoverable `100 CTM sender is not TCP active FC<FOURCC>`. |
| Missing/closing target | Recoverable `140 CTM recipient unavailable FC<FOURCC>`. |
| RCM | Existing recoverable `125 Command not implemented`. |

Raw syntax errors remain with the transport codec. Other invalid header directions retain state-machine rejection; accepting a typed object does not authorize sending it. Diagnostics never reflect protocol/token/extension payload. Valid/invalid CTM does not enter chat/search counters, payload logs, SQL, API snapshots or WebAdmin export; generic ADC frame/byte totals still apply. No transfer history, settings, metrics, new endpoint, dependency or migration is added. Existing transport frame/IP/output limits remain; transfer-specific quota/permission policy is a later increment.

## Verification and remaining stages

`adc.ctm` covers exact roundtrip/ownership, protocol/port/token/aggregate/native bounds, unsupported scopes, target/echo ordering and shared storage, copies/cancel/replay/throwing sinks, SID reuse, source closure during target insertion, endpoint/family changes, fault injection, allocation-free dispatch, pinned output and recipient/source failure isolation.

`adc.ctm.process` covers fragmented live IPv4/IPv6 ADC/ADCS D/E delivery, public INF TCP/UDP changes, unknown/ADCS protocols, fixed errors and recovery, bystander privacy, target reconnect, pre-login/forged source priority and API/log/chat/Search separation. The previous Transfer Core boundary regression now expects checked passive-CTM denial while retaining unimplemented RCM.

Real `adc.ctm.ncdc` / `adcs.ctm.ncdc` make ncdc TCP-active and request another peer's file list. The peer receives the actual DCTM through the hub, uses its port/token in a separate TCP connection to ncdc, completes CSUP/CINF and observes CGET. Chat then remains usable. This proves live CTM coordination and a client handshake; no successful file-body download or peer TLS negotiation is claimed by this test.

Later stages still need RCM handling, bounded pending token correlation/timeouts, transfer-specific security/permissions, peer protocol/TLS policy and monitoring. The hub does not implement GET/SND file-data serving, NAT traversal or client slot/content authentication. Results are recorded in [CHANGELOG](../../CHANGELOG.md); remote CI, the full OS/client matrix and production load require separate verification.
