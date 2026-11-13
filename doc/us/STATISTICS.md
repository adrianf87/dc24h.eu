# Statistics — alpha-0.3.13

**Current increment — alpha-0.5.13:** [WebAdmin Search Monitoring](WEBADMIN_SEARCH.md) adds browser-local query/result/refusal/queue rates, a bounded traffic chart and accessible recent-interval table. First samples, gaps, detected resets and failures start fresh baselines; pause/manual refresh and local clear preserve cumulative counters. Existing API/settings and credential isolation remain.

**Increment delivered in alpha-0.5.12:** [Search Configuration and Statistics](SEARCH_CONFIGURATION.md) exposes 19 cumulative hub SCH/RES counters, a lazy stored-token-window gauge and all 16 effective startup search settings through the authenticated read-only API and bilingual WebAdmin Search page. Queue acceptance is distinct from delivery; direct client UDP is excluded. No payload/history or configuration write endpoint is added.

Built directly from main `59b7b3a048b6d00504038a6a0d0b6580f3c04907`, which already includes alpha-0.3.1–alpha-0.3.12. Earlier branches and history are preserved.

## Session ownership and accounting

`users::Statistics` is a value snapshot owned by SessionManager. `statistics()` and `counts()` are O(1), allocation-free, noexcept owner-thread queries. No mutable models escape. Every successful phase change updates the two affected gauges after model commit. Failed validation, stale IDs, capacity refusals and allocation rollback leave statistics unchanged. Repeated close/finish does not count another event; rename/profile updates do not count another login.

| Field | Meaning |
| --- | --- |
| `sessions.total` | All currently owned models, including closed models awaiting removal |
| `sessions.accepted/negotiating/identified/ready/closing/closed` | Disjoint current phases; their sum equals total |
| `online()` | Exactly `sessions.ready` |
| `peak_online` | Highest simultaneous ready gauge in this manager lifetime |
| `created_sessions` | Successful model additions, counted after allocation/insertion |
| `successful_logins` | First transition to ready for each lifetime |
| `completed_sessions` | First transition to closed, including anonymous failures and shutdown |

A verified identity awaiting welcome is identified, consumes user capacity, and is not online. Ready begins only when transport accepts the final LF of the own INF welcome. Closing immediately leaves online while identity reservations remain until removal. Direct EOF/forced cleanup from any phase also updates the gauges. Removal only reduces total/closed; historical counters remain. Integer cumulative Users counters saturate at UINT64_MAX rather than wrap. Stats reset with a new manager/process; they are not persisted to MariaDB.

## Network publication and API

The reactor alone reads SessionManager and publishes only numeric values to Network atomics before waiting and on reactor exit. The API thread never reads a Session, registry, string or container. Gauges may lag an in-flight event until its reactor publication boundary; independently sampled fields are not a transaction. Do not require cross-field equalities while connections are changing. Current active/online gauges reset to zero on normal or exceptional reactor exit. Historical totals remain queryable on the C++ TcpServer object after run returns; the HTTP worker stops during process shutdown.

`GET /api/v1/status` keeps existing v1 fields and adds:

| JSON field | Meaning |
| --- | --- |
| `users.online` | Current ready users, ADC and ADCS combined |
| `users.peak_online` | Peak ready users since startup |
| `users.successful_logins` | Completed welcomes since startup, including reconnects |
| `network.rejected_connections` | All transport admission refusals; includes existing security_rejected subset |
| `network.closed_connections` | Admitted transport sockets released, including anonymous/TLS failures and shutdown |
| `network.peak_connections` | Most concurrently admitted sockets, including anonymous and closing peers |
| `network.received_bytes/sent_bytes` | Application bytes actually read/written, including ADC framing; excludes TLS handshake/record overhead |

Existing `active_connections` counts admitted sockets; `accepted_connections` is cumulative admitted sockets, not raw accept4 attempts and not logins. A failed ADC identity after admission increments accepted/closed, not transport rejected. A refused socket never increments closed. API HTTP sockets are excluded from all hub connection/user counters. Existing ADCS and security counters retain their meaning. Byte counters measure hub protocol traffic, not peer-to-peer file transfers. At a quiescent point accepted = active + closed; independent concurrent samples need not satisfy it.

The additive fields use numeric JSON values, with no nickname, SID, CID, PID, peer IP, credentials or paths. Existing same-UID Unix socket and Bearer requirements also protect these fields. No public listener or route is added. JSON serialization preserves uint64 values; WebAdmin rejects integers beyond JavaScript's safe range rather than showing an imprecise count.

## WebAdmin and compatibility

Overview shows online, peak online, successful logins and closed connections. Connections shows peak peers, all admission refusals and raw received/sent byte totals. Labels and hints are EN/US by default with PL available. Existing monitoring/navigation/chart behavior stays available. Unknown response fields are stripped before browser rendering/export. Missing/invalid counters make the snapshot unavailable, clear stale values and disable export.

Older v1 consumers can ignore the added fields. Deploy this WebAdmin with alpha-0.3.13 or later implementing these fields; the stricter new parser intentionally does not invent zero statistics for an older backend. There are no new configuration options, dependencies, SQL migrations or ADC wire commands. Accounts, long-term metrics storage, rates, histograms, per-user statistics and remote administrative access remain future work.

## Validation

`users.statistics` reconciles cached counts against owned model snapshots across all phases, errors, allocation failures, repeated operations and teardown. It tests partial final-LF completion and canceled welcomes through LoginHub. `users.statistics.process` uses mixed real ADC/ADCS sockets plus authenticated API readers: anonymous TLS, transport refusal, invalid proof, INF rename, EOF, reconnect, fatal close, peak/totals/bytes and private-data exclusion. Actual ncdc ADC/ADCS verifies completed login, reconnect and post-quit online counts. Existing lifecycle, limits, TLS, database and Apache regressions remain required. WebAdmin parser, browser and live C++ API tests cover presentation, US/PL and safe export.

Protocol reference: [official ADC specification](https://adc.sourceforge.io/ADC.html). Client reference: [ncdc](https://dev.yorhel.nl/ncdc). This increment adds observation only; existing ADC login rules remain in force.
