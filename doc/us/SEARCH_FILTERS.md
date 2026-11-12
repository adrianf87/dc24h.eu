# Search Filters — alpha-0.5.3

**Follow-up — alpha-0.5.6:** [Search Routing](SEARCH_ROUTING.md) forwards checked SCH over real ADC/ADCS: B/F broadcasts and lifetime-bound D/E delivery share one immutable packet with independent bounded queues. Supplied TO is registered in Search Core; absent TO remains valid. Source/state/feature checks, one-attempt dispatch and token cleanup are enforced. RES/result routing remains a later increment.

**Follow-up — alpha-0.5.5:** [Search Tokens](SEARCH_TOKENS.md) adds bounded session-lifetime/TO registration, local search generations, deadlines, cancellation and explicit active-search APIs in Core. Stateless preparation stays available; refused live SCH requests do not reserve tokens. Result correlation/routing and search administration remain later increments.

**Follow-up — alpha-0.5.4:** [TTH Search](TTH_SEARCH.md) carries the full 192-bit file root from SCH into immutable Core queries/plans, descriptors and compiled filters. Hash-only and combined queries use exact equality and retain all additional criteria. Real ncdc ADC/ADCS TTH searches are tested; live request/result routing remains a later increment.

Based directly on main `dd2a968469c3db21ada4e469afc85b7a3415eeab` after the alpha-0.5.2 merge. The earlier Search Core and ADC SCH stages are preserved. Code and diagnostics use EN/US UTF-8; documentation is mirrored in US/PL.

## Scope and API

`dc24h_search` now includes `search/filters.hpp` and `src/search/filters.cpp`. It provides reusable predicates over caller-supplied checked descriptors. The hub does not own a file index, walk directories or inspect clients' shares. This module prepares name, extension, size and type semantics for future search/result adapters; it does not implement SCH fanout or RES handling.

- `Query::create(QueryData)` validates and owns criteria without rewriting them.
- `Filter::create(QueryData)` returns `variant<Filter, QueryError>` and compiles internal Unicode comparison strings.
- `Item::create(ItemData)` returns `variant<Item, ItemError>` and owns a checked descriptor plus its comparison string.
- `filter.matches(item)` returns bool without allocation, exceptions, I/O, logging or mutation. Neither argument retains the other.
- `SchRequest::to_filter()` returns `variant<Filter, QueryError, SchError>`; `to_core_query()` supplies the same accepted textual/filter criteria to Core.

Successful values have private constructors. Filter copies share immutable compiled storage; Items own their strings and metadata. Input mutation/destruction cannot change either. Moving a Filter transfers its compiled storage; a moved-from Filter returns false. Item creation and filter compilation can propagate allocation failure without publishing a partial value or mutating input. Existing Core sender/lifetime/domain/feature checks continue to govern coordination; a matching descriptor grants no permission or proof of remote content.

## Matching contract

