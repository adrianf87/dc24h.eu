# Chat Logging — alpha-0.4.11

Based directly on main `2ad82a2beeb5b8b0963b8908657971c4d011e6dc` after Chat Configuration. Server code and wire diagnostics use EN/US UTF-8. Operator messages, help and documentation support US and PL.

## Safe event logging

Enable fixed outcome records with `[logging] chat=on` or `--log-chat on`. The default is off. CLI takes precedence over INI; accepted values are exactly `on` and `off`. Unknown, duplicate or malformed settings fail startup. Severity `warning`, `error` or `off` suppresses these INFO events and their summaries. Settings apply at startup.

```ini
[logging]
level = info
file = stderr
connections = off
chat = on
```

The five allowed event values are `public_dispatch`, `private_dispatch`, `rejected`, `flood_limited` and `internal_error`. A US record looks like `2026-11-11T00:00:00Z [INFO] Chat event=private_dispatch`; PL translates the fixed prefix to `Czat zdarzenie=` and retains stable event tokens.

No message body, configured greeting, arbitrary ADC field, nickname, SID/CID/PID, session ID, IP address, recipient, PM value or credential is passed to the chat logger. There is no transcript, per-user audit trail, conversation identifier or database persistence. EN/US remains the default; operator language does not alter wire messages.

Each Logger has an independent, mutex-protected, steady-clock budget of 32 chat outcome records per one-second window. Excess records increment a saturating suppression counter without formatting. The next window's first event, or Logger destruction, writes one WARN `Chat log events suppressed=N` summary (PL: `Pominięte zdarzenia logu czatu=N`). Connection logging retains its separate 64/s budget. A summary may accompany the first record of the next window. No timer emits periodic idle summaries.

The existing UTC logger provides bounded UTF-8 single-line formatting and best-effort failure handling. Allocation or write failures do not escape chat logging. A failed write still consumes its event allowance; suppression is a rate-limit count, not a disk-write success count. Existing trusted-directory, regular-file, no-symlink and mode-0600 rules apply. An existing log must already be mode 0600 and owned by this user. Logging uses the existing synchronous sink; this increment does not promise bounded disk latency or add log rotation/retention.

## Counter definitions

`LoginHub::chat_statistics()` is a no-allocation reactor-thread snapshot. One RAII attempt records exactly one outcome for each decoded client BMSG/DMSG/EMSG passed into `LoginHub::receive`, after the closing/immediate-close check. It defaults to rejection, so early state/source/flag/policy failures are counted. Failed preparation is an internal error. Fatal rejection is counted once; further input on that closing connection is excluded. Flood refusals count even when their protocol notice is suppressed. Counters use saturating uint64 addition and start at zero for each hub/server instance.

| API `chat` field | Meaning |
| --- | --- |
| `received` | Eligible decoded B/D/E MSG attempts |
| `public_dispatches` | BMSG dispatch calls returning a DispatchReport |
| `private_dispatches` | DMSG/EMSG dispatch calls returning a DispatchReport, including self messages |
| `rejected` | State/source/syntax/flag/policy failures or unavailable target before successful dispatch |
| `flood_limited` | Anti-Flood refusals, including silent ones |
| `errors` | Preparation/diagnostic allocation failures or unexpected dispatch errors |
| `queued` | Recipient queue acceptances summed from successful dispatch reports |
| `unavailable` | Dispatch-report recipients no longer available at dispatch time |
| `refused` | Dispatch-report recipient queue refusals, including output-cap/allocation failure |

Before saturation, an individual owner-thread snapshot satisfies `received = public_dispatches + private_dispatches + rejected + flood_limited + errors`. A dispatch is counted even if all recipients refuse; it does not mean successful transport delivery or client read. EMSG normally contributes two queue acceptances, or one when addressed to self. A target missing during preparation increments `rejected`, rather than dispatch-time `unavailable`. Recipient queue faults stay isolated and counted as refusals; no retry or extra message is introduced.

Parser-rejected raw frames, pre-LoginHub security/transport denials, input ignored while closing, unsupported non-B/D/E scopes, INF/STA/keepalives, welcome IMSG and trusted system `send_hub_message` are excluded. Existing protocol/network/security counters cover the other paths. This increment does not add system-message statistics or change protocol admission, routing, Anti-Flood, queues, readiness or ADC responses. Scope meanings follow the [ADC specification](https://adc.sourceforge.io/ADC.html).

## Network, API and WebAdmin

Network copies all nine totals into independent atomic counters on its existing publication cycle and on exit. API threads never access mutable session/chat models. `/api/v1/status` gains the fixed numeric object `chat`; existing local Unix socket and bearer authentication remain required. API snapshots sample fields independently and may transiently straddle an update; the arithmetic identity above is not a transaction guarantee for HTTP samples. Totals survive disconnects within the running process, then reset on restart.

WebAdmin strictly validates and allowlists all nine fields, rejects missing/negative/fractional/unsafe JavaScript integers, strips arbitrary fields and includes the safe counters in exported snapshots. Its overview shows public/private dispatch attempts, rejections and flood refusals in US/PL. Queue acceptance remains distinct from delivery. Server and WebAdmin should be updated together because the new fields are required by the client parser; there is no API mutation or new endpoint.

Chat accounting always runs, even when event logging is disabled or fails. Network sends a fixed event to Logger only when the corresponding outcome counter advances; a counter already saturated at uint64 maximum no longer produces that event. The hub/Core perform no logger, filesystem or database I/O.

## Verification

Local verification on 2026-11-11: all **90/90 CTest suites** passed in Debug, Release and ASan/UBSan with required integration tools and default leak detection, including real ncdc ADC/ADCS, disposable MariaDB and Apache HTTPS. The initial metadata check found outdated README introduction versions; after the documentation update it passed in all three configurations. Builds emitted no compiler warnings. WebAdmin typecheck/production build, 4 Node tests, 3 Chromium tests (including bilingual chat metrics) and real C++ API integration passed. The shipped INI, US/PL help, metadata, UTF-8 and local documentation links were checked. These are local results; remote CI, the full supported OS/client matrix and production-load guarantees are not claimed.

`chat.logging` tests no-allocation accounting, saturation, exact B/D/E/self fanout totals, missing targets, policy and silent-flood refusals, malformed source, ignored closing input, system-message exclusion, cleanup/restart isolation, allocation failure and recipient output limits. `chat.logging.process` exercises mixed ADC/ADCS routing, private recipient isolation using ordered INF fences, exact authenticated API totals, default/CLI configuration behavior, forged source rejection, silent flood accounting, rate-limited logs and omission of bodies/identifiers/credentials.

The logger suite covers opt-in and severity, invalid event values, independent connection/chat budgets, concurrent writers, PL prefixes, window rollover/shutdown summaries and a failing write sink. Actual ncdc ADC/ADCS tests enable chat logging, verify public/private counters and events and retain the earlier payload-privacy checks. WebAdmin tests validate the new contract, export stripping, bilingual metrics and real C++ API integration. See [TESTING.md](TESTING.md) and [CHANGELOG.md](../../CHANGELOG.md) for measured results and limits.

```sh
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug -DDC24H_REQUIRE_INTEGRATION_TESTS=ON
cmake --build build-debug --parallel 4
ctest --test-dir build-debug --output-on-failure -j 4
ctest --test-dir build-debug -R 'chat.logging|logging|configuration|adc.ncdc|adcs.ncdc' --output-on-failure
```
