# Search Tokens — alpha-0.5.5

**Current increment — alpha-0.6.3:** [ADC RCM](ADC_RCM.md) adds owning D/E reverse-request validation and live target/echo routing through Transfer Core, with per-family passive-source/active-target checks, stable lifetime/address admission and shared bounded queues. CTM/RCM share wire-field validation. Real ncdc covers RCM→CTM and peer handshakes in both roles; no pending-token registry or file-data handling is claimed.

**Increment delivered in alpha-0.6.2:** [ADC CTM](ADC_CTM.md) adds owning D/E CTM validation and live target/echo routing over Transfer Core, with checked source TCP activity, lifetime/endpoint rechecks, shared bounded queues and real ncdc client-handshake interoperability. Protocol/port/token/extensions retain their checked spelling and privacy. RCM and file-data handling remain later stages.

**Increment delivered in alpha-0.6.1:** [Transfer Core](TRANSFER_CORE.md) adds an independent C++20 connection-coordination engine: bounded owning intents, active/passive IPv4/IPv6 roles and immutable session-bound one-attempt plans with cancellation and dispatch rechecks. Shared Users address classification preserves Search behavior. Live CTM/RCM routing and file data transfer remain future work.

**Increment delivered in alpha-0.5.13:** [WebAdmin Search Monitoring](WEBADMIN_SEARCH.md) adds browser-local query/result/refusal/queue rates, a bounded traffic chart and accessible recent-interval table. First samples, gaps, detected resets and failures start fresh baselines; pause/manual refresh and local clear preserve cumulative counters. Existing API/settings and credential isolation remain.

**Increment delivered in alpha-0.5.12:** [Search Configuration and Statistics](SEARCH_CONFIGURATION.md) exposes 19 cumulative hub SCH/RES counters, a lazy stored-token-window gauge and all 16 effective startup search settings through the authenticated read-only API and bilingual WebAdmin Search page. Queue acceptance is distinct from delivery; direct client UDP is excluded. No payload/history or configuration write endpoint is added.

**Increment delivered in alpha-0.5.11:** [Search Security](SEARCH_SECURITY.md) adds connection-lifetime query/sent-result/received-result count and byte quotas, bounded denial notices, guest/global permission policy, trusted session grants and configurable active-search limits. Admission precedes routing/token publication. Optional strict result-token policy supplements existing owner/generation checks; direct client UDP remains outside hub enforcement.

**Increment delivered in alpha-0.5.10:** [Active/Passive](ACTIVE_PASSIVE.md) projects checked I4/I6/U4/U6 into public Users and adds allocation-free per-family TCP/UDP reply-path metadata to Search Core and immutable plans. Passive DRES remains hub-routed; active URES travels directly between clients. Dynamic INF changes, endpoint ownership and real ncdc UDP sending/receiving are tested.

**Increment delivered in alpha-0.5.9:** [Result Routing](RESULT_ROUTING.md) forwards checked DRES only to its ready target over ADC/ADCS. Plans bind session lifetimes and, when TO is present, the target owner’s active search generation. Tokenless results remain supported. Shared bounded queues preserve original fields without public fanout, sender echo or administration payloads.

**Follow-up — alpha-0.5.6:** [Search Routing](SEARCH_ROUTING.md) forwards checked SCH over real ADC/ADCS: B/F broadcasts and lifetime-bound D/E delivery share one immutable packet with independent bounded queues. Supplied TO is registered in Search Core; absent TO remains valid. Source/state/feature checks, one-attempt dispatch and token cleanup are enforced. RES/result routing remains a later increment.

Based directly on main `c9b020c0842550862a86988bdcf9b6cb97e6b6b9` after the alpha-0.5.4 merge. Earlier Search Core, SCH, filters and TTH are preserved. Code/diagnostics remain EN/US UTF-8, with mirrored US/PL documentation.

## Registry and identity

`search/tokens.hpp` and `src/search/tokens.cpp` provide the owner-thread `Tokens` registry. It borrows SessionManager, which must outlive it. A stored key is the exact **session lifetime ID + decoded TO**; it is not a SID, nickname, CID, fd or token alone. Two ready owners may use the same token independently. A duplicate active token for the same owner is refused and never refreshes its deadline.

