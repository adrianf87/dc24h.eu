# ADC SCH — alpha-0.5.2

**Current increment — alpha-0.6.1:** [Transfer Core](TRANSFER_CORE.md) adds an independent C++20 connection-coordination engine: bounded owning intents, active/passive IPv4/IPv6 roles and immutable session-bound one-attempt plans with cancellation and dispatch rechecks. Shared Users address classification preserves Search behavior. Live CTM/RCM routing and file data transfer remain future work.

**Increment delivered in alpha-0.5.13:** [WebAdmin Search Monitoring](WEBADMIN_SEARCH.md) adds browser-local query/result/refusal/queue rates, a bounded traffic chart and accessible recent-interval table. First samples, gaps, detected resets and failures start fresh baselines; pause/manual refresh and local clear preserve cumulative counters. Existing API/settings and credential isolation remain.

**Increment delivered in alpha-0.5.12:** [Search Configuration and Statistics](SEARCH_CONFIGURATION.md) exposes 19 cumulative hub SCH/RES counters, a lazy stored-token-window gauge and all 16 effective startup search settings through the authenticated read-only API and bilingual WebAdmin Search page. Queue acceptance is distinct from delivery; direct client UDP is excluded. No payload/history or configuration write endpoint is added.

**Increment delivered in alpha-0.5.11:** [Search Security](SEARCH_SECURITY.md) adds connection-lifetime query/sent-result/received-result count and byte quotas, bounded denial notices, guest/global permission policy, trusted session grants and configurable active-search limits. Admission precedes routing/token publication. Optional strict result-token policy supplements existing owner/generation checks; direct client UDP remains outside hub enforcement.

**Increment delivered in alpha-0.5.10:** [Active/Passive](ACTIVE_PASSIVE.md) projects checked I4/I6/U4/U6 into public Users and adds allocation-free per-family TCP/UDP reply-path metadata to Search Core and immutable plans. Passive DRES remains hub-routed; active URES travels directly between clients. Dynamic INF changes, endpoint ownership and real ncdc UDP sending/receiving are tested.

**Increment delivered in alpha-0.5.9:** [Result Routing](RESULT_ROUTING.md) forwards checked DRES only to its ready target over ADC/ADCS. Plans bind session lifetimes and, when TO is present, the target owner’s active search generation. Tokenless results remain supported. Shared bounded queues preserve original fields without public fanout, sender echo or administration payloads.

**Increment delivered in alpha-0.5.8:** [ADC RES](ADC_RES.md) adds owning typed DRES parsing and live ADC/ADCS validation of names, sizes, slots, opaque tokens and canonical TTH roots after session/source checks. Semantic errors are recoverable and private; valid results explicitly report pending result routing. Token correlation, result delivery and UDP remain future increments.

**Increment delivered in alpha-0.5.7:** [Feature Routing](FEATURE_ROUTING.md) compiles checked FSCH selectors once and uses current public INF SU for all-required/no-excluded matching. SUP negotiation is independent. Live capability changes, issuer membership, unknown revisions and limits are covered over ADC/ADCS and real ncdc. RES/result routing remains a later increment.

**Follow-up — alpha-0.5.6:** [Search Routing](SEARCH_ROUTING.md) forwards checked SCH over real ADC/ADCS: B/F broadcasts and lifetime-bound D/E delivery share one immutable packet with independent bounded queues. Supplied TO is registered in Search Core; absent TO remains valid. Source/state/feature checks, one-attempt dispatch and token cleanup are enforced. RES/result routing remains a later increment.

**Follow-up — alpha-0.5.5:** [Search Tokens](SEARCH_TOKENS.md) adds bounded session-lifetime/TO registration, local search generations, deadlines, cancellation and explicit active-search APIs in Core. Stateless preparation stays available; refused live SCH requests do not reserve tokens. Result correlation/routing and search administration remain later increments.

**Follow-up — alpha-0.5.4:** [TTH Search](TTH_SEARCH.md) carries the full 192-bit file root from SCH into immutable Core queries/plans, descriptors and compiled filters. Hash-only and combined queries use exact equality and retain all additional criteria. Real ncdc ADC/ADCS TTH searches are tested; live request/result routing remains a later increment.

**Follow-up — alpha-0.5.3:** [Search Filters](SEARCH_FILTERS.md) adds compiled Unicode case-insensitive name/path predicates, extension alternatives, inclusive/exact byte limits and file/directory types over checked descriptors. Filter-only queries and opaque NFC tokens now project to Search Core. Matching is allocation-free; routing/results, indexes and search administration remain later increments.

