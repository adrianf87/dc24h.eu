# ADC RCM — alpha-0.6.3

**Current increment — alpha-0.6.4:** [Connection Routing](CONNECTION_ROUTING.md) consolidates D/E CTM and RCM admission and delivery into one SID router, one immutable packet type and one bounded queue path. Recipients are fixed ready-session lifetimes: D targets only, E targets then independently echoes to the source. Tokens never select recipients; existing roles, wire validation, errors and privacy remain. Mixed-pair tests use the same token without crossing audiences.

**alpha-0.6.3 local validation (2026-11-13):** All **132/132 CTest suites** passed in Debug, Release and ASan/UBSan with required integration tools and default leak detection, including RCM native/fault/queue/lifetime checks, fragmented IPv4/IPv6 ADC/ADCS, real ncdc RCM→CTM and peer handshakes in both roles, previous CTM/Search/UDP, MariaDB and Apache HTTPS. No compiler warnings. WebAdmin build/typecheck, **12 Node tests**, **10 Chromium tests** and real C++ API integration passed. Metadata, UTF-8, language parity/local links, changed Python syntax, example INI and whitespace checks passed. Remote CI, the full OS/client matrix and production load remain unverified.

Based directly on main `4f86b20c407047ae89c537e2c77b7dad7d833633`, after the ADC CTM merge. Existing code/history are preserved; this feature branch is published separately from main.

## Implemented behavior

The hub now accepts checked **DRCM** for target-only reverse negotiation and **ERCM** for target then sender echo. It uses the existing [Transfer Core](TRANSFER_CORE.md), [CTM handler](ADC_CTM.md) and bounded shared output queues. No broadcast or bystander receives the request; no hub success acknowledgment is generated.