TO retains the existing Query/SCH policy: nonempty already-NFC UTF-8, at most 128 bytes, with no controls/format/line separators. Spaces, Unicode, case and backslashes remain opaque and unchanged. The registry does not trim, normalize, fold case, interpret a token as a path/command or use it as a password. Empty, over-limit or invalid tokens are refused; lookup uses exact bytes without allocating or re-validating Unicode.

Each accepted registration gets a nonzero local 64-bit generation. IDs increase, are not consumed by allocation failures and never wrap/reuse after exhaustion. `TokenHandle` owns the token, generation/owner/deadline snapshot and an opaque registry-domain reference. Copies retain the same registration; moving transfers the domain. Handles remain safe to inspect after registry destruction, but an old/foreign handle cannot cancel a new registry even at the same address or with identical numeric IDs. Neither a token nor an ID is an authentication credential or globally unique identifier.

`Tokens::activate(owner, token, now)` returns `variant<TokenHandle, TokenError>`. It requires a current ready owner. `find(owner, token, now)` returns an optional scalar `ActiveSearch` snapshot (ID, owner, expiry), not query contents. `active(handle, now)` verifies domain, current owner, deadline and generation. `cancel(handle)` removes only that stored generation; repeated/stale/foreign cancellations return false. Cancellation is cleanup: it may remove an expired entry that has not yet been pruned. `forget(owner)` removes all stored entries for that lifetime.

Cancellation/expiry allows the same owner's wire token to be registered again with a fresh internal generation. An old handle cannot remove or dispatch the replacement. **A reused wire TO alone cannot distinguish a late response to the earlier search.** This stage accepts no RES results; a future result adapter must define its wire-token/generation policy and sender/recipient checks before claiming safe remote correlation. Internal generation checks do not solve that wire ambiguity by themselves.

## Bounds, time and cleanup

| Local library policy | Default | Accepted bounds |
| --- | --- | --- |
| Total stored searches | 4096 | 1–65535 |
| Stored searches per owner | 8 | 1–64, at most the total limit |
| Lifetime | 60 seconds | 1–3600 seconds |
| Token | Existing 128-byte cap | Exact already-NFC decoded TO |

These constructor limits are application policy, not new ADC fields, network rate limits, runtime INI settings or interoperability claims. `first_id` can seed a fresh registry's local ID stream; zero is invalid and UINT64_MAX is the final available generation.

Callers supply `users::TimePoint` from the steady clock; the registry reads no clock and starts no thread/timer. Observed time is clamped monotonically, so backwards samples cannot extend a deadline or resurrect expired entries. A record expires at `now >= expires`. Deadline addition saturates at the clock maximum without overflow; a new record with no positive lifetime remaining is refused. Duplicates, failed admission and result lookups do not extend the deadline.

An ordered owner/token index supports bounded per-owner counting and allocation-free lookup. A deadline index points to stable map keys, so expiration removes only due entries. Both indexes commit together; failed allocation removes any partial insertion and consumes no generation. A handle is fully allocated before publication and subsequent moves allocate nothing. Unicode-library/C allocator fault coverage is separate from the C++ allocation campaigns.

Activation and lookup remove due entries. Lookup of an unready/missing owner forgets that owner's entries. `prune(now)` also scans stored records to remove closed/missing owners; callers should run it during their maintenance cycle. Global-cap admission prunes disconnected owners before refusing capacity. No SessionManager mutation hook or automatic Network timer is installed in this increment. `size()` reports stored entries, including closed-owner records awaiting maintenance. `find`, `active`, `cancel`, `forget` and `prune` are allocation-free/noexcept. Expiry work is bounded by the total cap; owner counting is bounded by the per-owner cap; explicit disconnected-owner pruning scans at most the stored total.

## Search Core lifecycle

Existing `prepare()` / `dispatch(Plan, sink)` remain stateless coordination APIs and do not reserve TO. New `Core::start(sender, source, data, now, selection)` validates ready sender/source ownership first, requires an explicit token, prepares the checked query/recipient plan, then registers its token. It returns `variant<ActivePlan, Error, QueryError, TokenError>`. A malformed query, invalid selection, absent token, duplicate/cap refusal or allocation failure never publishes a partial active plan; existing live registrations and user models remain intact (due-entry cleanup may still occur).

