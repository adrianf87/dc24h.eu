# Search Routing — alpha-0.5.6

**Current increment — alpha-0.5.12:** [Search Configuration and Statistics](SEARCH_CONFIGURATION.md) exposes 19 cumulative hub SCH/RES counters, a lazy stored-token-window gauge and all 16 effective startup search settings through the authenticated read-only API and bilingual WebAdmin Search page. Queue acceptance is distinct from delivery; direct client UDP is excluded. No payload/history or configuration write endpoint is added.

**Increment delivered in alpha-0.5.11:** [Search Security](SEARCH_SECURITY.md) adds connection-lifetime query/sent-result/received-result count and byte quotas, bounded denial notices, guest/global permission policy, trusted session grants and configurable active-search limits. Admission precedes routing/token publication. Optional strict result-token policy supplements existing owner/generation checks; direct client UDP remains outside hub enforcement.

**Increment delivered in alpha-0.5.10:** [Active/Passive](ACTIVE_PASSIVE.md) projects checked I4/I6/U4/U6 into public Users and adds allocation-free per-family TCP/UDP reply-path metadata to Search Core and immutable plans. Passive DRES remains hub-routed; active URES travels directly between clients. Dynamic INF changes, endpoint ownership and real ncdc UDP sending/receiving are tested.

**Increment delivered in alpha-0.5.9:** [Result Routing](RESULT_ROUTING.md) forwards checked DRES only to its ready target over ADC/ADCS. Plans bind session lifetimes and, when TO is present, the target owner’s active search generation. Tokenless results remain supported. Shared bounded queues preserve original fields without public fanout, sender echo or administration payloads.

**Increment delivered in alpha-0.5.8:** [ADC RES](ADC_RES.md) adds owning typed DRES parsing and live ADC/ADCS validation of names, sizes, slots, opaque tokens and canonical TTH roots after session/source checks. Semantic errors are recoverable and private; valid results explicitly report pending result routing. Token correlation, result delivery and UDP remain future increments.

**Increment delivered in alpha-0.5.7:** [Feature Routing](FEATURE_ROUTING.md) compiles checked FSCH selectors once and uses current public INF SU for all-required/no-excluded matching. SUP negotiation is independent. Live capability changes, issuer membership, unknown revisions and limits are covered over ADC/ADCS and real ncdc. RES/result routing remains a later increment.

Based directly on main `a5824cbc2a5d4ab1c8d6ddf99e5af87bfafe30cb` after the alpha-0.5.5 merge. Earlier Search Core, SCH, filters, TTH and tokens are preserved. Code/wire diagnostics remain EN/US UTF-8, with mirrored US/PL documentation.

## Live request delivery

`adc/search_routing.hpp` and `src/adc/search_routing.cpp` add SearchRouter, RoutedSearch and an immutable shared SearchFrame. LoginHub now owns Search Core and SearchRouter and forwards validated SCH through its existing bounded TCP/TLS queues. Valid searches no longer receive `Search routing not implemented`; receipt by eligible clients is verified with raw mixed ADC/ADCS peers and actual ncdc. There is no success acknowledgment or invented result.

