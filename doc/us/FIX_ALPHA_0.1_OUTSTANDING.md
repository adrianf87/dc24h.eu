# Historical alpha-0.1 follow-up — reviewed on beta-0.3.12 main

This branch fixes the two remaining *operational code defects* identified while
revisiting the 11 alpha-0.1 milestones. It does **not** reset the codebase to
alpha-0.1, claim an independent penetration test, or enable new public access.
Read [the original historical audit](AUDIT_ALPHA_0.1.md) for its scope.

## Corrections

1. **A11 — blocking logging:** the hub now formats bounded records on the
   producer thread and writes them from a single dedicated worker. The queue
   never holds more than 256 records and emits a fixed dropped-record count
   after recovering from a blocked sink. A permanently blocked sink may still
   prevent graceful logger destruction; SIGKILL may lose queued records. This
   remains best-effort operational logging, not compliance-grade audit storage.
   Termination signals are masked before creating the new worker, so the
   existing synchronous SIGINT/SIGTERM shutdown contract remains intact.
2. **A12 — misleading readiness:** `/api/v1/health` is unchanged API-worker
   liveness. Bearer-protected `/api/v1/ready` returns 200 only after Core is
   running and the whole configured ADC/ADCS listener set is bound; otherwise
   503. It has no response secrets, browser proxy or SQL access. Live database
   readiness, authenticated-user readiness and service capacity remain separate.

The test coverage adds queue-stall/overflow and 503 readiness regressions,
plus local API process checks. Existing slow-reader, ADC parser fuzzing and
in-process stress tests continue to cover parts of A11/A15.

## Earlier alpha-0.1 audit findings already superseded

- **A01–A04:** documentation metadata and CI integration guards in alpha-0.2.1.
- **A10:** outbound ADC and client session/login protocol in alpha-0.2–0.3.
- **A14:** persisted accounts, password authentication and role controls in
  alpha-0.7–0.8.
- **A13:** an isolated, loopback-only read-only password+TOTP portal in
  beta-0.2–0.3. This does *not* mean a public production gateway is ready.
- **A15:** later Ubuntu 26.04 CI image, sanitizer and bounded fuzz/stress
  regressions. A specific host/OS validation must still be performed separately.

## Deliberate boundaries

Config and TLS certificate hot reload, automated rotation/reopen of log files,
live database health, Internet-facing WebAdmin, new database migrations and
administrative remote writes require separate designs and operator approval.
Do not merge or deploy automatically; full C++/integration CI on the proposed
commit and security review are mandatory before any release decision.
