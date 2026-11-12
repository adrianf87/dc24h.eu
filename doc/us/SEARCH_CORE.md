# Search Core — alpha-0.5.1

**Current increment — alpha-0.5.8:** [ADC RES](ADC_RES.md) adds owning typed DRES parsing and live ADC/ADCS validation of names, sizes, slots, opaque tokens and canonical TTH roots after session/source checks. Semantic errors are recoverable and private; valid results explicitly report pending result routing. Token correlation, result delivery and UDP remain future increments.

**Increment delivered in alpha-0.5.7:** [Feature Routing](FEATURE_ROUTING.md) compiles checked FSCH selectors once and uses current public INF SU for all-required/no-excluded matching. SUP negotiation is independent. Live capability changes, issuer membership, unknown revisions and limits are covered over ADC/ADCS and real ncdc. RES/result routing remains a later increment.

**Follow-up — alpha-0.5.6:** [Search Routing](SEARCH_ROUTING.md) forwards checked SCH over real ADC/ADCS: B/F broadcasts and lifetime-bound D/E delivery share one immutable packet with independent bounded queues. Supplied TO is registered in Search Core; absent TO remains valid. Source/state/feature checks, one-attempt dispatch and token cleanup are enforced. RES/result routing remains a later increment.

**Follow-up — alpha-0.5.5:** [Search Tokens](SEARCH_TOKENS.md) adds bounded session-lifetime/TO registration, local search generations, deadlines, cancellation and explicit active-search APIs in Core. Stateless preparation stays available; refused live SCH requests do not reserve tokens. Result correlation/routing and search administration remain later increments.

**Follow-up — alpha-0.5.4:** [TTH Search](TTH_SEARCH.md) carries the full 192-bit file root from SCH into immutable Core queries/plans, descriptors and compiled filters. Hash-only and combined queries use exact equality and retain all additional criteria. Real ncdc ADC/ADCS TTH searches are tested; live request/result routing remains a later increment.

**Follow-up — alpha-0.5.3:** [Search Filters](SEARCH_FILTERS.md) adds compiled Unicode case-insensitive name/path predicates, extension alternatives, inclusive/exact byte limits and file/directory types over checked descriptors. Filter-only queries and opaque NFC tokens now project to Search Core. Matching is allocation-free; routing/results, indexes and search administration remain later increments.

**Follow-up — alpha-0.5.2:** [ADC SCH](ADC_SCH.md) now parses received requests and checks typed fields. The live response is now `125 Search routing not implemented`; routing and result exchange remain separate. The alpha-0.5.1 implementation and validation below describe their original delivery scope.

Based directly on main `c6b34ea849ef808a0bfee4a27e0c8ae38cff4a6b`, after the alpha-0.4.12 WebAdmin merge. Code, typed errors and test diagnostics use EN/US UTF-8; documentation is mirrored in US and PL.

## Delivered scope

`dc24h_search` is a separate C++20 library under `include/dc24h/search/` and `src/search/`, built by CMake with the existing Users and Text libraries. It provides a checked textual query value and a hub-side coordination engine: validate a ready source, select current ready session lifetimes, prepare an owning immutable plan, and attempt queue delivery through a trusted caller-supplied sink. It does not keep a hub file index or inspect local/shared files. Clients own their shared-file search implementation.

