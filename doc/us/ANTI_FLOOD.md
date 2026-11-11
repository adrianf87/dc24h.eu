# Anti-Flood — alpha-0.4.9

**Current increment — alpha-0.4.10:** [Chat Configuration](CHAT_CONFIGURATION.md) groups startup chat settings and adds decoded-text limits, independent public/private/action switches and optional/custom welcome. Defaults preserve previous behavior; recoverable policy refusals retain protocol validation and Anti-Flood.

Based directly on main `162ba385ce9b03a870fe44846943727537c7a35c`, after the Chat Validation merge. Code and protocol descriptions use EN/US UTF-8; operator messages/help and documentation also support Polish. Earlier branches and history are preserved.

## Policy and scope

Every actual Network connection receives its own `chat::FloodGuard`, configured through validated Network options and retained by the matching LoginHub peer. All BMSG, DMSG and EMSG share that source connection's two token buckets: message count and decoded UTF-8 text bytes. Changing recipient, PM/ME, public/private scope or nickname does not provide another budget. An EMSG counts once, including its source copy; broadcasts count incoming text once, not once per recipient.

Buckets start full, refill continuously with steady time and never exceed their burst capacities. Both budgets must permit a message; successful admission debits both together. A denied attempt consumes neither bucket, but does not reset refill time or the notification window. A frame larger than the byte burst cannot pass. The minimum configured byte burst accommodates any legal decoded MSG body.

| INI `[chat]` key | Default | Valid range |
| --- | --- | --- |
| `max_messages_per_second` | 5 | 1–1000000 |
| `message_burst` | 10 | 1–1000000 |
| `max_bytes_per_second` | 32768 | 1–16777216 |
| `byte_burst` | 131072 | 16384–16777216 |

For example, a fresh connection can send ten small messages immediately, then earn five message credits per second, subject also to byte credits. Rates and bursts are independent: a burst may be smaller than its per-second rate. Byte cost excludes ADC headers, flags, separators and escape expansion; the complete encoded frame limit and generic security budgets still cover those bytes. A space costs one decoded byte even though `\s` takes two wire bytes.

The guard retains fixed-size numeric/time state only. There is no message history, duplicate-text fingerprint, unbounded map or payload logging. Identical messages are limited by rate just like different messages. This is rate-based spam control, not content moderation, account sanctions or a ban engine.

## Placement and protocol behavior

The existing generic frame quota, malformed-input limits, deadlines and connection state machine remain first. Direction, NORMAL/readiness, source binding and known MSG-field validation occur before Anti-Flood. Invalid source or PM/ME syntax therefore still receives the existing fatal diagnostic even when a bucket is empty. Generic malformed frames remain discarded under existing security limits.

For a valid, ready B/D/E MSG, LoginHub checks the guard before routing preparation, recipient snapshots or private echo. This includes valid B/D/E attempts that later fail private/group/target policy, so repeated unavailable-target or PM-policy requests cannot bypass the chat budget. A denial publishes no chat frame to any recipient, including the source of EMSG. It does not create a backlog or retry job.