Based directly on main `0fd969bebfe399388f17fc14632cc72cb3f6ee3d`, after the Search Core merge. Code and wire diagnostics remain EN/US UTF-8; documentation is mirrored in US/PL. This release adds typed parsing and actual reception, while request fanout, results, quotas/history and search administration remain separate increments.

## Parser and owning request

`adc/sch.hpp` and `src/adc/sch.cpp` add `parse_sch(body)`, `validate_sch(message)` and immutable `SchRequest`. The first accepts one body without LF and returns `variant<SchRequest, ParseError, SchError>`; framing, escaping, UTF-8/NFC, header and generic parameter failures retain ParseError. Validation of a hand-built Message returns `variant<SchRequest, SchError>` and uses bounded shared serialization/reparsing to check consistency, including unknown fields. Neither operation authorizes a sender or routes a packet.

SchRequest owns the original checked Message plus typed criteria, a Search Core Selection and optional 24-byte Tiger root. Raw parameter order, duplicates of repeatable/unknown fields, numeric spellings and selectors are retained in `message()`; generic serialization can reproduce them with standard ADC escaping. No borrowed views survive copy/move or caller mutation. There is no dedicated SCH builder/serializer API in this increment.

The implemented hub-side scopes are B/F/D/E, with a valid source SID, a target exactly for D/E, and nonempty selectors exactly for F. H/I/C/U are outside this profile; UDP request reception is not added. Generic parser support for other scopes is unchanged. Message kind/command/positional count, source/target/header layout and bounded parameters are checked even for local objects.

## Typed fields and application policy

