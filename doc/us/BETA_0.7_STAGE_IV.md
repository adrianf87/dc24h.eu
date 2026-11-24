# Beta 0.7.13–0.7.16 — Stage IV: private WebAdmin observability

Scope: [#238](https://github.com/adrianf87/dc24h.eu-high-starter/issues/238), the single `beta-0.7.x-stage-full` branch and draft PR #239. This is an **authenticated, loopback-only, read-only** diagnostic projection. No hub start/stop, grant mutation, privileged operator action or public listener is introduced.

## 0.7.13 — trustworthy freshness

- C++ status emits `observation.schema=1` and `observed_unix_ms`. Node rejects malformed, future and older-than-10-second observations using receive-time wall-clock comparison. Only fixed aggregate counters cross the Node read-only boundary; no private account or configuration data is projected.
- Receipt and display expiry use monotonic elapsed time. The browser removes metrics after **45 seconds total verified source age** (initial source age plus time since receipt), even if a refresh hangs. The gateway incident store also expires its verified status 45 seconds after its last valid observation.
- Older v1 hubs without `observation` remain readable only as unverified counters; their source freshness and maintenance/recovery state are explicitly `Unknown`. Never fabricate a clock or recovery state.

## 0.7.14 — bounded redacted timeline

- The gateway stores at most **12** in-memory events, retained at most **10 minutes**. Events are allowlisted as `hub_started`, `hub_stopped`, `transport_refusals`, `login_refusals` and `rate_limit_refusals` with coarse event age only. No raw audit rows, actor IDs, usernames, source IPs, credentials or command text enter the browser projection.
- `unknown` history (not available or not verified) is different from `observed` with an empty event list (no verified transition yet). A legacy source, upstream failure, expired observation, clock reversal or hub counter/uptime reset discards previous-generation evidence rather than showing it as fresh.

## 0.7.15 — private threshold alerts

- Transport/TLS refusal, authentication-denial and rate-limit signals require **five events** in a short window and have a **60-second cooldown** per class. Bounded in-memory windows and fixed event names avoid identifiers, unbounded growth and alert storms.
- Audit loss and upstream unavailability are **unknown**, not healthy. A gateway restart begins with no incident history; the durable rate limiter and audit integrity remain separate security controls. Never publish private JSON, audit rows, tokens or SQL as CI artifacts.

## 0.7.16 — accessible EN/PL workflow

1. Use the approved private HTTPS origin. Sign in with an authorized C++ administrator/Owner grant plus password and TOTP or enrolled passkey; an anonymous browser cannot obtain metrics.
2. The `EN` and `PL` controls set the correct main-content language for screen readers. Keyboard navigation and mobile presentation cover the read-only incident region and session-expired messages.
3. `Acknowledge locally` changes **only the current screen**. It does not POST to the hub or persist an operator decision; the button returns to an unacknowledged state on the next verified refresh.
4. Expired/revoked sessions, failed reads and stale samples hide the previous private counters. A quiet timeline is **not** evidence that the hub is incident-free.

## CI and handoff

Node and Playwright regressions cover source age, legacy/invalid readings, monotonic reversal, hub restart, aggregate privacy, alert bounds/cooldown, bilingual keyboard/mobile UX and privilege exclusions. Acceptance requires green CI on the **exact final SHA** for GitHub-hosted Ubuntu 22.04/24.04/26.04. Per OWNER, **stop after the push triggers CI without waiting for results**. Pending CI is not Stage IV acceptance. Leave the PR in draft and do not merge, deploy or operate a production hub.

## Fix Pass 4 — review and repairs (2026-11-24)

- Rechecked the private C++→Node→browser observation chain. The final C++ grant check can await another upstream request after status receipt: sample age must include this monotonic wait and reject any now-over-10-second observation, not publish the previous age as current. Added a delayed-final-grant negative test and a recovery-to-unknown baseline check.
- Rechecked timeline/alert boundedness, raw-counter redaction and audit-loss invalidation. A monotonic reversal *after* an audit alert but *before* the next C++ observation could produce a negative event age; enforce one clock ordering across source receipts, audit events and incident reads. Added a regression covering this ordering.
- Rechecked EN/PL mobile incident region, keyboard-only local acknowledgment and absence of hub control writes. Removed the all-panel `aria-live` announcement that could repeatedly announce rapidly changing incident ages/counters; discrete `role=status` messages remain exposed to assistive technology.
- These targeted regressions do not substitute for the required complete Ubuntu 22.04/24.04/26.04 exact-head browser/C++/security/CodeQL/sanitizer/systemd/recovery/benchmark CI matrix. No acceptance, deployment or merge is claimed before its separate verification.
