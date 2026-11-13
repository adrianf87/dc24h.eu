# Result Routing — alpha-0.5.9

**Current increment — alpha-0.5.10:** [Active/Passive](ACTIVE_PASSIVE.md) projects checked I4/I6/U4/U6 into public Users and adds allocation-free per-family TCP/UDP reply-path metadata to Search Core and immutable plans. Passive DRES remains hub-routed; active URES travels directly between clients. Dynamic INF changes, endpoint ownership and real ncdc UDP sending/receiving are tested.

Local verification on 2026-11-13: **110/110 CTest suites passed in each of Debug, Release and ASan/UBSan**, with required integration tools and default leak detection. This includes mixed ADC/ADCS result delivery, allocation faults, actual ncdc receiving/producing results, MariaDB and Apache HTTPS. No compiler warnings. WebAdmin production build/typecheck, 4 Node tests, 6 Chromium tests and real C++ API integration passed. Release metadata, UTF-8, language file parity/local documentation links, changed Python syntax, shipped INI and whitespace checks passed. These are local results; remote CI, the full supported OS/client matrix and production load remain unverified.

Based directly on main `c837ecaec3d803953c083e23d78dd621c346c19f`, after the alpha-0.5.8 merge. Existing code, branches and history remain preserved. This branch adds actual result delivery through the hub and is published separately from main.

## Routing contract

`adc/result_routing.hpp` defines ResultRouter, RoutedResult, an immutable ResultFrame and typed ResultRoutingError. The adapter borrows Search Core and uses its existing ready-session and active-token lookups; it does not create a new search/result registry. Core and SessionManager must outlive the adapter and its dispatch calls. All calls occur on the owning reactor thread.

Preparation validates responder readiness and source SID ownership before invoking the [ADC RES parser](ADC_RES.md). Only DRES is supported. It resolves the target SID to a ready session lifetime; an absent, identified-only or closing recipient fails without fallback. A self-directed result is queued once. There is no broadcast, feature fanout, sender echo or automatic UDP forwarding.

The checked original Message and wire data are owned by one immutable shared frame. Field order, unknown fields/repeats, numeric spellings, decoded opaque token and canonical root survive normal ADC escaping/serialization. FN/SI/SL/TO/TR semantics and byte/field limits remain those of alpha-0.5.8. Parsed file names remain virtual metadata; no file is opened, hashed or indexed.

### Tokens and search windows

When TO is supplied, it must resolve exactly to an active search owned by the target's session lifetime. The same decoded token value can be used independently by different owners; no case folding or Unicode rewriting occurs. Unknown, expired, canceled and other-owner tokens fail with a recoverable fixed diagnostic. The plan records the local ActiveSearch generation and expiry; dispatch rejects a canceled/replaced generation even if its owner and token have been reused. Result arrival does not renew the deadline or complete the search, so multiple responses remain possible until expiry/cancellation/disconnect.

TO remains optional for compatibility with existing tokenless SCH. Such results use checked DRES target/source routing without search correlation. Removing TO cannot be distinguished from a legitimate tokenless response; this is an explicit compatibility policy, not an unsolicited-result defense. Presence of an active TO proves only a correlation window, not that the responder received the query, matched its filters or possesses the file. The token is not a credential. No responder allowlist, file/filter authentication, per-search count policy or deduplication is added. An incoming delayed wire result with a reused identical TO cannot be distinguished from a new-generation response; the local generation protects already prepared plans only.

These choices build on [ADC message routing and SCH/RES](https://adc.sourceforge.io/ADC.html), with file roots from [TIGR](https://adc.sourceforge.io/ADC-EXT.html). Active-token checking, the DRES-only profile and application limits are local server policy.

## Dispatch, queues and failures

A plan belongs to its originating ResultRouter domain, including across object-address reuse. Copies share one dispatch attempt. Foreign-router and missing-sink checks do not consume the attempt; otherwise an attempt is consumed before revalidation/callback. Retrying after a refused, stale or throwing sink is not supported. Sinks must not reenter dispatch or throw; exceptions propagate after the attempt has been consumed.

Dispatch rechecks source ownership/readiness, target lifetime/readiness and optional active-search generation. It allocates nothing after sink construction. A DispatchReport distinguishes one queued result from a refused enqueue; success means accepted by the queue, never delivered or read by the client. Checks apply at queue admission; already queued FIFO results are not retracted when a search expires later.

LoginHub's existing bounded queues now hold SharedResultFrame alongside other shared packets. Partial TCP/TLS sends retain the same pinned front and independent offset. Full logical frame charges remain until consumption/cleanup, including under TLS retries. Per-client and aggregate output caps apply. Queue overflow or queue-allocation failure closes only the affected recipient; the responder and unrelated peers remain usable. These failures produce no new response allocation on the responder. Preparation allocation failure closes only the source. A self-directed result naturally shares source/recipient failure ownership.

| Condition | Wire behavior |
| --- | --- |
| Valid DRES with eligible target and optional active TO | Queue original checked frame only to target; no success acknowledgment or source echo. |
| Invalid semantic result/unsupported B/F/E result profile | `ISTA 100 Invalid\ssearch\sresult FC<FOURCC>` |
| Missing/unready target | `ISTA 140 Search\sresult\srecipient\sunavailable FCDRES` |
| Supplied TO is not active for target | `ISTA 100 Search\sresult\stoken\sis\snot\sactive FCDRES` |
| Wrong state/source | Existing fatal 244/240 precedence, including malformed payloads. |
| Syntax/UTF-8/escape failure | Existing transport/parser policy; no reflected result data. |

No diagnostic reflects names, roots, tokens or target keys. Search traffic remains excluded from chat counters. Query/result payloads are not stored in SQL, logs, the private status API or WebAdmin. General frame/rate caps remain; there is no new result-specific flood limiter, runtime setting, operator catalog entry, migration, endpoint or frontend search control. Runtime languages remain `languages/us` and `languages/pl`; documentation remains mirrored in `doc/us` and `doc/pl`.

## Verification and future work

`search.results` covers exact target/self routing, original fields and ownership, active/other-owner/missing/case-sensitive tokens, cancellation/reuse/expiry, sender/target closure and SID reuse, router-domain and shared-attempt restrictions, refused/throwing sinks, allocation-free dispatch and preparation/source/recipient allocation failures. LoginHub checks cover pinned fronts, partial-send accounting, queue saturation, failure isolation and cleanup.

`search.results.process` sends fragmented frames over mixed ADC/ADCS connections with a bystander. It verifies independent owner namespaces, no echoes/fanout/duplicates, tokenless/file/directory compatibility, recoverable errors, disconnect/reconnect cleanup, source/state priority and API/log/chat privacy. Existing `adc.res.process` now verifies actual delivery for valid results while retaining malformed-result checks.

Real `adc.feature.ncdc` / `adcs.feature.ncdc` verify both directions: ncdc's active search receives a result visible in its search UI (its current wire query omits TO, exercising tokenless compatibility), and an indexed ncdc share responds to FSCH with a DRES delivered to the issuing raw client. TLS proxy captures remain encrypted. Existing name/filter/TTH, login/chat/reconnect and search regression tests remain enabled.

Future stages include responder eligibility, stricter tokenless/unsolicited-result policy, result count/rate limits, deduplication/history, UDP/SUDP, transfer negotiation and search administration. This release does not claim those capabilities or production-load validation.
