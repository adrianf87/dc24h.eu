# Search Security — alpha-0.5.11

**Current increment — alpha-0.5.12:** [Search Configuration and Statistics](SEARCH_CONFIGURATION.md) exposes 19 cumulative hub SCH/RES counters, a lazy stored-token-window gauge and all 16 effective startup search settings through the authenticated read-only API and bilingual WebAdmin Search page. Queue acceptance is distinct from delivery; direct client UDP is excluded. No payload/history or configuration write endpoint is added.

Local verification on 2026-11-13: **118/118 CTest suites passed in each of Debug, Release and ASan/UBSan**, with required integration tools and default leak detection. This includes search sender/recipient count/byte limits, permissions/revocation, native bounds, startup validation, real ncdc security denials and prior ADC/ADCS/UDP interoperability, MariaDB and Apache HTTPS. No compiler warnings. WebAdmin build/typecheck, 4 Node tests, 6 Chromium tests and real C++ API integration passed. Metadata, UTF-8, language parity/local documentation links, changed Python syntax, shipped INI and whitespace checks passed. Remote CI, the full OS/client matrix and production load remain unverified.

Based directly on main `c20b6d4bdbcf1b2fcdb7f2ded1f814988a37446a`, after the alpha-0.5.10 merge. Existing branches/history are preserved; this branch is published separately from main.

## Enforcement and permissions

`search/security.hpp` defines checked SecuritySettings, Permissions, Operation, authorization and a fixed-size SearchGuard per connection lifetime. LoginHub owns enforcement on the actual ADC/ADCS path. Search Core and the low-level adapters remain trusted building blocks; calling them directly does not apply transport quotas automatically.

The existing connection-state, ready-session and source-SID checks retain fatal 244/240 precedence. For supported SCH/RES commands, the hub then checks global enablement and the sender's session permission, debits the applicable operation/byte quota, optionally requires a result token, and only then performs typed validation, recipient selection, active-token publication and routing. Malformed semantic, criterion-free, duplicate-token and unavailable-recipient attempts consume the sender quota. Raw syntax/escape/UTF-8 failures remain covered by the existing transport/parser limits. Denied queries never create a plan, reserve TO or fan out data.

`queries_enabled` / `results_enabled` override all session grants. `guest_queries` / `guest_results` initialize each new peer's permission to originate SCH / DRES. Current users are verified ADC guests; no account/password/role authentication is added. Client INF CT/SU, SUP, CID proof, nicknames and TO never grant capabilities. Existing privilege-claim rejection remains authoritative.

The trusted owner-thread C++ method `LoginHub::set_search_permissions(id, permissions)` updates an existing **ready lifetime ID**, returning false for absent/unready/closing sessions. It has no wire command, API endpoint or WebAdmin control. Query revocation forgets that owner's active token windows; a later grant starts new windows. Permission changes preserve rate credits and notice history. Disconnect/reconnect and SID reuse do not inherit a trusted grant. Query permission controls origination, not a general right to receive unsolicited tokenless data; enable strict result tokens when tokenless compatibility is unwanted. Already queued FIFO messages and direct UDP packets cannot be retracted by revocation.

## Count, byte and recipient limits

Query, sent-result and received-result buckets are independent; each combines operation-count and decoded-byte credits. Limits start full, refill continuously using monotonic explicit time and debit count/bytes atomically. A denial consumes neither partial budget. Backward clocks are clamped; extreme time differences and sizes saturate without signed overflow. Guard admission, notice decisions and permission updates allocate nothing and retain no query text, filenames, roots, tokens or identity history.

Costs include the decoded FOURCC, source/target headers, selectors/signs, parameter names/values (including unknown fields) and separators, excluding LF. Native objects exceeding the shared frame/field bounds receive a saturating oversized cost before typed parsing. Costs do not multiply by fanout; existing output byte caps and connection/frame ceilings continue to bound transport work.

A responder's outgoing result quota cannot bypass the receiver's independent incoming-result quota by switching sources. The received-result bucket is per recipient lifetime, shared across all DRES senders. Recipient exhaustion drops the result before queue insertion and keeps the recipient/source usable. No payload is retained for deferred replay. Direct client URES does not pass through this guard; neither hub TLS nor hub quotas police external UDP.

