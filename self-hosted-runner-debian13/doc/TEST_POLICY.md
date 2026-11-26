# Security boundaries and supplemental test policy

## Allowed now

- Manually dispatched, OWNER-triggered, **read-only** host inventory and source
  metadata tests on `server-747585786` after the exact runner label is verified.
- Python regression checks for Stage II preflight and US-only documentation
  policy, executed as the unprivileged `github-runner` account.
- Small, sanitized JSON artifact reporting only the reviewed host/test shape.
- After a separately reviewed merge and OWNER dispatch, exactly three
  passwordless-sudo commands for the Stage II workflow: `sudo -n id -u`,
  `sudo -n python3 scripts/public_stage2_preflight.py` (read-only inventory),
  and `sudo -n python3 self-hosted-runner-debian13/tests/stage2_host_proof.py`.
- Non-root temporary-prefix C++/WebAdmin builds, a curated unprivileged CTest
  subset, and synthetic backup/restore/rollback and process-exit monitoring.
- OWNER-approved real test-only root installation in a unique `/opt` directory,
  two uniquely named runtime systemd units in `/run/systemd/system`, test hub
  start/restart/crash recovery, test-unit conflict, test fixture restore,
  installed-binary rollback, and the systemd sandbox integration CTest.

## Not authorized

- Automatic execution on `push`, `pull_request`, forks, or unreviewed code.
- Installing OS packages, enabling real hub units, changing firewall/DNS/TLS,
  binding public listeners, or launching a public WebAdmin.
- Reading, copying, migrating, or modifying `/var/lib/dc24h`, private MariaDB,
  auth keys, production configuration, audit state, or credentials.
- Running maintenance or rollback commands against a real existing service.
- Running broad CTest suites with privileged dpkg/package-manager integration
  or unreviewed host mutations. The reviewed temporary WebAdmin build may use
  `npm ci --ignore-scripts` without root, but must not install OS packages.
- Treating the presence of a runner, its CPU count, or green supplemental tests
  as a release, production, or Stage II GO.

## Fail-closed requirements

1. The dispatch must be for the protected `main` ref, sent by GitHub user
   `adrianf87` with the explicit confirmation phrase.
2. The job must select the exact custom label `twilight-debian13-stage2` and
   verify `RUNNER_NAME=server-747585786` and Unix user `github-runner`.
3. OS must be Debian 13, systemd must be PID 1, and both old/new hub unit states
   must be known and inactive. Unknown process visibility or a running hub
   blocks the test.
4. The original supplemental smoke workflow never uses `sudo`. The separate
   Stage II workflow permits the three reviewed noninteractive root calls
   specified above; builds still run as `github-runner`.
   **This is a workflow policy, not an OS privilege restriction:** because the
   account has passwordless sudo, a compromised build tool or npm dependency
   could still escalate. The OWNER confirmed that this is a dedicated test VPS
   without production secrets/private state and explicitly kept the existing
   passwordless sudo access. New privileged tests require confirmation
   `RUN_STAGE2_SUDO_HOST_TESTS`.
5. Published output is limited to bounded, reviewed JSON with no raw addresses,
   secrets, journal contents, private paths, or user identifiers.

**Stage II acceptance remains separate from test execution.** The approved
disposable VPS can now provide test-only privileged installation/recovery
evidence. A PASS does not prove historical-version package rollback, MariaDB or
private-state restore, ACL/xattr preservation, public deployment, or production
RPO/RTO.
