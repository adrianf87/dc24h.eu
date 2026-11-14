# WebAdmin Transfer Monitoring — alpha-0.6.12

**Local alpha-0.6.12 validation (2026-11-14, Debian 13):** All 173 CTest suites passed in Debug (including release-metadata/version rechecks) and all 173 passed in the complete Release run on Debian 13, with required integration tools and real ncdc. Both C++ builds were warning-free. WebAdmin production build/typecheck, 20 Node tests, 13 Chromium tests and actual C++ API-to-React CTM/RCM rate integration passed. Desktop/mobile PL screenshots were inspected; release metadata, UTF-8/language parity/local links, changed Python syntax and whitespace checks passed. Other supported operating systems, production load and the new remote CI run remain unverified.

Based directly on main `d24df98b9903ff259d01c87f5e1412bd0a87f6d5`, after the alpha-0.6.11 ncdc bind-scope repair. Earlier alpha-0.6.1–alpha-0.6.11 implementations and branches already exist; this increment adds the remaining monitoring stage without replacing them.

## Operator view

Open **Connections** to view recent connection-coordination activity above the cumulative [Transfer Statistics](TRANSFER_STATISTICS.md). Four cards show received CTM requests/second, received RCM requests/second, refused attempts/second and queued recipient copies/second. A two-series chart shows CTM/RCM on a shared scale at their actual observation times. **Recent intervals** opens an accessible table with the last six intervals, oldest first; numbers follow EN/US or Polish locale with at most one decimal.

The existing 22 cumulative counters remain visible: separate CTM/RCM outcomes, recipient queue copies/refusals/cancellations/allocation errors, and peer STA 41/42 reports. Queue acceptance does not prove socket transmission, an established client connection, file progress or completed transfer. Peer error reports are unverified client claims. No bandwidth, live file-transfer count or per-user transfer state is inferred.

The shared five-second polling switch and manual refresh drive all pages through one status request. Pausing retains the last observed interval and labels it as paused. **Clear local observations** clears only transfer observations in this browser; it sends no API mutation, changes no cumulative counter and leaves Search observations alone. Navigation and language changes retain history; page reload discards it. Older APIs without transfer statistics show unavailable values and an unavailable-data message instead of fabricated zeros.

## Sampling contract

`lib/transfer-monitoring.ts` owns at most **30 observations** (29 intervals), each containing the 22 allowlisted cumulative counters, server version/uptime and browser monotonic time. The existing `useHub` hook retains this bounded state. There is no local/session storage, database, durable history, extra endpoint or second polling timer.

Rates are observed interval averages: each individual counter delta is divided by elapsed `performance.now()` seconds, then related rates are summed. Subtraction before summation avoids rounding large cumulative totals near JavaScript's safe-integer boundary. The adapter rejects negative, fractional, missing or unsafe values in a present statistics extension. Independent API counter sampling does not require cross-counter outcome sums to match at any instant.

| Card | Counter deltas |
| --- | --- |
| CTM requests / s | `ctm_received` |
| RCM requests / s | `rcm_received` |
| Refused attempts / s | CTM and RCM `rejected`, `rate_limited`, `recipient_limited` |
| Queued copies / s | `ctm_queued` + `rcm_queued` |

Refused-attempt rates cover protocol/state/source/capability/policy and source/recipient quota denials. Internal errors, recipient queue refusals, canceled copies and peer reports remain separate cumulative details. One echoed request can queue two copies, so queued copies are not a count of unique attempts or connections.

The first successful response establishes a baseline; cards show em dashes until a second observation at least **one second** later. Equal counters then produce zero activity. Faster manual refreshes update ordinary snapshot values without replacing the retained baseline. Intervals up to **15 seconds** are accepted; a longer gap starts a new baseline. A backward browser timestamp, changed version, lower uptime or decrease in **any** of the 22 counters also resets the window. No line or rate bridges a detected restart.

HTTP failures, request timeouts, malformed responses, invalid numbers and missing transfer extensions clear observations. Recovery starts from a new baseline. Superseded aborted requests cannot replace newer observations. No missing interval is filled with zero. Reset detection uses the existing version/uptime/counter signals: a restart already caught up with all old values cannot be distinguished without a future server epoch identifier.

## Privacy and deployment

The existing authenticated [status API](API.md), private Unix socket and server-side credential arrangement are reused. Monitoring contains no SIDs, CIDs, nicknames, addresses/ports, protocol/token strings, file names or payloads. Snapshot export retains its fixed allowlisted cumulative format; browser timestamps and derived intervals are not exported. This increment adds no API write, SQL migration, dependency, INI key or production service activation. Follow [WebAdmin](WEBADMIN.md) and [Apache HTTPS](APACHE2_HTTPS.md) for existing build/hosting constraints.

## Verification

`transfer-monitoring.test.ts` covers exact CTM/RCM/refusal/copy deltas, idle zero, all 22 counter regressions, uptime/version/clock resets, missing extensions, one/fifteen-second boundaries, rapid refreshes, owned allowlists, bounded eviction and safe-integer arithmetic. Browser tests cover polling, chart/table values, navigation retention, pause/manual refresh, local clear, cumulative export, mobile Polish layout, malformed/missing/error data and reset/gap recovery.

The real C++ → private API → Vite adapter → Chromium harness establishes a browser baseline before sending more CTM and RCM frames through actual hub sockets. It checks positive observed rates and cumulative counters in React, retained Search monitoring, credential/payload isolation and mobile layout. Existing CTest suites use real **ncdc** for ADC/ADCS, CTM/RCM, TLS peer handshakes, modes, tokens, permissions, errors and limits. Final validation results and remaining platform limits are recorded in [Changelog](../../CHANGELOG.md) and [Testing](TESTING.md).