Field meanings follow [ADC SCH](https://adc.sourceforge.io/ADC.html): repeated AN/NO terms and EX extensions; inclusive GE/LE and exact EQ sizes; TY 1 for file or 2 for directory; optional opaque TO. TIGR's [TR field](https://adc.sourceforge.io/ADC-EXT.html) carries the tree root. Unknown fields remain opaque in the original Message and do not become Core criteria. No matching, wildcard execution, local file access, SQL or hash calculation is performed.

| Field | Checked representation / local policy |
| --- | --- |
| AN / NO | Owning decoded UTF-8/NFC strings, each 1–256 bytes; not ASCII-space-only; up to 32 combined terms |
| EX | Owning decoded text, each 1–32 bytes; up to 16; no leading period, slash, backslash or ASCII space |
| GE / LE / EQ | Optional nonnegative signed 64-bit integers; no overflow, fractional/plus spelling or inconsistent bounds; zero and INT64_MAX allowed |
| TY | Optional decimal value 1 or 2; absent means `ItemType::any`; valid integer aliases such as `01` are interpreted numerically but raw spelling is retained |
| TO | Optional nonempty decoded NFC text, 1–128 UTF-8 bytes; spaces and Unicode are retained, controls/format/line separators rejected |
| TR | Optional canonical unpadded uppercase Base32 Tiger root: 39 symbols and zero unused bits; includes hash-only requests |
| F selectors | Up to 32 total, first three uppercase ASCII letters and last uppercase letter/digit; duplicate or conflicting names refused |
| Payload | Up to 4096 decoded bytes across all complete parameter strings, including two-character names, unknown fields and repeats |

GE, LE, EQ, TY, TO and TR are singular: a repeated known scalar is rejected even when values match. GE must not exceed LE and EQ must fit any supplied bounds. Known text uses the existing stricter Text policy: controls, formatting characters and line/paragraph separators are rejected; no trimming, case conversion or normalization repair occurs. These are application bounds, not claims that every legal ADC peer uses the same limits. The generic 16384-byte encoded-body and 128-parameter limits remain in force; selector header bytes are separate from the decoded parameter payload budget.

Criteria may be filter-only, exclusion-only or hash-only. Empty, unknown-only and token-only requests return `SchError::no_criteria`; the live receiver ignores them without a response. This is an explicit bounded profile that avoids treating absent criteria as an unrestricted search. Empty known values, malformed numbers/hashes, duplicate scalars and size conflicts receive typed semantic failures. Unknown fields are syntax checked and counted against budgets, retained unchanged and never exposed in logs/API/UI. Their semantics are not implemented here.

## Bridge to Search Core

`criteria()` and `selection()` expose const typed values. `to_core_query()` returns `variant<search::Query, search::QueryError, SchError>` and applies the checked Search Core profile. As of alpha-0.5.3, filter-only requests and opaque NFC tokens with spaces/Unicode are accepted without inventing or rewriting fields. `to_filter()` compiles the same typed criteria into [Search Filters](SEARCH_FILTERS.md). As of alpha-0.5.4, TR is retained in `criteria().tth`, `root()` reads that same storage, and both projections support exact [TTH Search](TTH_SEARCH.md), including hash-only requests. The legacy `unsupported_hash_query` enumerator is retained for source compatibility but is no longer emitted. Raw values are not rewritten to force acceptance. The method is a checked projection, not a delivery plan or permission grant.

The ADC library now links to dc24h_search without introducing a reverse dependency. As of alpha-0.5.6, the receiver uses the checked SCH request to prepare/dispatch Search Core fanout through [Search Routing](SEARCH_ROUTING.md); original fields are preserved and copied plans share one attempt. Future integration must account for projection limits and preserve applicable extension semantics.

## Actual ADC/ADCS reception

LoginHub handles SCH after ClientSession state/source validation and final-welcome readiness. Closing peers are still ignored. A pre-ready request gets the existing fatal state error; a forged SID gets the existing fatal source error even if its fields are malformed. Protocol H/I/C/U scope rules remain governed by the existing state layer.

| Received input | Behavior |
| --- | --- |
| Valid in-profile SCH | Routed to eligible B/F/D/E recipients; no success acknowledgment |
| Semantic SCH failure | Recoverable `ISTA 100 Invalid\ssearch\squery FC<FOURCC>` |
| Empty/unknown-only/token-only | No reply and no forwarding |
| Raw generic parser failure | Existing bounded invalid-frame handling before the SCH receiver |
| Invalid source/state | Existing fatal `240` / `244` behavior before field work |

Descriptions and FOURCC are fixed; no query, token, root, filename or arbitrary field is echoed. Refusals preserve readiness and ordered queues; later valid queries/chat can proceed. As of alpha-0.5.6, valid requests reach only their eligible B/F/D/E recipients; rejected requests never fan out. Duplicate active TO and active-cap refusals return fixed 100 diagnostics; an unavailable directed target returns 140. See [Search Routing](SEARCH_ROUTING.md). RES keeps the prior unsupported behavior. SCH does not increment chat counters, add API fields or expose search text to WebAdmin. Search data is transient; no query/history database is added.

Parser/status allocation failure is caught at the receiving peer and forces only that peer into immediate internal-error close. Other sessions and queues remain available. The existing global frame/security/output budgets still apply. This stage does not add a search-specific rate limiter, success acknowledgment, timer, request registry or result correlation. Operator language does not translate wire descriptions.

## Verification and reproduction

Local verification for **alpha-0.5.2** on 2026-11-11: all **94/94 CTest suites** passed in Debug, Release and ASan/UBSan with required integration tools and default leak detection, including typed SCH tests, actual fragmented ADC/ADCS reception, ncdc searches, MariaDB and Apache HTTPS. Final builds emitted no compiler warnings. WebAdmin production build/typecheck, 4 Node tests, 6 Chromium tests and real C++ API integration passed. Metadata, UTF-8, local links, shipped INI and whitespace checks passed. These are local results; remote CI, the full supported OS/client matrix and production load remain unverified.

`adc.sch` checks typed BASE fields, Unicode/spaces/escapes, integer aliases and limits, canonical TR, opaque unknown fields, copy/move ownership, Core projection refusals, repeated scalar policy, exact decoded/parameter/term/token/selector budgets, raw versus semantic error types, hostile local objects and allocation-failure rollback/peer isolation. The C++ allocation campaign does not claim arbitrary C allocator or Unicode-library fault coverage.

`adc.sch.process` uses actual ADC/ADCS clients, fragmented writes and ordered INF fences to verify every field family, malformed semantics, silent criterion-free input, raw syntax recovery, no fanout, source/state precedence and API/log/token privacy. `search.core.process` is updated for the new parsed-request response. Actual ncdc ADC/ADCS `/search` now confirms `Search routing not implemented`, which proves successful live SCH parsing; it does not prove successful result exchange.

```sh
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug -DDC24H_REQUIRE_INTEGRATION_TESTS=ON
cmake --build build-debug --parallel 4
ctest --test-dir build-debug -R 'adc.sch|search.core|adc.ncdc|adcs.ncdc' --output-on-failure -j 4
ctest --test-dir build-debug --output-on-failure -j 4
```

See [Search Core](SEARCH_CORE.md), [TESTING.md](TESTING.md) and [CHANGELOG.md](../../CHANGELOG.md). Full routed searches/results, remote CI, the full OS/client matrix and production load require separate work/evidence.
