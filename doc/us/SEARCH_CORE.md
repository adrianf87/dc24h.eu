# Search Core — alpha-0.5.1

**Follow-up — alpha-0.5.2:** [ADC SCH](ADC_SCH.md) now parses received requests and checks typed fields. The live response is now `125 Search routing not implemented`; routing and result exchange remain separate. The alpha-0.5.1 implementation and validation below describe their original delivery scope.

Based directly on main `c6b34ea849ef808a0bfee4a27e0c8ae38cff4a6b`, after the alpha-0.4.12 WebAdmin merge. Code, typed errors and test diagnostics use EN/US UTF-8; documentation is mirrored in US and PL.

## Delivered scope

`dc24h_search` is a separate C++20 library under `include/dc24h/search/` and `src/search/`, built by CMake with the existing Users and Text libraries. It provides a checked textual query value and a hub-side coordination engine: validate a ready source, select current ready session lifetimes, prepare an owning immutable plan, and attempt queue delivery through a trusted caller-supplied sink. It does not keep a hub file index or inspect local/shared files. Clients own their shared-file search implementation.

The [ADC specification](https://adc.sourceforge.io/ADC.html) distinguishes search requests (SCH), results (RES) and feature-based routing. This release supplies the underlying generic model, not an ADC codec or network search handler. `QueryData` is an internal C++ contract; field bounds and validation below are application policy, not a claim about all legal ADC searches. The enum values are not wire numeric values.

Production LoginHub does not call Search Core yet. Valid-state/source SCH/RES still gets `ISTA 125 Command\snot\simplemented FC<FOURCC>`; state and source validation retain precedence. This prevents accidental unvalidated forwarding while protocol work is staged. There is no live result exchange, TTH query, UDP response endpoint, search timeout/rate quota, token registry, history, database migration, API/search statistics or WebAdmin search control. Existing Chat/WebAdmin behavior is preserved; their runtime version metadata advances to alpha-0.5.1.

## Immutable criteria

`Query::create(const QueryData&)` returns `variant<Query, QueryError>`. Successful creation owns an unchanged copy; caller mutation/destruction cannot alter it. `Query::data()` exposes only a const reference. The model does no trimming, normalization repair, case folding, wildcard/regex execution, file matching or SQL. Included/excluded strings, extension order/duplicates and an optional opaque token remain literal criteria for a future adapter/client. Failure returns a typed enum without echoing input. Allocation failure propagates.

| Input | Application rule |
| --- | --- |
| `include` | At least one term |
| `include` + `exclude` | At most 32 terms combined; each nonempty, not ASCII-space-only and at most 256 UTF-8 bytes |
| Text terms | Already-NFC UTF-8; controls, format characters and line/paragraph separators rejected by existing Text validation; internal/edge spaces are retained |
| `extensions` | At most 16 nonempty values, each at most 32 bytes, with the same text validation and no slash, backslash or ASCII space |
| `min_bytes`, `max_bytes`, `exact_bytes` | Optional nonnegative signed 64-bit values; minimum must not exceed maximum, and exact must satisfy any supplied bounds; zero and INT64_MAX are valid |
| `type` | Checked `ItemType::any`, `file` or `directory` enum |
| `token` | Optional; if supplied, 1–128 visible ASCII bytes (33–126), preserved without interpretation |
| Total owned input text | At most 4096 bytes across terms, extensions and token |

Counts, per-field byte sizes, sizes/enums and total bytes are checked before UTF-8/NFC work or copying. Size addition is bounded by the prior field/count checks. The input already owns its storage; a future decoder must separately bound wire bytes/parameters before building it. Empty include is deliberately unsupported even when size/type filters exist; hash-only, match-all and richer query semantics need later model/codec work. `QueryError` distinguishes missing/too-many terms, invalid text/extension/size/type/token and aggregate size failure. No successful query object can be constructed from unchecked mutable data.

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