The first denial queues a fixed `ISTA 100 Chat\smessage\srate\slimit\sexceeded FC<FOURCC>` to the source. Further denials produce at most one such notice per second across B/D/E; a successful message does not reset that window. Code 100 uses recoverable severity and the generic error category from [ADC STA](https://adc.sourceforge.io/ADC.html). The description contains no supplied message, SID, nickname or address. The notice itself uses existing bounded output queues; allocation/cap failures retain normal source cleanup.

Rate denial alone leaves the connection NORMAL and ready. After credit refills, later messages can pass. Denied messages are dropped rather than automatically resent. Existing generic input-rate and output limits can still close an abusive/slow connection. Per-recipient queue refusal and pinned TCP/TLS output remain unchanged.

IMSG welcomes/trusted hub messages, INF presence changes, keepalives, STA and unsupported H/F scopes are outside these client B/D/E buckets. Their existing state, generic frame, byte/deadline and output limits remain applicable. There is no new unmetered public/private chat path.

## Ownership, clocks and configuration

`chat/anti_flood.hpp` and `src/chat/anti_flood.cpp` provide `FloodLimits`, validation and `FloodGuard::admit(decoded_bytes, now)`. Credit uses integer nano-tokens, preserving fractional refill without floating-point drift. Refill multiplication is bounded by missing capacity; oversized byte counts cannot overflow the charge. Extreme test clocks saturate refill, and backwards time is clamped without minting credit. Admission/notification checks allocate nothing and are confined to the reactor owner thread.

Network always passes its configured limits into `LoginLimits::anti_flood`; no zero/disable INI option or new CLI switch is added. The lower-level trusted LoginHub API permits an omitted policy, like other transport-owned admission policies. Direct Core/adapters and trusted in-process system APIs are not network-rate gates. Test fixtures explicitly using those APIs must supply a policy when testing Anti-Flood.

`LoginHub::receive` accepts an optional steady timestamp for deterministic guard tests; production defaults to current steady time. This timestamp controls the chat guard, not all model clocks/deadlines. No system clock is changed. Connection cleanup destroys its guard. Reconnect starts a fresh connection budget; there is no persistent per-account/CID/IP chat bucket. Existing shared-IP connection/admission/identity limits remain active, and clients behind one IP do not share this new message bucket.

The shipped INI documents the four defaults. Unknown/duplicate keys, zero/negative/overflow values and out-of-range merged limits fail startup/config validation. US/PL help explains the defaults and recovery behavior. Settings are startup-only; no reload or WebAdmin configuration control is introduced.

No external dependency, migration, API JSON field or endpoint is added. Existing `adc_frames` still counts parsed incoming frames, including rate-denied chat; it is not a delivery counter. No new flood counters or message bodies are exported. Aggregate output bounds continue to account for each actual recipient; incoming byte quotas are not a fanout bandwidth guarantee or production benchmark.

## Verification

`chat.anti_flood` covers exact fractional message/byte refill, burst boundaries, transactional debit, one-second notice suppression across successful messages, fixed-memory/no-allocation admission, overflow-size and min/max/backwards clocks, configuration bounds, combined B/D/E/ME charging, independent source budgets, recovery, trusted system output, fatal source-check precedence and charge cleanup.

`chat.anti_flood.process` uses actual ADC/ADCS and INI overrides to verify mixed public/private/action bursts, exact allowed recipient matrices, bounded diagnostic output, private drop/no echo, independent healthy clients, refill recovery, fatal spoof rejection, reconnect lifetime reset and log/API privacy. A separate byte test verifies decoded-space charging and recovery independent of message count. Unit/configuration tests check all four keys, ranges, duplicates and PL diagnostics. Real ncdc ADC/ADCS suites retain normal public/private/action/welcome/reconnect coverage under the actual enabled default policy.

Existing burst-oriented chat process suites set an explicitly high, valid `[chat]` budget to keep their ordering/codec/transport purpose independent; Anti-Flood is not disabled in production or silently skipped by tests.

```sh
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug -DDC24H_REQUIRE_INTEGRATION_TESTS=ON
cmake --build build-debug --parallel 4
ctest --test-dir build-debug --output-on-failure --parallel 3
ctest --test-dir build-debug -R '^(chat.anti_flood|configuration|adc.ncdc|adcs.ncdc)' --output-on-failure
```

See [CHANGELOG.md](../../CHANGELOG.md) for validation results. Remote CI, the full OS/client matrix, sustained hostile load and long-running fuzzing require separate evidence.

Local verification on 2026-11-11: **86/86 CTest suites passed in Debug, Release and ASan/UBSan**, with strict integration-tool detection and without disabling leak detection. This includes real ncdc ADC/ADCS under enabled defaults, Anti-Flood/configuration checks, chat/session/queue regressions, disposable MariaDB and Apache HTTPS. Builds emitted no compiler warnings. WebAdmin build/typecheck, 4 Node tests, 3 Chromium tests and real C++ API integration passed. The shipped INI, metadata, UTF-8/language parity, local links, modified Python syntax, US/PL help and Git whitespace checks passed. Remote CI, the full OS/client matrix, sustained hostile load and long-running fuzzing remain unverified.
