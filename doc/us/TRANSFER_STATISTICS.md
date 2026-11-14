# Transfer Statistics — alpha-0.6.11

**Local alpha-0.6.11 validation (2026-11-14, Debian 13):** Final full runs passed 171/171 CTest suites in each of Debug, Release and ASan/UBSan with required integrations. An earlier parallel Debug ncdc mode-transition timeout passed three isolated rechecks and the final complete Debug run; the three-second deadline was retained. No compiler warnings. WebAdmin build/typecheck, 14 Node tests, 11 Chromium tests and live C++ API/React transfer telemetry passed. actionlint 1.7.12, bash syntax for workflow steps, C++/TypeScript counter-name parity, release metadata, UTF-8/language parity/local links, changed Python syntax and whitespace checks passed. Remote Actions/custom-runner completion is not awaited; other supported OS versions and production load remain unverified.

Based on main `122a682452cc02ee3589819beda5e70c3aca0914` after Transfer Limits. This increment adds aggregate connection-coordination counters through LoginHub, Network, the authenticated local status API and the existing WebAdmin Connections page. It does not observe peer sockets or file bytes.

## Counter contract

`transfers/statistics.hpp` defines a fixed 22-name allowlist, an owning `Statistics` snapshot and an allocation-free `Attempt` scope. LoginHub records one decoded client CTM/RCM attempt after the closing gate and before validation or allocations. Parser failures remain in network ADC-invalid counters; input ignored after closing adds no attempt. Standalone trusted Core/Router calls and locally generated output do not add client attempts.

For each prefix `ctm_` and `rcm_`, the following ten fields are exported:

| Suffix | Meaning |
| --- | --- |
| `received` | Decoded attempts, including state/source/wire rejection |
| `routed` | Valid dispatch completed, including recipient queue refusal/cancellation |
| `rejected` | Protocol, unavailable SID, capability or permission rejection |
| `rate_limited` | Source count/byte quota denial, including silent denials |
| `recipient_limited` | Aggregate target count/byte quota denial |
| `errors` | Source validation/preparation/diagnostic allocation or internal dispatch error |
| `queued` | Accepted immutable output copies; D can queue one and E two |
| `refused` | Recipient copies refused by queue limits or allocation failure |
| `canceled` | Captured copies canceled by dispatch rechecks |
| `queue_errors` | Recipient queue allocation failures; a subset of refused copies |

Every completed logical attempt has exactly one outcome: `received = routed + rejected + rate_limited + recipient_limited + errors`, until saturation. A failed allocation while constructing a quota/rejection notice replaces the prior outcome with `errors`. Recipient-local queue allocation errors remain part of a completed `routed` attempt and increment `queue_errors` plus `refused`; other recipients can still accept their copies. Queue acceptance does not prove transmission, peer connection, successful transfer or a completed file.

`peer_protocol_unsupported` counts accepted D/E peer STA reports with error 41; `peer_connection_failed` counts accepted D/E reports with error 42. A report is counted once after valid source/state/fields and target admission, before dispatch. Recoverable/fatal severity and E echo do not multiply it. Missing targets, malformed or forged reports, B/F/H STA, hub-generated diagnostics and socket EOF/reset do not increment these fields. Reports are unverified client claims; repeated opaque tokens are not correlated or deduplicated.

Counters saturate at `UINT64_MAX`, allocate no memory, retain no request content and are cumulative for one hub lifetime. INF/SUP, permission changes, output completion and client disconnect do not reset them. New hub startup resets them. The reactor publishes fixed atomic fields through the existing Network/API boundary, including final exit publication. Concurrent fields are independently sampled rather than a transaction; clients should not infer an instantaneous cross-field invariant from a racing API read.

## API and WebAdmin

The authenticated `GET /api/v1/status` response adds an optional-compatible v1 extension:

```json
{"transfers":{"statistics":{"ctm_received":0,"ctm_routed":0}}}
```

This abbreviated example omits the other twenty required statistics fields. The real server always returns all 22 names with unsigned integer values. Existing authentication, private Unix socket, HTTP framing, response limits and endpoint paths are unchanged. There are no user identifiers, SIDs, IP/port values, protocol/token strings, per-user records, paths or secrets in this extension. It does not include configuration or add an administrative write operation.

WebAdmin's Connections page displays CTM and RCM outcomes/copies and peer reports in EN/US or PL. Values refresh and export through the existing snapshot flow. `transfer-status.ts` validates all present counters as nonnegative safe integers and constructs an owning allowlisted object. Unknown fields are stripped. An older API without the extension remains supported and displays unavailable values instead of zeros. Malformed, missing or unsafe fields in a present extension invalidate the snapshot; outage/restart behavior follows the existing status lifecycle. The server's full uint64 range remains available to API consumers, while the browser rejects values outside JavaScript's exact integer range.

## Verification

`transfers.statistics` checks exact D/E CTM/RCM totals, source/capability/permission errors, silent/source/recipient quota denials, allocation-free ownership/saturation, one report per peer message, closing/disconnect/restart behavior and separation from Chat/Search. A 128-case one-shot allocation sweep reaches source errors, recipient queue errors and success while preserving an unrelated user and exactly one terminal outcome.

`transfers.statistics.process` checks exact API fields and values over mixed ADC/ADCS on IPv4/IPv6, fragmented input, repeated tokens, quota denials, peer 141/242, missing targets, cumulative disconnect state, restart reset and API/log privacy. Existing real ncdc transfer-core, CTM/RCM, capability/token/security/error/limit scenarios now also check aggregate request partitions; limit/error scenarios assert their corresponding denial and peer-report counters.

Frontend tests cover every counter, missing/invalid/unsafe data, owning allowlists, export privacy, older APIs and EN/PL mobile layout. Live C++ API integration checks that transfer statistics reach the browser adapter and UI. Remote CI and its Debian 13 custom-runner matrix are described in [Debian 13 CI](CI_DEBIAN13.md); completion is not awaited when publishing this branch.

See [Transfer Limits](TRANSFER_LIMITS.md), [Error Handling](ERROR_HANDLING.md), [API](API.md), [WebAdmin](WEBADMIN.md) and [Testing](TESTING.md). No new dependency, migration, configuration key, pending token registry, bandwidth/concurrency measurement or persistent telemetry is introduced.
