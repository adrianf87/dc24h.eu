# Limits — alpha-0.3.12

This increment starts directly from main `4e54b2e0c18f39542cb550d07d4a527ca11a1e10` after the alpha-0.3.11 User Disconnect merge. Earlier stages/history are retained; alpha-0.3.12 is not merged automatically.

## Settings and effective capacity

The server has one connection lifetime, one Session model per admitted transport and at most one public guest User per Session. Limits are shared by all ADC and ADCS listeners, with no transport-specific pool or registered-account exemption.

| Setting | Default | Range | Ownership/counting |
| --- | --- | --- | --- |
| `[network] max_connections`, `--max-connections N` | 1024 | 1..65535 | Existing Network connection objects, including pending TLS |
| `[users] max_sessions`, `--max-sessions N` | 65535 | 1..65535 | All SessionManager models until removal |
| `[users] max_users`, `--max-users N` | 65535 | 1..65535 | Public registrations from identification until removal |

Effective sessions = min(max_connections, max_sessions). Effective users = min(effective sessions, max_users). Higher ceilings are valid but cannot increase a lower cap. Thus existing configurations retain their old behavior: the default effective session and user caps are both 1024. Zero is invalid, not an unlimited sentinel. No capacity is eagerly preallocated at the ceiling.

For example, max_connections=100, max_sessions=60 and max_users=40 allow at most 60 retained connections/models and 40 public registrations. The remaining session slots can negotiate, but successful identity validation alone cannot bypass the user cap. max_connections=2 with both new values at 65535 remains limited to two sessions/users.

Configuration retains defaults → explicit INI → CLI precedence. Scalar ranges are validated before clamping, so an invalid high value cannot hide behind another smaller cap. Programmatic Network, LoginHub and SessionManager limits receive equivalent validation. The bilingual `--help`, INI example and startup effective-cap fields describe these controls. There is no dynamic reload or runtime limit change.

## Admission and release

Network checks its connection cap and SessionManager's effective session cap after accept, before per-IP admission, a transport token, TLS session, watch, protocol peer or Session allocation. Excess sockets are closed immediately without application output or a connection lifecycle record. This works equally for plaintext and incomplete TLS. The kernel TCP connection may already have succeeded. Rejections increment the existing generic rejected count and use rate-limited warnings; they do not count as security-quota rejections or successful per-IP admissions.

`users::Limits::bounded_by` computes checked effective ceilings. SessionManager's size/capacity and identified_users/user_capacity are the authoritative counts. There is no extra reservation counter that can drift from the owners. `SessionManager::add` returns the existing capacity_reached error before allocating a model or advancing its ID watermark. The legacy SessionManager(capacity) constructor retains equal session/user caps.

`SessionManager::identify` validates state/time/identity and existing SID/CID/nickname collision rules, then returns the new typed RegistryError::user_capacity_reached before allocating indexes or mutating the model when full. UserRegistry and NickManager are themselves bounded by the effective user cap. Index insertion remains transactional; allocation failure rolls back reservations, so retry can use the same available user slot. Existing registered users can update or rename at capacity without consuming a second slot.

LoginHub checks users_full after initial validated INF and duplicate CID handling, before guest approval and welcome/list planning. It sends fixed `ISTA 211 Hub\suser\slimit\sreached` followed by own IQUI and bounded drain/close. [ADC STA](https://adc.sourceforge.io/ADC.html#_sta) assigns error 11 to Hub full and severity 2 to fatal disconnect. A rejected user receives no hub/user-list welcome, is never registered or marked ready, and generates no public join/quit. The manager repeats the capacity gate at registration, protecting direct module callers as well. The single reactor thread makes the preflight and commit sequential without an admission race. Identity verification/protocol parsing may allocate before this preflight; it is not a new pre-Tiger rate limiter.

Session slots include accepted, negotiating, identified, ready, closing and finalized models until remove. User slots include identified users awaiting welcome, ready users, and closing/closed registrations until remove. Losing readiness, entering a terminal drain or merely finishing lifecycle does not free capacity. Historical readiness and pending-per-IP accounting remain independent. UserDisconnect releases protocol/SID, queued output and public indexes in its existing order; removing the model returns the corresponding capacity. Anonymous failure releases only its session slot. Rejected admission never acquires a user slot.

## Existing protections and limits

Per-IP active/pending caps, global pending login caps, admission/identity/frame rates, cumulative negotiation budgets, output byte caps and lifecycle deadlines continue to apply. These controls can reject a connection before the new user gate; increasing one ceiling never disables the others. Each current CID remains unique, and local SessionIds are never reused. There are no account-based multisession rules, persistent quotas, resource autoscaling or reserved operator slots in this increment.

Runtime EN/US remains the default. Both languages/us and languages/pl include the new session-limit warning, validation diagnostic and help. ADC status descriptions stay fixed EN/US and contain no client payload. No dependency, SQL migration, API JSON field or WebAdmin function is added. Existing version metadata/fixtures are updated; current API active/accepted counters remain usable for integration tests.

## Verification

`users.limits` checks checked bounds/defaults/clamping, programmatic rejection before clamping, legacy construction, session refusal without ID consumption, and user admission at identified/ready/closing/closed boundaries. It proves no-allocation user-capacity rejection without partial indexes, reservation retention until removal, rename/collision behavior at capacity, allocation rollback/retry with an unrelated user retained, and LoginHub refusal before any welcome/list with a stable unrelated pinned buffer. A later admission reuses released capacity and includes no rejected/old user.

`users.limits.process` uses real mixed ADC/ADCS sockets. It independently makes network and session caps the smaller ceiling, holds pending anonymous TLS, rejects excess sockets from both listeners without TLS admission, verifies API active/accepted/security counts and rate-limited warnings, then proves cleanup/reuse. The user-cap fixture repeatedly gets ISTA 211 → own IQUI without public join/quit, keeps existing users functional at capacity, closes an owner and admits the same CID/nickname again with a new SID. Every accepted lifetime receives exactly one terminal event; private rejected nicknames do not enter logs.

Configuration tests cover INI/CLI bounds, duplicate/missing/invalid values, precedence, direct Settings validation and US/PL help/warning text. Network tests reject invalid raw limits. Actual ncdc ADC/ADCS operates with max_sessions=3/max_users=2, fills the user cap with its observer, performs a live update and reconnects after cleanup. Earlier security/pending, timeout, list/update, disconnect, MariaDB, API and Apache regressions remain. C++ fault injection covers exercised registry allocations, not arbitrary OS or allocator failures. See [TESTING.md](TESTING.md) for local validation commands/results.
