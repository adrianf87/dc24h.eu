# Transfer Configuration — alpha-0.6.13

**Local alpha-0.6.13 validation (2026-11-14):** Full local runs passed **179/179 CTest suites in each of Debug and Release** on Debian 13 with required integrations, including four new real ncdc configuration/peer-handshake suites and all prior ADC/ADCS, MariaDB and Apache checks. Both builds completed without compiler warnings. WebAdmin production build/typecheck, **23 Node tests**, **14 Chromium tests** and real C++ API/React custom-configuration and traffic integration passed. Mobile PL rendering, shipped example INI, changed Python syntax, release metadata, UTF-8/language parity/local links and whitespace checks passed. The Ubuntu-only GitHub workflow is unchanged. Other supported OS versions, sanitizer runs for this increment and production load remain unverified; remote Check status is not awaited.

Based directly on main `cc48fbd90d8f023bf1295f726a34ce9e7e158426` after the alpha-0.6.12 merge. This stage completes startup configuration of connection-coordination limits and permitted peer protocols. It preserves the Ubuntu-only GitHub workflow and all earlier stages.

## Startup policy

The existing `[transfers]` section now also accepts `allowed_protocols`. All settings are read at startup; editing the INI does not change an existing hub. Use `dc24h --config /absolute/path/hub.ini --check-config` to validate without starting listeners or reading database passwords, then restart deliberately to apply changes. There is no reload command, browser editor or configuration write API.

```ini
[transfers]
allowed_protocols = ADC/1.0,ADCS/1.0
ctm_enabled = on
rcm_enabled = on
guest_initiate = on
guest_receive = on
request_rate = 32
request_burst = 64
request_byte_rate = 131072
request_byte_burst = 262144
incoming_rate = 128
incoming_burst = 256
incoming_byte_rate = 524288
incoming_byte_burst = 1048576
```

| Setting | Default | Meaning / accepted range |
| --- | --- | --- |
| `allowed_protocols` | `any` | `any`, `none`, or 1–16 exact comma-separated names |
| `ctm_enabled`, `rcm_enabled` | `on` | Independent command switches, strict `on` / `off` |
| `guest_initiate`, `guest_receive` | `on` | Independent source initiation / target receipt permission |
| `request_rate`, `request_burst` | 32, 64 | Source attempts per second / count capacity; 1–1000000 |
| `request_byte_rate`, `request_byte_burst` | 131072, 262144 | Source decoded bytes per second / byte capacity |
| `incoming_rate`, `incoming_burst` | 128, 256 | Aggregate target attempts per second / count capacity; 1–1000000 |
| `incoming_byte_rate`, `incoming_byte_burst` | 524288, 1048576 | Aggregate target decoded bytes per second / byte capacity |

Byte rates accept 1–16777216; byte bursts accept 16384–16777216. Rates and bursts are independent; a burst smaller than its rate is allowed. Budgets debit count and decoded bytes together, are shared by CTM/RCM and D/E within each connection lifetime, refill monotonically and are not reset by protocol/permission/INF changes. An E echo does not consume a second incoming request. These are coordination-message quotas, not file bandwidth or concurrent-transfer limits. Existing notice throttling and [Transfer Limits](TRANSFER_LIMITS.md) behavior remain.

`any` preserves previous extension-compatible behavior and does not bypass request validation, permissions or quotas. `none` denies every otherwise eligible CTM/RCM request. An explicit list uses exact case-sensitive bytes, keeps configured order and trims only ASCII space/tab around entries. Each name follows the existing CTM/RCM boundary: 1–32 visible ASCII bytes. Commas separate names, and lower-case `any` / `none` are reserved whole-value keywords; they cannot be mixed with entries. Empty entries, duplicates, overlong lists/names and invalid text fail startup. There is no wildcard expansion: `*` is a literal extension name. For example, `ADC/1.0` differs from `adc/1.0` and `ADC/1.00`.

Use `allowed_protocols = ADCS/1.0` to permit only the peer TLS protocol name. Hub ADC/ADCS transport, peer SU ADCS and active/passive mode remain separate. The hub preserves explicit protocol/port/token fields and never changes an ADCS request to ADC. Allowing a protocol is not proof that the peer supports it or completes a TLS handshake. Tokens remain opaque; no pending transfer registry or file handling is added.

## Enforcement and status handling

`transfers::ProtocolPolicy` owns a bounded list behind a checked factory and offers allocation-free exact lookups. Transfer Core owns a startup copy and checks it during prepare and dispatch; standalone native calls and both live CTM/RCM adapters use the same gate. Mutating the caller's original settings cannot change an existing Core. Source ownership/state, request syntax, ready target, endpoint/capability and operation/permission error precedence are retained. A trusted per-session grant cannot bypass protocol policy.

A protocol-policy denial sends only a recoverable `ISTA 125 Connection\sprotocol\snot\spermitted FC<command>` to the source, with no target delivery or E echo and no protocol/token content in the diagnostic. It is counted as an existing rejected request, not a peer unsupported-protocol report. Source quotas still bound rejected attempts; denied policy does not debit a target's incoming bucket. A subsequent permitted request and chat remain usable. Already queued frames are not recalled.

The [ADC specification](https://dc-protocols.github.io/ADC.html) defines access denial with error 25/FC, while errors 41/42 are peer reports about protocol support or direct connection failure. Such peer STA reports keep their existing validation/routing even when their PR value is excluded by startup policy; the hub does not interpret them as a new connection request or authorize a fallback.

## Effective settings in API and WebAdmin

Authenticated `GET /api/v1/status` adds `transfers.configuration` alongside the unchanged 22 `transfers.statistics` counters. It contains the four boolean switches, eight integer quota fields and `allowed_protocols`: string `"any"`, an empty array for `none`, or an ordered array of exact names. Quotes/backslashes in permitted visible-ASCII extension names are JSON-escaped. Owning effective settings are captured before Network options move and then copied by the API worker, avoiding moved-from strings or cross-thread mutable policy.

The **Connections** page displays all 13 effective settings in EN/US or Polish with a restart/read-only explanation. Older APIs with statistics but without configuration retain their statistics and show unavailable settings. A malformed present configuration rejects the snapshot rather than inventing defaults. The browser adapter checks booleans, each quota's bounds and exact bounded protocol arrays, strips unknown fields, owns copied arrays and renders names as text. Snapshot export includes only allowlisted configuration; monitoring history still retains counters only. No identities, endpoints, paths, credentials or request tokens are added.

## Verification

`transfers.configuration` tests exact/default/empty/list policy, ownership, 16-name/32-byte boundaries, duplicate/invalid input, literal wildcard behavior, Core/router CTM/RCM D/E admission, unchanged wire names, allocation-free lookup/dispatch and INI validation. `transfers.configuration.process` exercises ADC/ADCS and IPv4/IPv6 with active/passive peers, private denial and bystander isolation, case/extension/quoted names, exact API settings, unchanged running policy after editing, restart application, counters, reports and privacy. Existing limit tests retain numeric boundary/flood coverage.

Four real ncdc `*.transferconfiguration*.ncdc` suites apply an explicit ADC-only or ADCS-only policy, reject unknown/opposite protocols privately, then complete actual CTM or RCM→CTM plaintext/certificate-checked TLS peer handshakes over both hub transports. Frontend tests cover bounds, older APIs, owning allowlists, safe exports, literal rendering and mobile Polish layout. The real C++ API/React harness verifies custom effective limits and long quoted/backslash protocol names alongside observed traffic. Final local results belong in [Changelog](../../CHANGELOG.md); remote Check status is not awaited or reported as passed.
