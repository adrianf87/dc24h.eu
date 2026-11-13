# Search Configuration and Statistics — alpha-0.5.12

Local verification on 2026-11-13: **120/120 CTest suites passed in each of Debug, Release and ASan/UBSan**, with required integration tools and default leak detection, including real ncdc ADC/ADCS/UDP, MariaDB and Apache HTTPS. No compiler warnings. WebAdmin build/typecheck, 6 Node tests, 8 Chromium tests and real C++ API integration passed. Metadata, UTF-8, documentation language parity/local links, changed Python syntax, shipped INI and whitespace checks passed. Remote CI, the full OS/client matrix and production load remain unverified.

The alpha-0.5.12 branch starts from main `c20b6d4bdbcf1b2fcdb7f2ded1f814988a37446a` and incorporates the completed Search Security branch `a17009447b9375feca932be58ba39e555937931d` before adding this increment. Main is unchanged; existing code and history are preserved.

## Effective startup configuration

The 16 checked `[search]` settings from [Search Security](SEARCH_SECURITY.md#startup-configuration) now appear in the authenticated `GET /api/v1/status` response and the WebAdmin Search page. Their defaults and bounds are unchanged. The shipped [example INI](../../config/dc24h.ini.example) remains the source for operator edits; validate with `dc24h --config PATH --check-config` and restart to apply changes. There is no hot reload or configuration write endpoint. The UI shows actual effective startup values, including defaults when omitted from the INI, rather than a second editable configuration.

The five policy booleans are `queries_enabled`, `results_enabled`, `guest_queries`, `guest_results`, `require_result_token`. The eleven numeric limits are `max_active_searches`, `max_active_per_user`, `search_lifetime_seconds`, `query_rate`, `query_burst`, `query_byte_rate`, `query_byte_burst`, `result_rate`, `result_burst`, `result_byte_rate`, `result_byte_burst`. Incoming and outgoing result budgets use these result settings independently. Global/guest policy is exposed; trusted per-session permission grants are not.

## Aggregate counter contract

`search/statistics.hpp` defines 19 saturating unsigned 64-bit cumulative counters and one gauge. LoginHub owns updates on the reactor thread. Every decoded SCH or RES received for an existing nonclosing peer has exactly one received increment and one terminal outcome. This includes source/state/policy rejection and silent flood denials. Raw codec failures, calls to trusted Core/router APIs outside LoginHub, traffic after closing and direct client UDP do not count. Counters reset with a new hub process; they are not persisted.

| Query field | Meaning |
| --- | --- |
| queries_received | All decoded SCH attempts in scope. |
| queries_routed | Dispatch returned a report, including empty/zero-queue fanout. |
| queries_rejected | State/source/policy, malformed semantics, duplicate/cap/inactive token or unavailable target rejection. |
| queries_rate_limited | Source operation/byte quota denied. |
| queries_ignored | Admitted query without criteria. |
| query_errors | Source preparation/internal dispatch failure, including allocation failure. |
| query_queued | Recipient copies accepted into output queues. |
| query_unavailable | Unavailable directed target or dispatch recipient. |
| query_refused | Recipient copies refused by the queue/sink. |
| query_canceled | Planned recipient copies canceled by a dispatch recheck, including source closure. |

| Result field | Meaning |
| --- | --- |
| results_received | All decoded RES attempts in scope. |
| results_routed | Dispatch returned a report without recipient quota denial, even if no copy queued. |
| results_rejected | State/source/policy/token/semantic/target rejection. |
| results_rate_limited | Sender result operation/byte quota denied. |
| results_recipient_limited | Target incoming-result quota denied; counted even when notice suppressed. |
| result_errors | Source preparation/internal dispatch failure. |
| result_queued | Copies accepted into the target queue. |
| result_unavailable | Unavailable target at preparation or dispatch. |
| result_refused | Copies refused by the target queue/sink, including incoming-quota denial. |

Received equals the sum of its five outcome counters before saturation, for a single owner-thread snapshot. Queue counters are separate dimensions; one broadcast may queue many copies, and target failure is not a source error. Queued means accepted for output, not socket delivery or client acceptance. Security notices and system responses do not count as search copies. Target queue allocation failure remains a refusal and isolates that target.

`tracked_searches` is the number of **stored token windows** in Core. Expiry is lazy, so an expired record can remain until normal Core cleanup; viewing the status never forces an expiry sweep. Disconnect, query permission revocation and explicit cancellation/normal cleanup remove windows. Tokenless searches do not add records. This is neither a file/result count nor an exact count of currently unexpired searches. The UI labels it “Tracked token windows” with the lazy-expiry explanation.

## API and threading

The existing local authenticated API adds one top-level object; authentication, Apache proxy and credential handling remain as documented in [API](API.md):

```json
{"search":{"statistics":{"queries_received":0,"queries_routed":0,"queries_rejected":0,"queries_rate_limited":0,"queries_ignored":0,"query_errors":0,"query_queued":0,"query_unavailable":0,"query_refused":0,"query_canceled":0,"results_received":0,"results_routed":0,"results_rejected":0,"results_rate_limited":0,"results_recipient_limited":0,"result_errors":0,"result_queued":0,"result_unavailable":0,"result_refused":0,"tracked_searches":0},"configuration":{"queries_enabled":true,"results_enabled":true,"guest_queries":true,"guest_results":true,"require_result_token":false,"max_active_searches":4096,"max_active_per_user":8,"search_lifetime_seconds":60,"query_rate":10,"query_burst":32,"query_byte_rate":32768,"query_byte_burst":131072,"result_rate":64,"result_burst":128,"result_byte_rate":262144,"result_byte_burst":1048576}}}
```

The reactor publishes counters/gauge to atomic fields read by the API worker. Individual fields are safe to sample, but a response is not a transaction across counters; clients must not reject a sample because cross-field sums temporarily differ. Configuration is immutable after checked startup. Polling does not reset counters, acquire Core state from another thread, or allocate a token registry copy.

Only fixed aggregate names and settings are serialized. There are no query strings, result names, TTH/CID/SID, tokens, per-session permissions, credentials, search history or database records. Direct URES between clients bypasses the hub and is excluded; ADCS does not encrypt that independent UDP traffic.

## WebAdmin and compatibility

The Search navigation page contains the tracked-window gauge, ten query metrics, nine result metrics and sixteen read-only configuration values. EN/US and PL labels, desktop/mobile layout, existing refresh/disconnection behavior and allowlisted JSON snapshot export are supported. No browser-held API credential or configuration form is added.

Older API responses with no `search` extension remain valid: the page explicitly reports unavailable search data and shows em dashes. A present but incomplete/invalid extension invalidates the snapshot, using the existing connection-lost state. Unknown fields are stripped before display/export. All counters must be nonnegative JavaScript safe integers; values beyond `Number.MAX_SAFE_INTEGER` are rejected rather than rounded, although the server continues counting up to uint64 saturation. Settings use the same numeric bounds and per-owner/total rule as startup.

## Verification and boundaries

`search.statistics` verifies exact outcomes, ignored/malformed/duplicate/denied/flood attempts, permission grants, source/target quotas, queue refusal/cancellation, cleanup/reset, allocation failure, saturation and allocation-free accounting. `search.statistics.process` checks exact ADC/ADCS counters, effective settings, authentication/privacy, tracked-window cleanup and process restart. Existing ncdc active/passive scenarios assert hub SCH/DRES accounting while external URES stays excluded. Frontend tests check all values/settings, both languages, refresh/reset, legacy/malformed API, export filtering and actual C++ API-to-React traffic.

Validation results are recorded in [CHANGELOG](../../CHANGELOG.md). No new dependency, migration, payload logging, configuration write API, persistent history, result deduplication or production-load certification is introduced. Remote CI and the full OS/client matrix require separate verification.
