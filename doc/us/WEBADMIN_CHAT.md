# WebAdmin chat statistics — alpha-0.4.12

Based directly on main `be34e21e3ebad17b3f75cd5277a3dd6023c043d2`, after Chat Logging. This increment consumes the existing `/api/v1/status.chat` fields. Server behavior, counter definitions, wire protocol and API schema remain those described in [Chat Logging](CHAT_LOGGING.md).

## Operator view

Open **Chat** in the monitoring navigation, use the **View all chat statistics** link on Overview, or load `#chat` directly. Browser reload/back/forward retains hash-based navigation. The shared language preference switches all labels and hints between EN/US and Polish; numeric formatting uses `en-US` or `pl-PL`. The mobile menu closes after navigation and supports scrolling on short screens.

The page contains nine values, directly from the validated API snapshot:

| Section | Label | API field | Meaning |
| --- | --- | --- | --- |
| Message attempts | Received attempts | `received` | Eligible decoded public/direct/echo client MSG attempts entering chat handling |
| Message attempts | Public chat | `public_dispatches` | Public dispatches returning a report, including actions |
| Message attempts | Private chat | `private_dispatches` | Direct/echo dispatches returning a report, including self messages |
| Message attempts | Rejected chat | `rejected` | Validation, policy or unavailable target failures before successful dispatch |
| Message attempts | Chat rate limits | `flood_limited` | Anti-Flood refusals, including silent ones |
| Message attempts | Internal errors | `errors` | Attempts interrupted by internal preparation/dispatch errors |
| Recipient queues | Queued copies | `queued` | Recipient output queue acceptances |
| Recipient queues | Unavailable recipients | `unavailable` | Dispatch-report recipients that became unavailable before queueing |
| Recipient queues | Refused copies | `refused` | Recipient queue refusals, including capacity/allocation failures |

The scope text explains that totals start at server startup and reset on restart, and exclude welcomes/system messages. Raw parser failures and transport/security refusals before chat handling are also excluded; see the owner model's precise boundaries in [Chat Logging](CHAT_LOGGING.md). A message can produce multiple recipient copies. Queue acceptance does not mean delivery or client read. A dispatch may have no accepted recipients. The page displays raw totals; it does not manufacture delivery rates, per-second traffic, percentages or transcript data.

Overview retains the existing four-counter summary and now links to the complete view. A shared Metric component preserves the established layout and styles. The dedicated page separates the six attempt counters from three recipient counters, adapts its grid to desktop/tablet/mobile widths, and wraps large values without page overflow. Real zero values remain `0`; unavailable counters use `—`.

## Refresh, failures and export

The existing refresh control and five-second automatic refresh apply to Chat. Turning automatic refresh off leaves manual refresh available. Totals are replaced from each successful snapshot, including lower/zero values after a server restart; browser navigation and language changes do not reset backend totals. No local delta/rate history is calculated for chat.

The existing bounded API reader and strict `parseSnapshot` check all nine counters. Missing fields, nonnumeric/negative/fractional/unsafe JavaScript integer values or a failed API request discard the whole snapshot. The page shows the existing connecting/unconfigured/offline banner, clears previous values to dashes and disables snapshot export until a valid response arrives. Live all-zero responses remain connected and exportable. API atomic fields are independently sampled, so the UI does not require their sum to match within one HTTP sample. Values above JavaScript's safe-integer limit are rejected rather than rounded.

**Export snapshot** uses the existing safe export: timestamp plus the entire normalized status snapshot, including the nine named `chat` counters. It is not a conversation export. Unknown fields and chat text/recipient/identity data are removed by the same parser allowlist before rendering or downloading. Local socket/token configuration remains server-side in the Vite adapter; neither the token nor arbitrary backend text is exposed to the browser. There are no chat commands, configuration mutations, user histories, new endpoints or credential inputs in this view. Production gateway/session requirements remain in [WEBADMIN.md](WEBADMIN.md).

## Validation and reproduction

Local verification for **alpha-0.4.12** on 2026-11-11: all **90/90 CTest suites** passed in Debug, including real ncdc ADC/ADCS, disposable MariaDB and Apache HTTPS. WebAdmin typecheck/production build, 4 Node tests, 6 Chromium tests and real C++ API → adapter → React integration passed. Desktop/mobile screenshots were inspected; the chat navigation/export test passed again after the sidebar scrolling adjustment. Metadata, UTF-8, local documentation links, shipped INI and whitespace checks passed. These are local results; Release/ASan/UBSan were not rerun for this UI increment, and remote CI, other browsers/OS combinations and production load are unverified.

The browser suite covers exact values for every counter, locale formatting and maximum safe integer wrapping, Overview links, direct `#chat`, active navigation, reload, mobile menu closure, US/PL copy, refresh, restart-to-zero, unavailable/invalid/missing API data, recovery and export allowlisting. Existing parser/adapter tests retain per-field numeric validation and secret isolation.

Real integration starts C++, logs in two ADC clients, sends BMSG/DMSG/EMSG, then causes missing-target and text-limit rejections. It verifies nonzero backend totals through Unix API → Vite adapter → React, all nine desktop/mobile values and absence of private markers and credentials from API/HTML/logs. The server's existing CTest ncdc ADC/ADCS suites still verify real client compatibility; this UI increment does not add a protocol feature.

```sh
cd webadmin
npm run build
npm test
PLAYWRIGHT_CHROMIUM_EXECUTABLE=/usr/bin/chromium npm run test:e2e
PLAYWRIGHT_CHROMIUM_EXECUTABLE=/usr/bin/chromium npm run test:integration -- ../build-debug/dc24h
```

Omit `PLAYWRIGHT_CHROMIUM_EXECUTABLE` when using Playwright-installed Chromium. See [TESTING.md](TESTING.md) and [CHANGELOG.md](../../CHANGELOG.md) for measured results. Remote CI, other browsers, the full supported OS matrix and production load require separate evidence.