Security refusal notices share **one allowance per second per source connection** across query/result, permission, strict-token and incoming-recipient-limit denials. Successful traffic and permission changes do not reset that allowance. Existing typed-validation/active-token errors still use their fixed diagnostics, but their requests consume sender quota. Target queue cap/allocation failures retain prior isolation and do not allocate a response on the source.

| Security condition | Fixed response, when notice allowance is available |
| --- | --- |
| Globally disabled operation | `ISTA 125 Search\soperation\sis\sdisabled FC<FOURCC>` |
| Missing session permission | `ISTA 125 Search\soperation\snot\spermitted FC<FOURCC>` |
| Sender count/byte exhaustion | `ISTA 100 Search\srate\slimit\sexceeded FC<FOURCC>` |
| Strict policy without TO | `ISTA 100 Search\sresult\stoken\srequired FCDRES` |
| Receiver count/byte exhaustion | `ISTA 100 Search\srecipient\srate\slimit\sexceeded FCDRES` |

No success acknowledgment, reflected query/result data, fatal disconnect for a normal security denial, search log stream, database record or search API/GUI counter is introduced. Chat and its separate anti-flood/counters are unchanged.

## Startup configuration

All settings are in `[search]` in the shipped [example INI](../../config/dc24h.ini.example). They are validated before listeners, including when set through trusted network/LoginHub options. Keys/sections are case-sensitive; duplicate/unknown keys, invalid booleans, nonpositive/out-of-range numbers and cross-limit errors fail startup. There is no hot reload or new CLI override.

| Key | Default | Bounds / meaning |
| --- | --- | --- |
| queries_enabled / results_enabled | on / on | Global SCH / DRES switches. |
| guest_queries / guest_results | on / on | New guest session origination permissions. |
| require_result_token | off | on refuses tokenless DRES; off preserves passive ncdc compatibility. Supplied TO always retains active target-owner/generation checks. |
| max_active_searches | 4096 | 1..65535 total token windows. |
| max_active_per_user | 8 | 1..64, no greater than total. |
| search_lifetime_seconds | 60 | 1..3600; monotonic, lazily expired. Results never renew deadlines. |
| query_rate / query_burst | 10 / 32 | Operations per second / initial count credit; each 1..1000000. Shared by B/F/D/E SCH. |
| query_byte_rate / query_byte_burst | 32768 / 131072 | Decoded bytes per second / credit; rate 1..16777216, burst 16384..16777216. |
| result_rate / result_burst | 64 / 128 | DRES operations per second / credit; same bounds as queries. Applied independently to outgoing and incoming results. |
| result_byte_rate / result_byte_burst | 262144 / 1048576 | Decoded-byte rate/credit, applied independently to outgoing/incoming results; same bounds as query bytes. |

Trusted `LoginLimits.search` and `network::Options.search` carry the same settings; active limits now live in `.search.tokens` instead of the older LoginLimits `.search_tokens`. Core's TokenLimits interface remains unchanged and shares the same checked validation. Existing parser field/text/hash limits, total/per-peer output caps, ready/source ownership, lifetime-bound dispatch and active TO cleanup remain in force.

## Verification and remaining boundaries

`search.security` covers independent count/byte/recipient budgets, atomic debit, exact fractional refill, shared notices, extreme/backward clocks and sizes, allocation-free flood handling, malformed/native oversized attempts, permission precedence, trusted grants/revocation, unchanged credits, reconnect defaults, strict-token behavior and multi-sender recipient protection. Existing parser stress tests explicitly raise their query quota so semantic coverage is independent of anti-flood.

`search.security.process` exercises real ADC/ADCS sender/recipient exhaustion, silent repeated denial, recovery, guest/global switches, strict tokens, configured active total/per-owner caps, expiration, reconnect cleanup, source/state precedence, configuration failures and API/log/chat privacy. Real `adc.searchsecurity.ncdc` / `adcs.searchsecurity.ncdc` show a permission-denied search in ncdc and prove the connection/chat remain usable; existing active/passive UDP, feature/result, filter/TTH, queue and lifetime tests remain enabled.

This release adds hub-side transport admission, not persistent accounts/roles, a responder allowlist, authenticated file/filter claims, per-search result-count/deduplication policy, NAT/SUDP enforcement, distributed IP limits beyond the existing security module, or production-load certification. EN/US remains the implementation language; documentation and existing runtime catalogs remain US/PL. No new dependency, migration, operator catalog entry, write API or WebAdmin search page is added.