The [ADC specification](https://adc.sourceforge.io/ADC.html) defines RCM with positional protocol/token, used by a passive client to request coordination from an active peer. A supporting peer responds with CTM and preserves the protocol/token. D addresses the peer; E also echoes the origin. The hub does not synthesize that response or choose a port for the active client.

```text
DRCM ABCD EFGH ADC/1.0 opaque-token
ERCM ABCD EFGH ADCS/1.0 opaque\stoken XXfuture
```

The receiving active client supplies its own CTM port. The hub validates/routes that CTM through the already implemented adapter. Actual client sockets, CSUP/CINF, optional peer TLS and file commands remain outside the hub's data path. [ADCS peer encryption](https://adc.sourceforge.io/ADC-EXT.html) is independent of the hub transport. No plaintext downgrade, automatic protocol negotiation or peer reachability claim is introduced.

## Typed codec and shared field boundaries

`adc/rcm.hpp` defines owning RcmRequest, RcmError, parse_rcm and validate_rcm. Generic raw syntax failures remain ParseError; RCM semantic failures use RcmError. The request owns the original checked Message, with protocol/token accessors. No port accessor, fabricated value or implicit CTM conversion exists.

| Input | Rule |
| --- | --- |
| Header | D/E RCM, exact command/kind, valid source/target SIDs, no selectors, inferred or explicit positional_count 2. |
| Protocol | Required 1..32 bytes, visible ASCII 33..126. Unknown valid names are retained. |
| Token | Required 1..128 decoded bytes, valid UTF-8 already in NFC; no control/format/line/paragraph separators. Spaces/backslashes survive canonical escaping. |
| Extensions | Named-field grammar after two positional parameters. Unknown fields/repeats retain order/content. A named TO or port-like extension does not replace the positional token or create a port. |
| Bounds | 128 parameters, 2048 total decoded parameter bytes, plus existing 16384-byte frame ceiling. |

The private `src/adc/connection_fields.hpp` centralizes CTM/RCM D/E header, parameter/aggregate, protocol-name and opaque-token checks. CTM keeps its independent port validation and three-position layout; its public API/behavior is unchanged. Native Message validation serializes/reparses through the shared wire codec before owning copies, preventing malformed text/extensions/layout from bypassing syntax. Field byte ceilings are local policy rather than ADC-wide maxima. Allocation failure leaves models unchanged and propagates to the adapter.

## Reverse roles and family selection

The existing NORMAL/readiness/source-SID checks precede RCM work. RcmRouter validates the request, resolves an existing ready target and refuses self-targets. Transfer Core's new `resolve_target(key)` returns only ready lifetime IDs without allocating or copying profiles.

A reverse plan requires a family where the **source is passive and the target is TCP-active**. TCP activity requires exact public TCP4/TCP6 plus a usable numeric public address under the existing Users policy. UDP declarations/ports do not make either peer TCP-active. Source and target need not use the same hub transport.

RcmRouter prefers eligible IPv4, otherwise eligible IPv6; it rejects when neither exists. Activity is per family: a source active on TCP4 may request a reverse path through TCP6 when passive there and the target is TCP6-active. Fully active source/no compatible passive family and passive/passive combinations are refused. This selection binds internal admission metadata; RCM has no family field and the hub does not force clients to use a particular endpoint or infer outgoing family support.

The resulting immutable Transfer Core plan captures both lifetime IDs/keys, the reverse intent and the active target's selected listener address. It has **no port**. A changed endpoint/family is not silently substituted into an old plan; a newly prepared request uses current public INF.

## Dispatch, queues and failures

`adc/rcm_routing.hpp` supplies RcmRouter, RoutedRcm, immutable SharedRcmFrame and queued/refused/canceled copy reports. Router/Core domains reject foreign plans, including address reuse. Copies share one attempt. Foreign-router/missing-sink errors do not consume it; other attempts consume before rechecking ready lifetimes/keys, source passivity, target activity and the selected target address. Disconnect, closing and SID replacement cannot retarget queued intent. Explicit cancel works only on a prepared owned plan.

D calls the target sink once. E calls it first, then independently considers source echo. Before echo, source lifetime/key and selected-family passivity are checked again: source closure or activation during the target callback cancels that echo. A target queue refusal can coexist with a successful source echo; echo is not delivery confirmation. Preparation failure, unavailable target or failed pre-dispatch recheck produces no original echo. A callback exception consumes the attempt even if earlier work was partially accepted, so callers must not blindly retry.

One immutable packet is shared across selected queues with independent offsets and full logical byte charges. Existing FIFO, partial sends, pinned TLS fronts and per-client/aggregate caps remain. Dispatch/lookup/cancel allocate nothing after sink construction, including maximum-length protocol/token strings. Preparation allocation failure isolates the sender; target queue allocation/cap failure isolates that target. Echo queue failure affects its recipient, without public fallback.

## Diagnostics and privacy

| Condition | Live behavior |
| --- | --- |
| Pre-login/unready source | Existing fatal 244. |
| Forged source SID | Existing fatal 240 before RCM handling. |
| Semantic failure, B/F RCM or self-target | Recoverable `100 Invalid connection request FC<FOURCC>`. |
| No compatible passive-source/active-target family | Recoverable `100 RCM requires passive sender and active recipient FC<FOURCC>`. |
| Absent/closing target | Recoverable `140 RCM recipient unavailable FC<FOURCC>`. |

Raw syntax failures remain in the transport codec; other invalid directions retain state-machine rejection. No diagnostic reflects protocol/token/extension content. RCM/CTM do not enter chat/Search counters, payload logs, SQL, API snapshots or WebAdmin export. Generic ADC frame/byte totals still count them. No transfer metrics/history/page, new INI key, dependency, endpoint or migration is added. Existing transport/IP/frame/output limits remain; transfer-specific permissions/quotas are not claimed.

This stage **does not add a pending RCM-to-CTM token registry**. The hub preserves each frame's checked data; peers generate/copy and validate negotiation tokens. It does not authenticate a responder or enforce that every CTM token/protocol matches a prior RCM, deduplicate attempts, expire pending negotiations, validate file claims or serve GET/SND data. Those are separate future policies/modules.

## Verification

`adc.rcm` checks raw/native/semantic/text/aggregate bounds, exact roundtrip and ownership, absent ports, per-family roles and UDP independence, shared target/echo storage/order, copies/replay/cancel, exceptions after partial echo work, source activation/closure, target SID/address/family changes, allocation faults, allocation-free dispatch/lookup and pinned queue/source-target failure isolation. Existing CTM unit tests protect the shared validator refactor.

`adc.rcm.process` exercises fragmented IPv4/IPv6 ADC/ADCS D/E routing, client-generated CTM replies with identical protocol/token, INF role changes, fixed errors/recovery, reconnects, bystanders, source/state precedence and API/log/chat/Search privacy. Previous Transfer Core/CTM boundary regressions now expect checked incompatible-role denial rather than unimplemented RCM. The ncdc boundary test makes both peers passive and confirms the diagnostic plus working chat.

Real `adc.rcm.ncdc` / `adcs.rcm.ncdc` test both roles. Passive ncdc emits DRCM; an active peer replies with DCTM carrying its exact protocol/token, accepts ncdc's separate TCP connection and validates CSUP/CINF token plus CGET. Then active ncdc receives a passive peer's DRCM, emits matching DCTM and accepts a token-bearing peer handshake. The harness allows bounded informational CSTA referrer announcements before CINF. Chat remains usable afterward. These tests prove coordination/handshake interoperability, not successful file-body download or peer TLS certification.

Results are recorded in [CHANGELOG](../../CHANGELOG.md). Remote CI, the full OS/client matrix and production load require separate verification.
