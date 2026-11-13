# ADC RES — alpha-0.5.8

**Current increment — alpha-0.6.3:** [ADC RCM](ADC_RCM.md) adds owning D/E reverse-request validation and live target/echo routing through Transfer Core, with per-family passive-source/active-target checks, stable lifetime/address admission and shared bounded queues. CTM/RCM share wire-field validation. Real ncdc covers RCM→CTM and peer handshakes in both roles; no pending-token registry or file-data handling is claimed.

**Increment delivered in alpha-0.6.2:** [ADC CTM](ADC_CTM.md) adds owning D/E CTM validation and live target/echo routing over Transfer Core, with checked source TCP activity, lifetime/endpoint rechecks, shared bounded queues and real ncdc client-handshake interoperability. Protocol/port/token/extensions retain their checked spelling and privacy. RCM and file-data handling remain later stages.

**Increment delivered in alpha-0.6.1:** [Transfer Core](TRANSFER_CORE.md) adds an independent C++20 connection-coordination engine: bounded owning intents, active/passive IPv4/IPv6 roles and immutable session-bound one-attempt plans with cancellation and dispatch rechecks. Shared Users address classification preserves Search behavior. Live CTM/RCM routing and file data transfer remain future work.

**Increment delivered in alpha-0.5.13:** [WebAdmin Search Monitoring](WEBADMIN_SEARCH.md) adds browser-local query/result/refusal/queue rates, a bounded traffic chart and accessible recent-interval table. First samples, gaps, detected resets and failures start fresh baselines; pause/manual refresh and local clear preserve cumulative counters. Existing API/settings and credential isolation remain.

**Increment delivered in alpha-0.5.12:** [Search Configuration and Statistics](SEARCH_CONFIGURATION.md) exposes 19 cumulative hub SCH/RES counters, a lazy stored-token-window gauge and all 16 effective startup search settings through the authenticated read-only API and bilingual WebAdmin Search page. Queue acceptance is distinct from delivery; direct client UDP is excluded. No payload/history or configuration write endpoint is added.

**Increment delivered in alpha-0.5.11:** [Search Security](SEARCH_SECURITY.md) adds connection-lifetime query/sent-result/received-result count and byte quotas, bounded denial notices, guest/global permission policy, trusted session grants and configurable active-search limits. Admission precedes routing/token publication. Optional strict result-token policy supplements existing owner/generation checks; direct client UDP remains outside hub enforcement.

**Increment delivered in alpha-0.5.10:** [Active/Passive](ACTIVE_PASSIVE.md) projects checked I4/I6/U4/U6 into public Users and adds allocation-free per-family TCP/UDP reply-path metadata to Search Core and immutable plans. Passive DRES remains hub-routed; active URES travels directly between clients. Dynamic INF changes, endpoint ownership and real ncdc UDP sending/receiving are tested.

**Increment delivered in alpha-0.5.9:** [Result Routing](RESULT_ROUTING.md) forwards checked DRES only to its ready target over ADC/ADCS. Plans bind session lifetimes and, when TO is present, the target owner’s active search generation. Tokenless results remain supported. Shared bounded queues preserve original fields without public fanout, sender echo or administration payloads.

Local verification on 2026-11-13: **108/108 CTest suites passed in each of Debug, Release and ASan/UBSan**, with required integration tools and default sanitizer leak detection. This includes real ncdc ADC/ADCS, MariaDB and Apache HTTPS. No compiler warnings. WebAdmin production build/typecheck, 4 Node tests, 6 Chromium tests and the real C++ API → adapter → React integration passed. Release metadata, UTF-8, documentation language parity/local links, modified Python syntax, shipped INI and whitespace checks passed. These are local results; remote CI, the full supported OS/client matrix and production load remain unverified.

Based directly on main `f5c80aa0142b4ae6aa751ccde9a9b9e95dad1a9b` (alpha-0.5.7 already merged). Existing branches alpha-0.5.1–alpha-0.5.7 and their history are preserved. This increment receives and validates results; result forwarding is deliberately a separate future increment.

## Parser and ownership

`adc/res.hpp` exposes `parse_res(body)` returning `variant<ResRequest, ParseError, ResError>` and `validate_res(message)` returning `variant<ResRequest, ResError>`. The body excludes LF. Generic frame/escaping/UTF-8/NFC/header failures preserve ParseError. An immutable ResRequest owns both its checked original Message and a `search::ItemData`, optional available-slot count and optional decoded opaque token. Copies/moves retain ownership. Original field order, unknown fields (including repeats), numeric spellings and token bytes survive ordinary ADC serialization. No separate wire serializer is added.

