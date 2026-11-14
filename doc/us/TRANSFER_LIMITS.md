# Transfer Limits — alpha-0.6.10

**alpha-0.6.10 local validation (2026-11-14):** All 169/169 CTest tests passed in each of Debug, Release and ASan/UBSan with required integration tools, including the new native/process limits suites, two real ncdc flood/refill suites, previous plaintext/TLS peer handshakes, MariaDB and Apache HTTPS. No compiler warnings. WebAdmin build/typecheck, 12 Node tests, 10 Chromium tests and real C++ API integration passed. Release metadata, UTF-8/language parity/local links, changed Python syntax, example INI startup/shutdown and whitespace checks passed. GitHub Actions completion is not awaited; the full supported OS matrix and production load remain unverified.

Based directly on main `76f3710905367d76ddd6791643446d14f152e6ec`. Earlier alpha-0.6.1–alpha-0.6.9 branches and their merged implementation are preserved. This increment limits hub-mediated connection requests; peer file data continues to flow directly between clients.

## Budgets and configuration

`transfers/limits.hpp` defines validated `Limits`, `FloodLimits`, `Direction` and `TransferGuard`. Each LoginHub connection owns one guard with independent outgoing and incoming count/byte buckets. Guards use fixed memory and retain no tokens, SIDs, peer addresses, message text or request history. Admission and notice checks allocate nothing and use an injected steady-clock timestamp.

All settings are under `[transfers]`:

| INI key | Default | Allowed range |
| --- | ---: | --- |
| `request_rate` | 32 | 1..1000000 requests/second |
| `request_burst` | 64 | 1..1000000 requests |
| `request_byte_rate` | 131072 | 1..16777216 decoded bytes/second |
| `request_byte_burst` | 262144 | 16384..16777216 decoded bytes |
| `incoming_rate` | 128 | 1..1000000 requests/second |
| `incoming_burst` | 256 | 1..1000000 requests |
| `incoming_byte_rate` | 524288 | 1..16777216 decoded bytes/second |
| `incoming_byte_burst` | 1048576 | 16384..16777216 decoded bytes |

Missing keys use defaults. Zero, signed, fractional, malformed, overflowing, duplicate and out-of-range values fail startup. Trusted Network/LoginHub options and direct guard construction validate the same bounds. Restart to change settings. There is no off switch or permission bypass for these quotas.

Buckets begin full. Fractional nanosecond refill saturates at capacity; idle time cannot accumulate an unlimited burst. A backwards timestamp cannot create credit, and extreme min/max timestamps do not overflow. Count and byte credits debit atomically within each bucket: a denied request spends neither. Byte cost includes the decoded four-character header, source/target, all parameters and separators; the final LF is excluded. Escaped wire bytes remain covered by existing transport frame/byte quotas. The 16384 minimum byte burst does not increase the existing CTM/RCM field limit.

## Admission order and delivery

1. Existing session/state/source checks run first. Exhausted credits do not suppress terminal source forgery or invalid-state rejection.
2. A CTM/RCM attempt spends its source's outgoing credit before Connection Router prepares a plan, checks capabilities/permissions or emits a recoverable routing diagnostic. Invalid recipients, incompatible roles and policy denials cannot generate unlimited replies. A successful debit is not refunded on a later refusal or allocation/queue failure.
3. Only a successfully prepared request can spend the exact target lifetime's incoming credit. All senders targeting that lifetime share this budget, preventing independent source budgets from bypassing target protection. No target state is created for nonexistent SIDs. Target refusal cancels the plan before any target delivery or E echo.
4. Existing one-attempt dispatch, readiness/endpoint/permission rechecks and bounded shared queues apply. An admitted E request costs one source and one target attempt; its source echo is not charged as another incoming request. Queue acceptance does not prove that clients connected.

CTM/RCM, D/E scope, IPv4/IPv6, protocol names, tokens and recipient changes share each connection's outgoing budget. Incoming and outgoing budgets are independent, so receiving requests does not spend the ability to respond with CTM. INF/SUP changes and trusted permission updates preserve credits. Disconnect destroys the guard; a new lifetime begins with fresh budgets. Existing per-IP accept/connection and general transport limits remain responsible for reconnect churn.

Both source and target quota refusals drop the original request without target delivery or echo. The source receives at most one recoverable `ISTA 100 Connection\srequest\srate\slimit\sexceeded FC<FOURCC>` per second across all transfer quota denials; subsequent denials are silent. Successful routing does not reset the notice window. The static diagnostic discloses no token, protocol, endpoint, SID or target budget. Peer STA remains available to report connection failures and is governed by existing transport/output limits. The [ADC specification](https://adc.sourceforge.io/ADC.html), sections 3.3 and 5.3.1, defines D/E routing and recoverable generic STA; the quotas and notice interval are local policy.

Allocation failure while building a source notice uses the existing source-local close boundary. Target queue allocation failure remains target-local; previously admitted E echo semantics are unchanged. No denied request is queued for later retry. Existing queued frames are not recalled or debited again when partially written.

## Verification and scope

`transfers.limits` checks count/byte atomicity, fractional refill, extreme/backwards clocks, invalid limits/direction, allocation-free guard operations, command/scope/protocol sharing, permission and mode changes, target aggregation, independent directions, source forgery while exhausted and disconnect cleanup.

`transfers.limits.process` covers fragmented mixed ADC/ADCS connections on IPv4 and IPv6, private bounded denial, refill recovery, target aggregation, full decoded extension-byte costs, invalid/duplicate INI keys and API/log privacy. `adc.transferlimits.ncdc` and `adcs.transferlimits.ncdc` flood a real ncdc client with unsupported-protocol offers: exactly one admitted offer produces its peer 141 response, the remaining requests yield one private hub notice, refill allows another peer response, and both clients retain chat/quit behavior. Earlier positive plaintext/TLS CTM/RCM peer handshakes remain in the full regression suite.

This stage adds no dependency, migration, account policy, pending-token registry, HTTP endpoint, transfer statistics or WebAdmin control. The authenticated API still exposes its previous schema; new transfer quotas are configured through INI. Trusted standalone Core/ConnectionRouter consumers remain responsible for their own admission policy; LoginHub applies the live transport quotas. Peer data bandwidth, concurrent transfers, slots, transfer deadlines, persistent penalties and a full supported-OS/load matrix are future work.

See [Configuration](CONFIGURATION.md), [Connection Security](CONNECTION_SECURITY.md), [Error Handling](ERROR_HANDLING.md), [Transfer Core](TRANSFER_CORE.md), [Testing](TESTING.md) and the [example INI](../../config/dc24h.ini.example).
