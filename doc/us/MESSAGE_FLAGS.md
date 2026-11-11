# Message Flags — alpha-0.4.6

**Current increment — alpha-0.4.9:** [Anti-Flood](ANTI_FLOOD.md) adds enabled per-connection B/D/E message and decoded-byte buckets, configurable in `[chat]`. Excess messages are dropped before fanout with bounded recoverable notices; existing validation and general resource limits remain.

**Increment delivered in alpha-0.4.8:** [Chat Validation](CHAT_VALIDATION.md) centralizes client text, SID and sender readiness/ownership checks in both adapters. Recipient lifetime selection and dispatch revalidation remain authoritative; system IMSG stays separate.

**Increment delivered in alpha-0.4.7:** [Hub Messages](HUB_MESSAGES.md) adds trusted, lifetime-bound system IMSG delivery and a fixed welcome after own BINF. Client text cannot originate system messages; existing queue bounds and final-LF readiness remain.

Based directly on main `1e83114eb613dc59b10c6956025f14e5467f24a8`, after the alpha-0.4.5 Message Routing merge. Earlier branches and history are preserved. The server uses EN/US UTF-8 with Polish operator messages and mirrored documentation.

## PM and ME contract

This increment extracts the PM/ME support previously introduced by the MSG codec and chat adapters into `adc/msg_flags.hpp` and `src/adc/msg_flags.cpp`. The codec, local builder and both public/private projections now share flag decoding and canonical parameter construction. Existing accepted messages and routing policy are retained.