`ActivePlan` owns the Plan and TokenHandle. `dispatch(activePlan, sink, now)` verifies the Core domain, sender and current registration, then uses existing recipient lifetime/feature checks. It rechecks token activity before each recipient. Completing/canceling the search inside a trusted sink cancels remaining recipients; it does not undo prior queue acceptance. Dispatch itself allocates nothing after Sink construction. A thrown sink still propagates after possible partial delivery, without automatic retry or completion.

`complete(activePlan)` cancels that generation. A successful dispatch leaves the search active for its response window; queue acceptance is not completion. `find_search(owner, token, now)` exposes the local active identity; `prune_searches(now)` runs cleanup. Caller-owned active plans/handles do not cancel on destruction, so explicit completion or bounded expiration is required. Token tracking does not add an automatic single-dispatch/replay guard; trusted adapters still own one-attempt scheduling, result validation and queue policy. Stateless Plan dispatch deliberately remains available and does not consult this registry.

## Actual ADC/ADCS boundary

The [ADC SCH specification](https://adc.sourceforge.io/ADC.html) defines optional TO. As of alpha-0.5.6, [Search Routing](SEARCH_ROUTING.md) calls Core::start for valid requests with TO and routes requests without TO statelessly without fabricating a token. Duplicate/active-cap refusals do not fan out, zero-queue/empty-selection dispatch completes the token, and disconnect forgets the owner. Valid routing emits no acknowledgment. RES remains unsupported; generations do not solve reused-wire-token late-result ambiguity.

No result routing, UDP response handling, database history, search API/UI control, new operator message or configuration is added. Registry keys/metadata remain transient; criteria live only in caller-owned Query/Plan objects. No token, search ID, query text, filename or TTH is exported to logs, chat counters, storage, API or WebAdmin. Existing `languages/us` and `languages/pl` behavior is retained. Server/WebAdmin release metadata advance together to alpha-0.5.5.

## Verification

Local verification for **alpha-0.5.5** on 2026-11-12: all **100/100 CTest suites** passed in Debug, Release and ASan/UBSan with required integration tools and default leak detection, including token lifecycle/fault tests, actual ADC/ADCS refusal/reuse/privacy checks, real ncdc, MariaDB and Apache HTTPS. Builds emitted no compiler warnings. WebAdmin production build/typecheck, 4 Node tests, 6 Chromium tests and real C++ API integration passed. Release metadata, UTF-8, documentation language parity/local links, modified Python syntax, shipped INI and whitespace checks passed. These recorded results are local; remote CI is tracked in GitHub Actions. The full supported OS/client matrix and production load remain unverified.

`search.tokens` checks opaque case/Unicode tokens and exact byte bounds, independent owners, state/source precedence, duplicate/no-refresh behavior, expiry edges/backwards/extreme time, global/per-owner caps, disconnect/SID reuse, copied/moved/stale/foreign/reused-address handles, ID exhaustion, no-allocation cleanup/dispatch, SCH/TTH projection, active Core completion/mid-sink cancellation and transactional allocation rollback in both registry indexes and Core start. Existing stateless Core/filter/TTH/parser tests remain enabled.

`search.tokens.process` uses actual ADC/ADCS for repeated same-token requests beyond the registry's default per-owner cap, a second owner, malformed duplicate TO, TTH, absent/token-only input, disconnect/reconnect reuse and unsupported RES. Ordered INF/chat fences prove no fanout; API/log checks prove token/query privacy and unchanged chat accounting for searches. The existing real ncdc name/filter and TTH tests remain enabled on both transports; optional TO omission remains valid. These transport checks prove the refused-search boundary, not successful active remote searches/results.

Reproduce with the normal Debug/Release/ASan builds and `ctest --test-dir build-debug -R 'search\.|adc.sch|ncdc' --output-on-failure -j 4`, then the complete suite. See [TESTING.md](TESTING.md), [Search Core](SEARCH_CORE.md), [ADC SCH](ADC_SCH.md), [ROADMAP.md](ROADMAP.md) and [CHANGELOG.md](../../CHANGELOG.md). The branch preserves history and is not merged into main automatically.