The [ADC specification](https://adc.sourceforge.io/ADC.html) distinguishes search requests (SCH), results (RES) and feature-based routing. This release supplies the underlying generic model, not an ADC codec or network search handler. `QueryData` is an internal C++ contract; field bounds and validation below are application policy, not a claim about all legal ADC searches. The enum values are not wire numeric values.

Since alpha-0.5.6, production LoginHub uses [Search Routing](SEARCH_ROUTING.md) and Search Core to forward valid SCH after source/state/readiness checks. Supplied TO uses active-search admission; absent TO uses stateless preparation. B/F fanout and lifetime-bound D/E are supported. Valid routing emits no success/refusal acknowledgment; malformed, duplicate/cap and unavailable-target requests receive fixed errors. RES remains unsupported. Result correlation, UDP responses, search-specific rate policy/history and search administration remain later increments.

## Immutable criteria

`Query::create(const QueryData&)` returns `variant<Query, QueryError>`. Successful creation owns an unchanged copy; caller mutation/destruction cannot alter it. `Query::data()` exposes only a const reference. The model does no trimming, normalization repair, case folding, wildcard/regex execution, file matching or SQL. Included/excluded strings, extension order/duplicates and an optional opaque token remain literal criteria for a future adapter/client. Failure returns a typed enum without echoing input. Allocation failure propagates.

| Input | Application rule |
| --- | --- |
| `include` | Optional as of alpha-0.5.3; at least one recognized name/extension/size/type/TTH criterion overall |
| `include` + `exclude` | At most 32 terms combined; each nonempty, not ASCII-space-only and at most 256 UTF-8 bytes |
| Text terms | Already-NFC UTF-8; controls, format characters and line/paragraph separators rejected by existing Text validation; internal/edge spaces are retained |
| `extensions` | At most 16 nonempty values, each at most 32 bytes, with the same text validation and no leading period, slash, backslash or ASCII space |
| `min_bytes`, `max_bytes`, `exact_bytes` | Optional nonnegative signed 64-bit values; minimum must not exceed maximum, and exact must satisfy any supplied bounds; zero and INT64_MAX are valid |
| `type` | Checked `ItemType::any`, `file` or `directory` enum |
| `token` | Optional; if supplied, 1–128 already-NFC UTF-8 bytes, including spaces/Unicode, preserved without interpretation (alpha-0.5.3) |
| Total owned input text | At most 4096 bytes across terms, extensions and token |

Counts, per-field byte sizes, sizes/enums and total bytes are checked before UTF-8/NFC work or copying. Size addition is bounded by the prior field/count checks. The input already owns its storage; a future decoder must separately bound wire bytes/parameters before building it. As of alpha-0.5.3, include may be empty when another recognized criterion is present; token-only/empty input returns `no_criteria`. As of alpha-0.5.4, an optional fixed 24-byte `tth` root is also a criterion, including hash-only queries; it survives copying and plan dispatch without a protocol dependency. `QueryError` distinguishes absent criteria/too-many terms, invalid text/extension/size/type/token and aggregate size failure. No successful query object can be constructed from unchecked mutable data.

## Session selection and plan ownership

`search::Core` borrows a const SessionManager, which must outlive it and its calls; all work stays on that manager's reactor/owner thread. `prepare(sender, source, data, selection)` first checks the local lifetime ID exists, is ready with an identity, and owns the exact source key. Identified-but-welcome-pending and closing sessions cannot search. Invalid source/state is rejected before query copying or fanout work. Preparation returns `variant<Plan, Error, QueryError>`.

Optional `Selection` contains required and excluded feature names. At most 32 selectors total are allowed. Names have four characters: first three uppercase ASCII letters, fourth uppercase letter or digit. Repeated selectors and required/excluded contradictions are rejected, not silently changed. All required features and none of the excluded features must occur in the current public Profile feature vector. This is capability selection, not authentication/authorization or proof of network reachability. No filtering trusts claimed share size, file count, slots, roles or IP addresses.

Without selectors, all current ready users are selected, including the sender. IDs are ordered increasingly and gathered through bounded 256-entry SessionManager pages; plans can cover the registry's maximum 65535 sessions. With selectors, the sender is included only when it satisfies them. A valid plan with zero recipients is allowed. Pending/negotiating/identified/closing/closed users are excluded.

Plan owns its checked Query, source key, Selection and snapshot of recipient lifetime IDs; it does not copy user profiles, nicknames, CIDs, PIDs, IPs or credentials. Later joins are not added. A removed recipient cannot be replaced by a new connection that reuses its SID. A retained opaque routing-domain token binds plans to their originating Core, even after that Core is destroyed and another is constructed at the same memory address. A copied plan retains this ownership contract. Preparation never mutates registry state or queues; allocation failure cannot publish a partial plan.

## Dispatch contract

`dispatch(plan, sink)` checks its routing domain, current sender identity/readiness and a nonempty sink before attempting recipients. Before each sink it rechecks sender and recipient lifetime, readiness and current recipient features. Capability loss makes that recipient unavailable; a late capability gain does not add an ID absent from the plan. Nickname changes alone do not invalidate a stable SID.

The four report fields mean:

| Field | Meaning |
| --- | --- |
| `queued` | Sink returned true; accepted by a caller-owned queue, not delivery/read |
| `unavailable` | Planned lifetime is missing, unready or no longer matches selectors |
| `refused` | Sink returned false; caller owns queue bounds/fault isolation |
| `canceled` | Remaining IDs skipped because sender ownership/readiness disappeared during dispatch |

For a returned report these fields sum to the planned recipient count. Initial sender/domain/sink errors return typed `Error` without calling the sink. A sender closing inside a sink stops the remaining fanout and reports cancellation; recipient failures do not prevent other eligible recipients. Dispatch performs no allocation after Sink construction; the engine itself reads no clocks, files, sockets, logger or database.

Sinks are trusted, synchronous, must not reenter dispatch and should not throw. They may close/remove sessions; the next iteration checks lifetimes again. A thrown sink exception propagates after possible partial acceptance, with no retry. Plans are not a replay-prevention or persistence facility: callers own admission, rate limits, single-attempt scheduling, serialization, per-recipient byte caps and queue exceptions. The Core borrows no mutable iterator across a callback.

## Verification and next increments

Local verification for **alpha-0.5.1** on 2026-11-11: all **92/92 CTest suites** passed in Debug, Release and ASan/UBSan with required integration tools and default leak detection, including Search Core, mixed ADC/ADCS staging-boundary tests, actual ncdc searches, disposable MariaDB and Apache HTTPS. Builds emitted no compiler warnings. WebAdmin production build/typecheck, 4 Node tests, 6 Chromium tests and real C++ API integration passed. Metadata, UTF-8, local documentation links, shipped INI and whitespace checks passed. The ncdc fixture was corrected during targeted testing to account for encrypted proxy traffic and list-tab keyboard navigation. These are local results; remote CI, the full supported OS/client matrix and production load are not claimed.

`search.core` checks Unicode/NFC/control policy, all text/count/token/size limits, exact byte boundaries, no silent normalization, typed query errors, source/state precedence, immutable ownership, multipage membership, zero recipients, feature selectors/updates, late joins, SID reuse, copied/foreign/reused-address domains, missing sink, mid-dispatch sender cancellation and refusal isolation. A C++ allocation campaign verifies failure rollback and no-allocation dispatch; it does not simulate arbitrary failures of the C allocator inside Unicode normalization. Sink exceptions are checked for propagation without retry.

`search.core.process` checks actual mixed ADC/ADCS sessions: SCH/RES remains unsupported, query data never crosses an ordered INF fence, pre-ready/wrong-source requests are rejected, chat remains usable and API/logs omit search text/tokens. Actual ncdc ADC/ADCS now issues `/search -hub SearchCoreSecret` and receives the existing unsupported response, then exits normally. This is a staging-boundary test, not evidence of successful end-to-end search/results.

Next increments can attach checked SCH parsing/serialization, immutable shared wire frames, transport/feature-scope policy, request admission and result validation/correlation. None is marked delivered here. See [TESTING.md](TESTING.md), [ROADMAP.md](ROADMAP.md) and [CHANGELOG.md](../../CHANGELOG.md).

```sh
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug -DDC24H_REQUIRE_INTEGRATION_TESTS=ON
cmake --build build-debug --parallel 4
ctest --test-dir build-debug -R 'search.core|adc.ncdc|adcs.ncdc' --output-on-failure -j 4
ctest --test-dir build-debug --output-on-failure -j 4
```
