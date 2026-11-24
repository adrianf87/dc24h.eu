# Beta 0.8.13–0.8.16 — Stage IV: privacy-preserving operator reliability

Issue [#240](https://github.com/adrianf87/dc24h.eu-high-starter/issues/240); single development branch `beta-0.8.x-stage-full`, draft PR #241. **Owner exception (2026-11-24): Stage IV was expressly authorized before Stages II/III appeared on the shared branch.** This is an implementation handoff, not stage acceptance or permission to merge/deploy. Future Stages II/III and Fix Pass 4 still have their own separate work units and integration gates.

## 0.8.13 — authenticated freshness across restart

- Retain the beta-0.7 C++ observation envelope (`schema=1`, source wall-clock observation), Node's at-most-10-second verified source-age contract, monotonic gateway delay, and browser's total 45-second freshness expiry. Invalid/future/old samples fail closed; older v1 samples never claim verified source freshness.
- A restarted Node process begins with **unknown live incident state**. An optional persisted aggregate journal cannot establish the new process's C++ source identity, hub health, recovery status, permission grants or current service state. Only a new authenticated C++ read establishes a fresh baseline.
- The gateway constructs only allowlisted counters and event kinds after a fresh C++ grant check. Raw C++ network refusal counts, source IPs, actor/account/device names, audit rows and tokens are not rendered.

## 0.8.14 — explicitly historical local incident continuity

- `remote-incident-journal.ts` is an **optional, separately initialized disposable operator telemetry file**, not an authoritative sixth credential or recovery store. The five critical private stores and MariaDB schema-v11 six-component recovery cohort do not change.
- Owner-only directory/file (`0700`/`0600`), canonical absolute path, no symlink ancestors/leaf, exclusive initialization, fixed redacted enum names, **12 entries maximum**, **10-minute retention**, **4 KiB maximum**, content-fingerprint change detection and fsync-backed replacement. Corruption, external rewrite, reversal, missing file or unsafe modes fail closed to `unknown`. No raw audit or identity is copied into this file.
- The authenticated response distinguishes **live** `incidents.state` and optional **retained** `incidents.retained.state`. Retained events have an explicit EN/PL heading “not live,” never appear as proof of present health or forensic completeness. A lost audit sink invalidates visible retained history until a separate operator recovery.
- No persistent journal is enabled by default. When absent, the prior in-memory-only, read-only incident timeline remains. This file is not automatically created, restored, published as an artifact, or given to the browser as a raw file.

## 0.8.15 — audit capacity and stopped-writer preservation

- The durable SHA-256 audit chain still **fails closed at 8 MiB** and never rotates, truncates or resets automatically. A local, fixed-code, cooldown-limited `audit_near_capacity` warning starts at **75%** of the configured limit; this is not a browser counter or a new authentication grant.
- The explicit offline `remote-audit-archive.ts` tool requires `--owner-stopped-writer-ack` and a destination that does not exist in a private directory. It verifies source chain integrity before copy, compares full source and archive digests after fsync, and preserves the original file byte-for-byte. It never reinitializes an audit chain or resumes authentication after exhaustion.
- Stop the writer externally before executing the archive tool; a CLI flag cannot prove systemd was stopped. The existing 8 MiB limit remains active after archiving. Any future log rotation with cryptographic chain continuity requires a separate Owner-approved design and migration. Hash chaining is tamper-evidence against incidental corruption, not an adversarially keyed signature.
- Deterministic tests use a reduced bound to exercise **pre-capacity warning, exact-bound write refusal**, original-byte preservation, corruption refusal and archive-collision refusal without performing tens of thousands of production-sized fsyncs. An actual OS `ENOSPC` or production retention regime is not certified.

## 0.8.16 — EN/PL accessible read-only operator view

- Historical and live timelines are separately labeled with bilingual text, a semantic region heading and explicit unknown-state explanations. Keyboard-only local acknowledgment is a browser-only state change, not an audit acknowledgment or upstream action.
- Browser response parsing enforces exact keys, bounded event age/count, fixed enums and no private actor/IP/credential fields. Negative tests cover unknown retained history, foreign identifiers and unverified sources.
- The remote gateway stays loopback-bound with Apache private HTTPS. No hub start/stop, role/grant mutation, public listener or remote operator action is added by Stage IV.

## Required evidence and non-claims

Targeted Node, parser, audit, restart and browser/keyboard regressions are included. Full C++/WebAdmin/Chromium/CodeQL/ASan/UBSan/systemd/ADC/ADCS/recovery/package/benchmark tests **must run at one exact final source SHA** on GitHub-hosted Ubuntu **22.04, 24.04 and 26.04**. Do not run required CI on Debian, Xeon or private runners. Per Owner: **stop immediately after branch push/CI trigger; do not wait or poll**. Pending CI is not acceptance. Do not mark Stage IV complete, merge, release, deploy, publish audit data or alter production recovery cohorts.
