# Connection Routing — alpha-0.6.4

**alpha-0.6.4 local validation (2026-11-13):** All **134/134 CTest tests** passed in each of Debug, Release and ASan/UBSan with required integration tools and default leak detection, including the common SID matrix/fault/lifetime checks, same-token parallel mixed ADC/ADCS IPv4/IPv6 pairs, real ncdc CTM/RCM peer handshakes, previous Search/UDP, MariaDB and Apache HTTPS. No compiler warnings. WebAdmin build/typecheck, **12 Node tests**, **10 Chromium tests** and real C++ API integration passed. Metadata, UTF-8, language parity/local links, changed Python syntax, example INI and whitespace checks passed. Remote CI, the full OS/client matrix and production load remain unverified.

This increment starts from main `79b7c1ffa82d6a5389ddc92bd13e3ffa61eb95a1` after alpha-0.6.3. It unifies the existing CTM and RCM routing implementation. The feature branch preserves history and is published separately from main.

## Admission and recipients

`adc::ConnectionRouter` is the common owner-thread engine for checked DCTM, ECTM, DRCM and ERCM. LoginHub owns one router over its Transfer Core and SessionManager. It has one command handler and one shared connection-frame alternative in the bounded output queue. Other ADC commands are rejected by this engine; existing chat, search and status routing remain separate.

The source SID must belong to the ready sending session. The exact target SID resolves through the session registry to a ready session lifetime. Self-targets are invalid. Missing, unready or closing targets cannot select a replacement. CID, nickname, socket descriptor and token never choose recipients. In particular, equal tokens in simultaneous independent pairs do not join their audiences or create a pending connection record.

| Scope | Frozen audience, in order | Delivery |
| --- | --- | --- |
| DCTM / DRCM | Target lifetime | Target only |
| ECTM / ERCM | Target lifetime, source lifetime | Target attempt, then independently checked source echo |

`RoutedConnection::recipients()` exposes this bounded audience as a const span of lifetime IDs. Dispatch can cancel stale recipients, but cannot expand the audience, broadcast, fall back to another user or resolve a reused SID to a new login. Failed preparation or stale-target dispatch produces no echo. Target queue refusal can still permit the independent source echo. Queue acceptance acknowledges coordination output, not peer receipt, connection establishment or file delivery.

## Validation and roles

The existing owning CTM and RCM codecs remain the wire boundary. D/E layouts, source/target SIDs, visible protocol names, CTM port, opaque NFC token, extension preservation, decoded budgets and frame ceilings are unchanged. A valid unknown protocol is preserved for peer negotiation. The hub neither rewrites tokens nor downgrades ADCS. RCM never gains a fabricated port or hub-generated CTM reply.

CTM checks a usable TCP-active source, choosing IPv4 before IPv6, then prepares the target. RCM first resolves a distinct ready target, then chooses IPv4 before IPv6 where the source is passive and the target TCP-active. Public address ownership and exact TCP4/TCP6 eligibility remain shared Users/Transfer Core rules; UDP activity does not grant TCP activity. Family selection is internal metadata, with no new ADC field. Source/state fatal precedence and existing command-specific recoverable 100/140 diagnostics remain intact.

The [ADC specification](https://adc.sourceforge.io/ADC.html) defines direct/echo message headers and CTM/RCM connection negotiation. The implementation details and bounds here describe this repository's supported subset.

## Lifetimes, callbacks and queues

Each plan retains a router domain, Core domain, immutable request, selected endpoint and original source/target lifetime IDs. Copies share one Core attempt. Cancel, replay, recursive dispatch, foreign routers and reconstruction of a router at the same memory address cannot grant another attempt. Empty sinks and foreign plans are rejected before consuming an otherwise valid plan. Dispatch rechecks readiness, SID ownership, connectivity and the captured listener address.

One immutable `ConnectionFrame` owns the checked message and serialized wire bytes. Target and optional source queues share this exact frame while retaining independent FIFO offsets and logical byte charges. Existing partial TCP/TLS output pinning and queue limits remain. Dispatch retains frame and echo metadata before the target callback; even replacing the caller's routed value with another command inside that callback cannot redirect the original echo. A closed or replaced source cancels echo; changed CTM endpoint or RCM source activation also cancels it. Callback exceptions consume the attempt and propagate without replay. Dispatch/cancel allocate nothing after sink construction.

Preparation allocation failure isolates the source. A recipient enqueue allocation failure or output-cap refusal isolates that recipient; the existing close/drain rules still apply. No registry mutation is required to prepare routing, and an allocation failure cannot partially publish an audience.

## Public C++ API transition

`CtmRouter` and `RcmRouter` remain command-restricted facades with prepare/dispatch/cancel methods and separate domains. Their implementation delegates to ConnectionRouter. Existing Ctm/Rcm frame, routed-plan, report and error names alias the common connection types. Ordinary calls remain available, and the earlier native suites compile unchanged.

The old type identities are now unified: external overloads or variants that contain both former frame/plan/error types may need to use the common type once. Rebuild dependent C++ code; a stable binary ABI is not promised. Code that handles both commands should prefer ConnectionRouter directly.

## Scope and verification

No configuration key, dependency, database migration, endpoint, WebAdmin page or transfer counter is added. Payloads, tokens and per-user connection history remain absent from logs, SQL and API/WebAdmin. Chat/Search counters do not count connection requests; transport byte/frame/output limits still apply. Pending-token registration/correlation/deadlines, new transfer security policies, peer sockets and file data remain outside this increment.

`transfers.routing` checks every distinct source/target pair in a four-SID matrix for both commands and scopes, exact frozen audiences, immutable frame identity, command allowlisting, error priority, domain reuse, copied/canceled/replayed plans, actual forced SID reuse and callback replacement across CTM/RCM. Fault injection checks preparation rollback and allocation-free dispatch, including maximum protocol/token sizes, refusal and exceptions. Earlier CTM/RCM role, lifetime and output-queue suites remain enabled.

`transfers.routing.process` exercises IPv4 and IPv6 with mixed ADC/ADCS clients: independent CTM and RCM pairs deliberately share one token, client replies remain target-only, bystanders receive nothing, stale SIDs fail after reconnect, forged sources close privately, and chat recovers. API/log assertions check payload privacy and unchanged Chat/Search aggregates. Existing real ncdc ADC/ADCS CTM and RCM tests continue through direct CSUP/CINF/CGET peer handshakes in both roles. See [Build](BUILD.md), [Testing](TESTING.md), [Transfer Core](TRANSFER_CORE.md), [ADC CTM](ADC_CTM.md) and [ADC RCM](ADC_RCM.md).
