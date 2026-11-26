# Security boundaries and supplemental test policy

## Allowed now

- Manually dispatched, OWNER-triggered, **read-only** host inventory and source
  metadata tests on `server-747585786` after the exact runner label is verified.
- Python regression checks for Stage II preflight and US-only documentation
  policy, executed as the unprivileged `github-runner` account.
- Small, sanitized JSON artifact reporting only the reviewed host/test shape.
- After a separately reviewed merge and OWNER dispatch, exactly two
  passwordless-sudo operations for the Stage II workflow: `sudo -n id -u` and
  `sudo -n python3 scripts/public_stage2_preflight.py` (read-only inventory).
- Non-root temporary-prefix C++/WebAdmin builds, a curated unprivileged CTest
  subset, and synthetic backup/restore/rollback and process-exit monitoring.

## Not authorized

- Automatic execution on `push`, `pull_request`, forks, or unreviewed code.
- Any other use of `sudo`, installing OS packages, enabling systemd units, changing
  firewall/DNS/TLS, binding public listeners, or launching a public WebAdmin.
- Reading, copying, migrating, or modifying `/var/lib/dc24h`, private MariaDB,
  auth keys, production configuration, audit state, or credentials.
- Running maintenance or rollback commands against a real existing service.
- Running broad CTest suites with privileged systemd/dpkg integration on a
  potentially shared/production VPS. The reviewed temporary WebAdmin build may
  use `npm ci --ignore-scripts` without root, but must not install OS packages.
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
   Stage II workflow permits only the two reviewed noninteractive root checks
   specified above; all other phases run as `github-runner`.
   **This is a workflow policy, not an OS privilege restriction:** because the
   account has passwordless sudo, a compromised build tool or npm dependency
   could still escalate. The OWNER must confirm that this runner is disposable
   and contains no production secrets/state before authorizing dispatch, or
   restrict its sudoers permissions through a separately approved host change.
5. Published output is limited to bounded, reviewed JSON with no raw addresses,
   secrets, journal contents, private paths, or user identifiers.

**Pre-merge Stage II engineering acceptance still requires a separately
isolated Debian 13 VM/clone with synthetic data**, documented install/rollback
evidence and its own OWNER decision. This shared VPS runner does not replace it.