The flag meanings follow [ADC 1.0.4, MSG](https://adc.sourceforge.io/ADC.html): PM supplies the conversation's reply SID; a normal private conversation uses the originating SID. ME1 requests third-person presentation. Flags do not supply sender authentication or recipient membership.

| Input | Typed value / routed behavior |
| --- | --- |
| No PM | No private conversation metadata is fabricated |
| PM plus a valid four-character base32 SID | `private_group` owns a checked SID value |
| No ME | `third_person` is absent |
| ME0 | Explicit `third_person=false`; output retains ME0 |
| ME1 | `third_person=true`; output retains ME1 |
| ME-0, ME000 | Valid integer zero; routed output is ME0 |
| ME01, ME0001 | Valid integer one; routed output is ME1 |
| Bare PM/ME, invalid SID, other integer values/overflow | Semantic error; live fatal ISTA 240 and own IQUI |
| Duplicate PM or ME, even identical or ME0 | Semantic error; no partial publication |

Known named fields may appear in either order with unknown fields interleaved. Only tokens after the single positional text are flags. Text such as `PMBBBB ME1`, including escaped spaces/newlines, remains ordinary text. Each parse creates new values; flags are not inherited from earlier messages.

The existing integer profile accepts decimal signed 64-bit spellings whose value is zero or one, including leading zeroes and negative zero. Leading plus, whitespace, empty values, non-numbers, other numbers and overflow are rejected. The raw codec preserves valid input spellings, extension values, duplicates of unknown fields and field order. Local builders and routed projections emit only known flags, in PM-then-ME order with canonical ME0/ME1. Unknown metadata is not relayed.

## Shared API and validation boundary

- `MsgFlags` owns `optional<Sid> private_group` and `optional<bool> third_person`. No string view, registry reference or transport resource is retained.
- `parse_msg_flags(span<const string>) noexcept` reads decoded named tokens excluding positional text. It checks token shape, PM/ME values and duplicates without allocation, bounds the field count to 127 and rejects an individual token over 16384 bytes. Unknown values are ignored by this helper.
- `make_msg_parameters(text, flags)` returns a new owning vector containing the text and canonical optional flags. It validates nonempty bounded UTF-8/NFC chat text, permits decoded LF and rejects other ASCII controls/DEL. Invalid local text throws `invalid_argument`; allocation errors propagate. Caller-owned text and flags are unchanged.
- `MsgView::flags()` returns an owning flag value while the existing `MsgView` text still borrows the message. Existing MsgView/MsgOptions fields and codec entry points remain available.

The flags helper alone does not validate unknown values, headers, total encoded length or permissions. Full `parse_msg`/`validate_msg`/`serialize_msg` retain those codec checks, including UTF-8/NFC for unknown fields, 128 total parameters and the 16384-byte encoded body plus final LF. PM/ME bytes, spaces and escape expansion count toward that limit. A component vector is not a validated wire frame.

## Routing and privacy

`MessageRouter` continues to bind the ready source to its connection and to its Chat Core domain. BMSG without PM is public, including optional ME0/ME1. Any PM-marked BMSG is refused with recoverable ISTA 125 and never becomes public content. DMSG selects the target; EMSG selects target then sender, once for self-messages. A present PM must equal the source for a normal private conversation; another valid group SID remains unsupported with ISTA 125. PM is not used as a second recipient lookup. ME changes presentation metadata only, never recipients or source identity.

For example, accepted input `EMSG AAAB AAAC hello ME01 XXopaque PMAAAB` is routed as `EMSG AAAB AAAC hello PMAAAB ME1` followed by LF. A subsequent unflagged message remains unflagged. No PM or ME is invented for it.

Malformed syntax is still discarded by the generic parser under existing quotas; known-field errors are fatal, unsupported groups/scopes remain recoverable, and missing targets retain ISTA 140. State/source precedence and queue lifetimes, FIFO, bounds, shared immutable packets and isolated recipient failures are unchanged. Message text and unknown metadata are absent from logs, database writes and administrative API/export paths.

No new configuration, dependency, SQL migration, API JSON field or WebAdmin feature is added. Existing US/PL help now explains PM/ME. Group chat, moderation/history, offline messages and registered-account permissions remain separate work.

## Verification

`adc.msg.flags` covers absent/false/true values, owned SIDs, allocation-free decoding, duplicate/error classification, field bounds, UTF-8/NFC rejection by the full codec, positional flag-like text, opaque extensions, complete encoded limits and the B/D/E PM/ME recipient/projection matrix. Existing allocation-failure, domain/lifetime, backpressure and shared-frame tests exercise the refactored paths.

`adc.msg.flags.process` verifies both source transports, interleaved fields and canonical output, public/private action/plain combinations, no flag carry-over, exact recipients with ordered public fences, group/public refusal, source-only fatal cleanup and log/API privacy. The real `adc.ncdc` and `adcs.ncdc` tests additionally receive explicit ME0/ME1 private messages and send a private action reply through the PM conversation tab. Existing public/action/private/reconnect and plaintext codec round-trip checks remain.

Reproduce with strict integration-tool detection:

```sh
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug -DDC24H_REQUIRE_INTEGRATION_TESTS=ON
cmake --build build-debug --parallel 4
ctest --test-dir build-debug --output-on-failure --parallel 3
ctest --test-dir build-debug -R '^(adc.msg.flags|adc.ncdc|adcs.ncdc)' --output-on-failure
npm --prefix webadmin run build
npm --prefix webadmin test
PLAYWRIGHT_CHROMIUM_EXECUTABLE=/usr/bin/chromium npm --prefix webadmin run test:e2e
PLAYWRIGHT_CHROMIUM_EXECUTABLE=/usr/bin/chromium npm --prefix webadmin run test:integration -- ../build-debug/dc24h
```

Current validation results are recorded in [CHANGELOG.md](../../CHANGELOG.md). Remote CI, the full OS/client matrix and production-load validation require separate evidence.

Local validation on 2026-11-11: **80/80 CTest suites passed in Debug, Release and ASan/UBSan**, including real ncdc ADC/ADCS. Leak detection was not disabled; builds emitted no compiler warnings. WebAdmin production build/typecheck, 4 Node tests, 3 Chromium tests and live C++ API integration passed. Metadata, UTF-8/language parity, local links, Python syntax, US/PL help and Git whitespace checks passed. These results are local and do not establish remote CI, full OS/client coverage or production-load behavior.