The [ADC specification, SCH and file naming](https://adc.sourceforge.io/ADC.html) defines case-insensitive matching of full names, all AN conditions, exclusion by any NO, extension alternatives, byte operators and file/directory types. This implementation chooses the following explicit local comparison and descriptor policy:

| Criterion | Behavior |
| --- | --- |
| AN (`include`) | Every term must occur as a literal substring of the name including its virtual share path |
| NO (`exclude`) | A match of any term anywhere in that name excludes the item |
| EX (`extensions`) | A file must end with at least one dot-prefixed extension on its final slash-delimited component; directory names and parent components cannot satisfy EX |
| GE / LE | Inclusive lower/upper size limits in bytes |
| EQ | Exact byte count, intersected with GE/LE |
| TY | File, directory or either when absent; concrete descriptors cannot have type `any` |
| TO | Opaque correlation metadata; never a matching condition |

Name/extension comparison uses the existing utf8proc library with stable Unicode full case folding and NFC composition on internal copies. For example, `ŻÓŁW` matches `żółw`, `STRASSE` matches `Straße`, and Greek sigma forms compare equally. Accents are retained: `zolw` does not match `żółw`. Original criteria, token, descriptor spelling and case remain unchanged. No locale-dependent conversion, trimming, token splitting, wildcard or regex execution occurs. Spaces, punctuation, `*` and `?` in criteria are literal.

EX values omit the leading period. Matching requires a nonempty stem, a dot boundary and an entire suffix: `iso` matches `disk.ISO`, but not `notiso`, `disk.iso.bak`, `/dir.iso/file` or the bare hidden name `.iso`. Compound literal suffixes such as `tar.gz` are supported; they cannot span `/`. Multiple EX values are OR alternatives and combine with the other fields using AND. EX with TY directory is a valid predicate that matches no descriptors. These boundary/compound/hidden-name choices are local policy, not additional ADC wire fields or extension-group inference.

`ItemData` carries a decoded virtual `name` including its share path, concrete ItemType and optional nonnegative signed 64-bit `bytes`. Name length is 1–4096 UTF-8 bytes, not ASCII-space-only, with the existing already-NFC Text policy (no controls, formatting characters or line/paragraph separators). A file name cannot end in `/`. A directory name may carry its trailing `/`. This is descriptor validation, not a full ADC file-list/RES parser or OS path validator. The module never opens the name or resolves traversal/symlinks.

Size zero is a real value; unknown size is `nullopt`. Any size predicate refuses an unknown size, including for directories. A supplied directory size is evaluated like a file size; no recursive size is computed or guessed. Without size criteria, unknown size is acceptable. Zero and INT64_MAX are handled through comparisons without subtraction or overflow. Invalid/negative sizes and forged types are refused before text work.

## Query and SCH integration

At least one recognized criterion is required: an AN, NO, EX, GE/LE/EQ, concrete TY or (as of alpha-0.5.4) TTH root. Positive AN terms are now optional, so exclusion-only, extension-only, size-only and type-only requests project into Core without fabricated text. Empty and token-only QueryData returns `QueryError::no_criteria` (replacing the old `missing_term` case). This permits broad explicit predicates such as GE0/TY1; request quotas and routing admission remain future work, not a property of matching.

Existing bounds remain: 32 combined AN/NO terms, 256 bytes per term, 16 extensions of at most 32 bytes, 128 token bytes and 4096 aggregate text bytes. Extension validation now consistently rejects a leading period in both SCH and Query. Tokens accept opaque already-NFC text including spaces/Unicode, matching the SCH profile; invalid text is refused and TO alone still is not a criterion. Input is bounded before text normalization/copying. Internal case-folded strings may expand but derive only from these bounded inputs. Matching work is bounded by the candidate name, term and extension limits; compilation/descriptor preparation allocates, repeated matching does not.

As of alpha-0.5.4, TR-bearing requests project their full root into both Core and Filter, including combined TR/text/filter requests. Hash-only queries require exact file-root equality; extra filters remain AND constraints. See [TTH Search](TTH_SEARCH.md). A Tiger root cannot be silently lost or treated as a text-only filter. Original checked SCH fields/order/unknown parameters/numeric spellings remain in the owning Message. Unknown fields keep the existing ADC ignore-and-preserve policy; ASCH/SEGA groups are not interpreted or advertised by this module.

As of alpha-0.5.6, [Search Routing](SEARCH_ROUTING.md) forwards valid B/F/D/E requests through shared bounded queues after source/state/readiness and semantic validation. Present TO is registered, absent TO is preserved, and result handling remains unsupported. Filter predicates are reusable library logic, not a hub file index; clients perform their own searches. No query/token/name data is exported to logs, storage, API or WebAdmin.

## Verification

Local verification for **alpha-0.5.3** on 2026-11-11: all **95/95 CTest suites** passed in Debug, Release and ASan/UBSan with required integration tools and default leak detection, including real ncdc ADC/ADCS, MariaDB and Apache HTTPS. Builds emitted no compiler warnings. WebAdmin production build/typecheck, 4 Node tests, 6 Chromium tests and real C++ API integration passed. Release metadata, UTF-8, documentation language parity/local links, modified Python syntax, the shipped INI and whitespace checks passed. Results are local; remote CI, the full supported OS/client matrix and production load are not claimed.

`search.filters` checks full-path AN/NO, Unicode expansion/sigma/accents, literal spaces/punctuation, extension OR/dot/component/compound boundaries, directory isolation, inclusive/exact/zero/INT64_MAX sizes, missing sizes, concrete types, invalid text/types/sizes, exact bounds, immutable copy/move ownership, SCH projection, Core filter-only preparation, C++ allocation-failure rollback and allocation-free matching. It does not simulate arbitrary C allocator failure inside utf8proc.

`adc.sch.process` extends real fragmented B/F/D/E SCH over ADC/ADCS with each standalone filter family and Unicode extensions/tokens, retaining recovery, no fanout, source/state precedence and API/log privacy checks. Actual ncdc ADC/ADCS uses `/search -hub -ge 1K -t archive SearchCoreSecret`; plaintext capture verifies GE1024 and extension fields, while TLS remains encrypted. Both prove parsed reception and the explicit routing boundary, not successful result exchange. ncdc uses one size condition per search; the combined GE/LE/EQ semantics are tested directly and through raw ADC/ADCS.

```sh
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug -DDC24H_REQUIRE_INTEGRATION_TESTS=ON
cmake --build build-debug --parallel 4
ctest --test-dir build-debug --output-on-failure -j 4
```

Repeat in Release and ASan/UBSan as described in [TESTING.md](TESTING.md). See [Search Core](SEARCH_CORE.md), [ADC SCH](ADC_SCH.md), [ROADMAP.md](ROADMAP.md) and [CHANGELOG.md](../../CHANGELOG.md). Request/result routing, a file index, search-specific quotas/history, remote CI, the full supported OS matrix and production load need separate work/evidence.
