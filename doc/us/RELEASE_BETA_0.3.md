# beta-0.3.12 — release-candidate review (not production deployment)

This is the **manual** release-readiness checklist for the cumulative branch
`beta-0.3.1-12-full` (stages 0.3.1–0.3.12). An updated runtime version does
not mean CI has passed, the PR is mergeable, a tag exists or a service has
been deployed. Keep the PR as Draft until a separate explicit review decision.

## beta-0.3.11 — bounded load, fault injection and audit

- Socket-free Node regressions submit a 16-login burst plus 128 anonymous
  requests. At most four same-actor logins can use private password proof
  within the one-minute budget; unauthenticated reads are rejected without
  contacting the C++ API. This is **not** a throughput or capacity benchmark.
- A separate test holds 64 concurrent summary reads behind a deterministic
  private-API fault, confirms all return generic 503 without stale/private
  data, and confirms a fresh safe read after recovery. Earlier revocation,
  expiry, concurrent logout, replay, and 12-read permission-race tests remain.
- A small fixed audit formatter now emits only schema, component, approved
  event name and ISO UTC timestamp. Unit tests reject forged events and
  invalid clocks; no account IDs, IPs, passwords, OTP, grants or cookies are
  serialized. Journald retention is still outside the application. Audit
  sink failure must not grant permissions, but successful activity can proceed
  without a durable journal record: **not compliance-grade logging**.

## beta-0.3.12 — checks before any release decision

1. **GitHub CI** must finish successfully for the exact final commit. Review
   `Check status` on hosted Ubuntu 22.04/24.04/26.04: C++ Debug/Release,
   CTest, WebAdmin TypeScript/build/unit/Chromium/real-C++ integration,
   production npm advisory check, ASan and UBSan. A prior commit's green
   checks do not clear the new candidate. Review skipped or cancelled jobs.
2. **Static release gate:** run `python3 tests/beta_release_gate_tests.py`,
   `python3 tests/beta_baseline_tests.py`,
   `python3 tests/remote_gateway_config_tests.py`,
   `python3 tests/project_checks.py`, and the Node tests. Verify the final
   version is `beta-0.3.12` across hub/API/controller/WebAdmin/fixtures.
3. **Scope audit:** inspect the exact five read-only `/remote/v1` routes,
   C++ permission rechecks, no new remote writes, unchanged public Apache,
   no public bind, unchanged MariaDB migration v11, package tuple `0.1.0`,
   and unchanged ADC/ADCS behavior. Review the diffs, not just this document.
4. **MFA/session review:** verify per-account unique secrets, ownership 0600
   under a 0700 directory, synced replay journal across restarts, clock
   synchronization, lockout budgets, cookie/CSRF protection, live grant
   revocation and the 2-minute idle/10-minute absolute session lifetimes.
   A stolen password + live TOTP phishing remains a risk.
5. **HTTPS/systemd and optional pilot:** on a sacrificial host, inspect TLS
   1.2/1.3 negotiation, certificate SAN/expiry/trust, cipher suites, strict
   headers, service isolation and C++ socket/token availability. If explicitly
   testing private access, verify the SSH host key and HTTPS certificate,
   run `python3 scripts/remote_pilot_check.py --ca-file ...`, and confirm
   the workstation SSH tunnel listens only on `127.0.0.1`.
6. **Operational audit:** arrange restricted persistent journald storage,
   retention, review/alerting and off-host backups. The current event stream
   has no per-user attribution, tamper evidence or guaranteed durable write.
   Do not claim a compliance-grade or public-access audit.
7. **Manual go/no-go:** explicitly document approvals, commit SHA, passing
   workflow run, known risks and rollback plan before tagging/merging or
   shipping. This code change creates no release/tag and does not merge to
   `main`. Do not enable a public gateway with this TOTP-only design.

## Rollback / no-go

If HTTPS, the private API, MFA replay persistence or journald inspection
cannot be verified, cancel the pilot and do not deploy. Stop any SSH local
forward, log out and stop the separate gateway. To revert a test deployment,
restore the last reviewed local-only configuration and binary together;
**preserve the MFA enrollment and replay ledger** and never restore an older
ledger with the same secret. Re-check certificate trust and live permissions
after rollback. Never roll back to an MFA-free Internet-facing service.

Remaining work for a production Internet gateway: phishing-resistant
WebAuthn/passkeys, enrollment/recovery and device revocation, independent
rate-limiting/abuse protection, immutable attributed audit, public-edge
architecture, capacity measurements and a separate security review.
