# WebAdmin Search Monitoring — alpha-0.5.13

**Current increment — alpha-0.6.5:** [Protocol Negotiation](PROTOCOL_NEGOTIATION.md) gives Transfer Core owning protocol values and explicit ADC/1.0 plaintext / ADCS/1.0 TLS intent. Pure preference/capability selection never downgrades required TLS. Live CTM/RCM preserve the selected name and extension compatibility; peer failure replies retain opaque tokens. Real ncdc tests cover certificate-checked TLS peer handshakes over both hub transports, alongside the terminal-backpressure regression.

**Increment delivered in alpha-0.6.4:** [Connection Routing](CONNECTION_ROUTING.md) consolidates D/E CTM and RCM admission and delivery into one SID router, one immutable packet type and one bounded queue path. Recipients are fixed ready-session lifetimes: D targets only, E targets then independently echoes to the source. Tokens never select recipients; existing roles, wire validation, errors and privacy remain. Mixed-pair tests use the same token without crossing audiences.

**Increment delivered in alpha-0.6.3:** [ADC RCM](ADC_RCM.md) adds owning D/E reverse-request validation and live target/echo routing through Transfer Core, with per-family passive-source/active-target checks, stable lifetime/address admission and shared bounded queues. CTM/RCM share wire-field validation. Real ncdc covers RCM→CTM and peer handshakes in both roles; no pending-token registry or file-data handling is claimed.

**Increment delivered in alpha-0.6.2:** [ADC CTM](ADC_CTM.md) adds owning D/E CTM validation and live target/echo routing over Transfer Core, with checked source TCP activity, lifetime/endpoint rechecks, shared bounded queues and real ncdc client-handshake interoperability. Protocol/port/token/extensions retain their checked spelling and privacy. RCM and file-data handling remain later stages.

**Increment delivered in alpha-0.6.1:** [Transfer Core](TRANSFER_CORE.md) adds an independent C++20 connection-coordination engine: bounded owning intents, active/passive IPv4/IPv6 roles and immutable session-bound one-attempt plans with cancellation and dispatch rechecks. Shared Users address classification preserves Search behavior. Live CTM/RCM routing and file data transfer remain future work.

**alpha-0.5.13 local validation (2026-11-13):** Debug server build and all **120/120 CTest suites** passed with required integration tools, including real ncdc ADC/ADCS/UDP, MariaDB and Apache HTTPS. WebAdmin production build/typecheck, **12 Node tests**, **10 Chromium tests** and real C++ API-to-React integration with positive search rates passed. Desktop and mobile PL views were inspected. Metadata, UTF-8, language parity/local links, changed Python syntax, shipped INI and whitespace checks passed; no compiler warnings. This increment changes frontend monitoring and release metadata, with no C++ search algorithm/API change. Remote CI, the full OS/client matrix and production load remain unverified.

This increment is based directly on main `dbd5f74dacdb409c073c99df805ff419975894d0`, after alpha-0.5.12 was merged. Existing code/history and US/PL catalogs are preserved. The feature branch is published separately; main is unchanged.

## Operator view

Open **Search** in WebAdmin to see recent aggregate hub search activity above the existing cumulative statistics and effective startup configuration. The page supports EN/US and Polish, desktop and mobile navigation, manual refresh and the shared five-second automatic polling switch. It uses the existing authenticated [status API](API.md), proxy and private credential arrangement. There is no second polling request, endpoint, write operation, dependency or migration.

Four cards show received queries/second, received results/second, refused attempts/second and queued recipient copies/second for the last accepted interval. A two-series chart shows query/result rates on a shared scale with actual elapsed sample positions. **Recent intervals** opens an accessible table of the last six intervals, oldest first. Table values and chart labels use the selected locale and at most one decimal. The chart has an accessible name; tabular values are available without interpreting its colors.

Cumulative counters and all 16 read-only settings from [Search Configuration](SEARCH_CONFIGURATION.md) remain below the monitor. Stored token windows still have lazy expiry; they are not an exact live-search count. Queued copies confirm output queue acceptance, not delivery. Direct client URES never enters the hub API or this monitor. No query text, filenames, TTH, token, SID, CID, per-user list or search history is shown.

## Sampling contract

`lib/search-monitoring.ts` keeps up to **30 observations**, giving at most 29 intervals. Each observation owns an allowlisted copy of the 19 cumulative counters, server version/uptime and a monotonic browser timestamp. It excludes the token-window gauge, configuration and unknown API fields. The shared `useHub` hook retains this bounded history while navigating between pages or changing language. Nothing is persisted in local/session storage, a database or a server-side history.

Rates are estimates: `(new counter - previous counter) / elapsed seconds`. Browser `performance.now()` measures time between successfully parsed responses; server uptime is only a reset signal. These are observed interval averages, not instantaneous load, configured rate limits or network-delivery rates. Independent API counter sampling remains valid; no received/outcome sum equality is imposed.

| Display | Counter deltas used |
| --- | --- |
| Queries / s | queries_received |
| Results / s | results_received |
| Refused attempts / s | queries_rejected + queries_rate_limited + results_rejected + results_rate_limited + results_recipient_limited |
| Queued copies / s | query_queued + result_queued |

Refused-attempt rates cover validation/state/source/policy and sender/recipient quota outcomes. They do not add queue-refusal copies, ignored queries or internal errors to this attempt metric. A single query can queue many recipient copies. The underlying cumulative details remain available to distinguish outcomes.

The first accepted snapshot is a baseline: all four rates show em dashes and the chart waits for two observations. Equal counters in a later interval correctly produce zero activity. Polls less than **one second** apart update the ordinary snapshot but do not append an observation; their eventual increments remain included relative to the retained baseline. Intervals up to **15 seconds** are accepted. A longer gap, backward timestamp, version change, lower uptime or decrease in **any** of the 19 cumulative counters starts a new baseline without a negative rate or a connecting line across the reset. Gauge decreases do not reset rate history.

A failed HTTP request, timeout, malformed response, unsafe numeric value or missing search extension clears search observations. Older APIs without `search` retain the established unavailable-data state. Recovery begins from a fresh baseline. An aborted superseded request cannot clear a newer request's state. The monitor never fills missing intervals with zero or bridges an outage.

Disabling automatic polling keeps the last observed interval and explicitly labels polling as paused. Manual refresh still works. **Clear local observations** clears this browser history only: it sends no API mutation and leaves cumulative counters/settings untouched. The next accepted poll establishes a fresh baseline. Reloading the browser also discards observations. Snapshot export remains the fixed allowlisted cumulative API snapshot; it does not export browser timing or derived history.

Reset detection uses available version/uptime/counter signals. A restart that has already caught up with every previous counter and uptime cannot be distinguished with this API contract. No process identity/epoch endpoint or durable monitoring service is claimed.

## Verification

The monitoring unit tests exercise exact deltas, idle zero, every cumulative regression, gauge changes, uptime/version resets, invalid clocks, one-second/fifteen-second boundaries, rapid refresh retention, owned allowlisted copies, 30-observation eviction and the JavaScript safe-integer boundary. Browser tests exercise actual shared poll updates, chart/table values, pause/manual refresh, local clear, unchanged cumulative export, mobile PL layout, missing/invalid/error responses and restart/gap recovery.

The real C++ API integration establishes a browser baseline, then sends two additional ADC query/result pairs through actual hub sockets and checks positive observed rates and cumulative totals in React. Existing ncdc ADC/ADCS/UDP interoperability remains covered by server tests. Validation results are recorded in [CHANGELOG](../../CHANGELOG.md); remote CI, the full OS/client matrix and production load require separate verification.
