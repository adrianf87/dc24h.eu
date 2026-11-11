# TTH Search — alpha-0.5.4

Based directly on main `edb702ada4fea04d7854a18370a052fc96035ea7` after the alpha-0.5.3 merge. Earlier Search Core, SCH and filters are preserved. Code and diagnostics remain EN/US UTF-8, with mirrored US/PL documentation.

## Delivered behavior

`search::TthRoot` in `search/tth.hpp` is a fixed 24-byte public file-root value without a dependency on ADC, sockets or the crypto provider. `QueryData::tth` and `ItemData::tth` are optional roots. Every 192-bit value is representable; absence is distinct from an all-zero value. A root is caller-supplied metadata, not proof that a file exists or that the caller has its contents.

`Query::create()` accepts a TTH as the only criterion, with no fabricated positive name term. Existing text, extension, size, type and token checks still run for combined requests. The root payload is a fixed 24 bytes; the existing 4096-byte aggregate text budget, field counts and name bounds are unchanged. A root alone does not permit malformed or over-limit additional fields.

`Filter::create()` owns the root in immutable compiled state. `matches(item)` requires exact equality of all 192 bits and a file descriptor with a known root. A missing or different root fails; directories cannot satisfy a TTH request. `Item::create()` refuses a supplied file root on a directory with `ItemError::invalid_hash`, before text processing. Files may omit a root for ordinary name/filter matching.

Hash-only matching is independent of a valid file descriptor's name, extension and known/unknown size. Additional AN/NO/EX/GE/LE/EQ/TY constraints remain AND conditions: an equal root never bypasses them. A request combining TR and TY directory is accepted as an unsatisfiable predicate, consistently with the earlier explicit EX/TY directory policy. It is not silently changed to a file-only request. TO remains opaque correlation metadata and does not affect matching.

Matching performs no allocation, I/O, normalization or hashing, and is noexcept. Creation retains the earlier bounded Unicode work and allocation-failure contract. Query, Item, Filter and Core plans own/copy their roots; changing caller storage cannot alter a prepared query, descriptor or dispatch. Core still checks the ready sender/source, session lifetimes, routing domain and feature selection. TTH is not a new identity/authentication mechanism.

## ADC representation and projection

The official [TIGR extension](https://adc.sourceforge.io/ADC-EXT.html) defines TR as the Base32-encoded Tiger tree root. This stage uses the existing checked ADC Tiger Base32 codec: exactly 39 uppercase unpadded characters from A–Z/2–7 and zero unused tail bits. Lowercase, padding, wrong lengths/characters, `TTH/` prefixes and magnet/URN strings are not silently repaired. This is the existing canonical application wire profile; no second codec, dependency or hashing provider is introduced.

`parse_sch()` / `validate_sch()` place the decoded TR directly in `criteria().tth`. `SchRequest::root()` reads that same storage, eliminating independent copies of the typed root. Both `to_core_query()` and `to_filter()` now carry hash-only and combined requests without losing TR. The legacy `SchError::unsupported_hash_query` enumerator remains for source compatibility but is no longer emitted. The projection return types are unchanged.

The original Message retains raw checked fields, order, numeric spellings and unknown parameters for existing serialization. Duplicate TR is rejected even when identical. All previous parameter/frame/decoded-payload, Unicode, selector, scalar and state/source rules remain in force. Wire parsing remains in ADC; Search receives only a binary root and stays protocol independent.

| Input | Result |
| --- | --- |
| `BSCH AAAB TR<canonical-root> TOtoken` | Owning hash-only request; Core and Filter preserve the root/token |
| Equal file root plus satisfied extra fields | Predicate matches |
| Missing/different file root or a directory | Predicate does not match |
| Equal root with an unsatisfied name/extension/size/type filter | Predicate does not match |
| Malformed or duplicate TR | Existing typed `invalid_hash` / `duplicate_field` semantic failure |

## Live transport boundary

Real ADC/ADCS reception continues to validate state/source/readiness before parsing SCH. A valid TTH request still receives recoverable `125 Search routing not implemented`; malformed semantic input receives `100 Invalid search query`. This increment implements TTH criteria, projection and matching over supplied descriptors. It does not forward SCH, accept/correlate RES, send UDP responses or publish search results. Raw TCP/TLS tests and real ncdc prove parsed TTH reception and this explicit boundary, not completed remote result exchange.

No hub file index, share traversal, Tiger tree computation, file-content verification, request registry, search-specific quota/history or new configuration/API/UI is added. The existing `adc::tiger192` hashes a byte sequence for identity-related code; it must not be substituted for a whole file's Merkle-tree root. Unit tests distinguish the empty-file TTH (Tiger of the leaf prefix byte 0x00) from Tiger of an empty byte sequence; a general TTH generator is not claimed.

Roots, search terms, opaque tokens and names remain transient and are not echoed into diagnostics, logs, chat statistics, database, API or WebAdmin. No SQL migration or operator-language entry is needed; existing `languages/us` and `languages/pl` behavior is retained. Runtime and WebAdmin release metadata advance together to alpha-0.5.4.

## Verification

Local verification for **alpha-0.5.4** on 2026-11-11: all **98/98 CTest suites** passed in Debug, Release and ASan/UBSan with required integration tools and default leak detection, including hash-only ncdc ADC/ADCS, prior filter/chat/login regressions, MariaDB and Apache HTTPS. Builds emitted no compiler warnings. WebAdmin production build/typecheck, 4 Node tests, 6 Chromium tests and real C++ API integration passed. Release metadata, UTF-8, documentation language parity/local links, modified Python syntax, shipped INI and whitespace checks passed. These are local results; remote CI, the full supported OS/client matrix and production load remain unverified.

`search.tth` covers the empty-file vector, every one-bit difference across all 192 bits, zero/absent/all-ones distinctions, renamed files and unknown sizes, directories, combined criteria, exact text budgets, immutable/copy/move ownership, B/F/D/E SCH round trips, canonical-input rejection, duplicate roots, source-bound Core plans/dispatch, C++ allocation-failure rollback and allocation-free matching/dispatch. Existing `search.filters`, `search.core`, `adc.sch` and identity-codec tests remain enabled. Fault injection does not claim arbitrary C allocator/provider failure coverage.

`adc.sch.process` includes canonical hash-only and combined queries, lowercase/padded/duplicate/noncanonical roots and privacy/recovery/no-fanout fences on actual ADC/ADCS transports. New `adc.tth.ncdc` and `adcs.tth.ncdc` use the existing full login/chat/reconnect fixture with `/search -hub -tth LWPNACQDBZRYXW3VHJVCJ64QBZNGHOHHHZWCLNQ`. Plaintext capture checks a TR-bearing SCH without invented AN/EX and the fixed response; TLS keeps the root encrypted. Existing ncdc name/extension/size searches remain separate tests.

```sh
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug -DDC24H_REQUIRE_INTEGRATION_TESTS=ON
cmake --build build-debug --parallel 4
ctest --test-dir build-debug -R 'search\.|adc.sch|tth.ncdc' --output-on-failure -j 4
ctest --test-dir build-debug --output-on-failure -j 4
```

Repeat Release and ASan/UBSan using [TESTING.md](TESTING.md). See [Search Core](SEARCH_CORE.md), [ADC SCH](ADC_SCH.md), [Search Filters](SEARCH_FILTERS.md), [ROADMAP.md](ROADMAP.md) and [CHANGELOG.md](../../CHANGELOG.md). The numbered branch preserves history and is not merged into main automatically. Live routing/results and production-load/full-platform validation require subsequent work.