The [ADC message-type rules](https://adc.sourceforge.io/ADC.html) determine recipients:

| Scope | Recipients |
| --- | --- |
| B | All currently ready users, including the source |
| F | Currently ready users whose public INF SU supports all `+` and none of the `-` selectors; the source is included only when it matches |
| D | The currently ready target only |
| E | Target and source, with one copy when they are the same lifetime |

B/F retain deterministic SessionManager snapshot order. D/E select the target lifetime first, then the source for E. The source, recipients and target are lifetime IDs, not fd numbers or reusable SIDs. Core adds the protocol-independent `Audience`/`Scope` argument to prepare/start and resolves ready target keys. Missing/unready targets are refused before registration/queueing; directed searches never fall back to broadcast. A closed target cannot be replaced by a new session that reuses its SID.

Selection excludes anonymous, negotiating, identified-but-welcome-pending, closing and closed sessions. Dispatch rechecks the source and every planned recipient's readiness/lifetime/current features. Later joins are not added; lost features make a planned recipient unavailable. Source closure stops the remaining fanout. These checks preserve the existing final-own-INF readiness boundary rather than treating protocol NORMAL alone as ready.

## Checked wire data and shared queues

SearchRouter first validates sender readiness/source ownership, then applies existing full SCH semantic/header validation. AN/NO/EX, integer aliases, sizes, TY, opaque TO, canonical TR, unknown fields/order and selectors are preserved in the owning Message. Generic serialization builds the shared output encoding; validation retains the existing bounded serialization/reparse check. No fields are replaced by a normalized Filter view; clients search their own shares. Unknown fields remain syntax/size checked and forwarded according to ADC extension behavior. They originate in the client's SCH, not in server identity/profile storage.

One immutable Message/wire packet is shared by all selected output queues. Each recipient retains independent TCP/TLS offsets and the full logical byte charge. Partial sends, pinned TLS retries, other-peer appends and source disconnection do not mutate or free a surviving queue's front. Existing per-client/aggregate caps apply; a slow/refusing recipient is closed in isolation and healthy recipients can continue. Queue allocation failure is caught at that recipient; preparation/diagnostic failure closes only the source and removes its token records. Acceptance means queued, not delivered/read.

Copies of RoutedSearch share one dispatch-attempt state. Empty sinks/foreign-Core plans are refused without consuming a valid attempt; once dispatch starts, even a copied publication cannot run it again. Moved-from frames are refused. A sink exception propagates after possible partial delivery and consumes the attempt; trusted callers must explicitly cancel if appropriate and never blindly retry. Core's lower-level stateless API remains available to trusted modules; this one-attempt guarantee applies to the ADC routing wrapper.

Preparation allocates the checked frame and attempt state before publishing an active token. Allocation failure cannot leave a newly reserved TO without a completed routed publication. Dispatch's Core sink is constructed from a reference wrapper and allocates nothing after the caller's Sink setup; queue allocation remains transport-owned. Recipient snapshots are bounded by the configured session limit and gathered through existing bounded pages. Shared encoding does not remove per-recipient logical charging or promise a production-load capacity.

## Active TO and cleanup

A valid request with TO uses Core::start and the alpha-0.5.5 session-lifetime/token registry. Default active limits remain 4096 globally, 8 per owner, with a 60-second lifetime. `LoginLimits::search_tokens` allows trusted in-process construction overrides; no new INI/CLI switch is introduced. Duplicate active tokens and capacity/exhaustion failures are recoverably refused without fanout. Names, TTH and additional criteria remain in the prepared query unchanged.

Absent TO remains valid and is forwarded unchanged through stateless preparation; the hub does not fabricate a client token. A valid empty feature selection produces no copies or success message. After dispatch, a token is retained only when at least one queue accepted a copy; zero-queue/refused/stale dispatch completes it. There is no automatic retry of a partially delivered search.

Successful token-bearing routing keeps the response window active until expiry or owner cleanup. Expiry is lazy through token registration/lookup; disconnect explicitly forgets the owner's entries before releasing its model/SID. Full registry admission also reclaims disconnected owners as before. This stage adds no automatic Network timer or full-registry scan on every tokenless request. A queued frame may outlive the source while remaining ahead of its ordered QUI; no stale owner can start another dispatch.

Internal generations prevent stale trusted handles from canceling replacement searches. Original wire TO is preserved, so a reused TO still cannot distinguish a late old result by itself. This release does not accept/correlate RES or claim to solve that ambiguity. Search result parsing/routing, UDP exchange, successful end-to-end results, a hub file index, search-specific rate policy/history and search administration remain later increments. Existing general frame/IP/admission and bounded output policies remain authoritative; active-token caps are not a search rate limiter.

## Diagnostics and privacy

| Input/result | Wire behavior |
| --- | --- |
| Valid routable SCH | Eligible clients receive the checked SCH; no ISTA success/refusal acknowledgment |
| No recognized criterion (including token-only) | Silently ignored, no registration or forwarding |
| Invalid semantic fields/selectors | Recoverable `100 Invalid search query` with fixed FOURCC |
| Unavailable D/E target | Recoverable `140 Search recipient unavailable`, no fallback/echo |
| Active same-owner TO | Recoverable `100 Search token already active` |
| Active-search cap/exhaustion | Recoverable `100 Active search limit reached` |
| Wrong source / pre-ready request | Existing fatal 240 / 244 before routing work |
| RES | Existing explicit `125 Command not implemented`; no result forwarding |

Intended B/F/D/E recipients now receive search text, hashes, tokens and unknown request fields as required for search execution. Those payloads are not echoed in diagnostics or exported to logs, chat statistics, database, API or WebAdmin. No SQL migration, endpoint, runtime configuration field, operator-language message or frontend search control is added. Existing US/PL operator/UI behavior is preserved; release metadata advances together to alpha-0.5.6.

## Verification

Local verification for **alpha-0.5.6** on 2026-11-12: all **102/102 CTest suites** passed in Debug, Release and ASan/UBSan with required integration tools and default leak detection, including routing scopes/lifetimes/tokens/queue/fault tests, actual mixed ADC/ADCS fanout, real ncdc name/filter/TTH receipt by another client, MariaDB and Apache HTTPS. Builds emitted no compiler warnings. WebAdmin production build/typecheck, 4 Node tests, 6 Chromium tests and real C++ API integration passed. Release metadata, UTF-8, documentation language parity/local links, modified Python syntax, shipped INI and whitespace checks passed. These recorded results are local; remote CI is tracked in GitHub Actions. The full supported OS/client matrix and production load remain unverified.

`search.routing` checks exact B/F/D/E scope, self-collapse, current features, unready exclusion, target lifetime/SID reuse, unknown fields/Unicode/numeric spelling/TTH preservation, shared packet identity, 270-recipient paging, copied-publication replay guards, foreign/empty sinks, expiry/duplicate/empty-selection cleanup, thrown sinks, allocation rollback and no-allocation dispatch. Actual LoginHub checks shared pinned fronts, independent offsets/full charges, source cleanup, a slow directed recipient's refusal isolation and zero chat accounting.

`search.routing.process` uses fragmented mixed ADC/ADCS delivery with ready and pending peers, explicit SU selectors, positive/negative/empty feature scopes, direct/echo/self audiences, token duplicates/caps/independent owners, disconnect/reconnect cleanup, unavailable targets, source/state precedence, unsupported RES and log/API privacy. Existing SCH/Core/token process fixtures now verify routed copies and no copies for refused requests rather than expecting the old valid-search refusal.

Real ncdc name/filter and TTH searches remain enabled on ADC and ADCS. The raw observer advertises TCP4 and checks receipt of the actual source SCH; plaintext capture verifies fields/echo and ADCS preserves encryption. Tests do not claim results or file transfers. Reproduce with normal Debug/Release/ASan builds, `ctest --test-dir build-debug -R 'search\.|adc.sch|ncdc' --output-on-failure -j 4`, then the complete suite. See [TESTING.md](TESTING.md), [Search Tokens](SEARCH_TOKENS.md), [ADC SCH](ADC_SCH.md), [ROADMAP.md](ROADMAP.md) and [CHANGELOG.md](../../CHANGELOG.md). The branch is not merged automatically.