The initial hub profile supports **DRES only**, with valid source/target SIDs, no feature selectors and zero positional parameters. The generic ADC parser still recognizes other message types. B/F/E results receive a recoverable semantic refusal in NORMAL; C/U/H/I are outside the existing hub session profile. This is a bounded application profile, not a claim of complete ADC result support. Validation checks hostile hand-built Messages via bounded shared serialization/reparse, including unknown fields.

## Fields and application limits

| Field | Validation |
| --- | --- |
| FN | Required full virtual share name; nonempty NFC printable text, at most 4096 decoded UTF-8 bytes. A trailing `/` denotes a directory. Rooted and relative names are accepted; root alone, empty components and `.`/`..` components are refused. Names are metadata and never opened as OS paths. |
| SI | Required signed 64-bit wire integer with nonnegative value, including zero; overflow/float/leading plus fail. Directory sizes remain explicit metadata. |
| SL | Optional nonnegative signed 64-bit available-slot count; absent differs from zero. |
| TO | Optional for compatibility with tokenless SCH; if supplied, nonempty NFC printable opaque text, at most 128 decoded UTF-8 bytes, unchanged. An empty supplied token fails. |
| TR | Required canonical 39-symbol uppercase unpadded Tiger Base32 root for files; all 192 bits retained. Invalid padding bits fail. Directories have no file root and reject TR in this initial profile. |

All five known fields are singular; duplicates fail even when identical. Unknown well-formed fields are preserved without interpretation. The total decoded parameter budget is 8192 bytes, including two-byte names and unknown values, checked before copies/Unicode work in the local-object API. The shared 128-parameter and 16384-byte wire-body limits remain. The parser does not check the existence, contents or hash of any advertised file, the target's presence, token activity, responder eligibility or filter matching.

Field meanings follow [ADC RES / SCH](https://adc.sourceforge.io/ADC.html) and [TIGR](https://adc.sourceforge.io/ADC-EXT.html). TO remains optional to match this server's existing tokenless SCH support; the specification requires copying TO when present in the request. Path, directory, field and byte limits above are explicit local policies. Clients remain responsible for safely handling advertised names.

## Live reception and isolation

LoginHub invokes validation after existing connection-state, ready-lifetime and source-SID ownership checks on TCP and TLS. Wrong state/source retains fatal 244/240 precedence even when the result payload is semantically malformed. Semantic failures receive fixed `ISTA 100 Invalid\ssearch\sresult FCDRES`; a valid result receives `ISTA 125 Search\sresult\srouting\snot\simplemented FCDRES`. The source can continue using the connection. Other supported session scopes identify their actual offending FOURCC.

Neither response acknowledges delivery or correlation. No result is forwarded, published, retained in a registry/database, exported via the API or added to chat counters. Names, tokens and roots do not enter logs or status responses. No output is queued to the target. Allocation failure during receive closes only the source with the existing internal-error cause; other sessions and their queues remain usable. Existing transport frame/rate/output caps apply; no result-specific flood policy is claimed.

## Verification and remaining work

`adc.res` tests typed values, file/directory profiles, optional fields, original-wire round trips, Unicode/escaping, malformed fields and canonical hashes, boundaries, hostile objects, copies/moves, allocation rollback and receive-failure isolation. `adc.res.process` uses fragmented real ADC/ADCS frames, recoverable errors followed by fences, no target delivery, source/state precedence and API/log/chat privacy. Existing real `adc.feature.ncdc` / `adcs.feature.ncdc` now require the new validated-result response after ncdc matches an indexed file and emits DRES; this distinguishes valid client output from a generic unsupported-command response. Existing SCH, TTH, token and feature tests remain enabled.

Result correlation/routing, unsolicited-result policy, recipient eligibility, per-search result limits, deduplication/history, UDP/SUDP, transfer coordination and administration are future stages. There is no new INI/CLI setting, operator catalog entry, dependency, API endpoint, database migration or WebAdmin search page. EN/US remains the implementation language; documentation is mirrored in `doc/us` and `doc/pl` and runtime catalogs remain in `languages/us` and `languages/pl`.
