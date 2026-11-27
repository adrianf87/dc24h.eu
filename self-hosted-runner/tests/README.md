# Self-hosted Debian 13 runner tests

**All runner-specific test implementation lives here.** The project-wide `tests/` directory remains reserved for application/protocol/build tests. If GitHub Actions is later integrated, the workflow definition must stay in `.github/workflows/` (GitHub's required location), but should call entry points under `self-hosted-runner/scripts/`.

## Test commands

From the project checkout:

```bash
bash self-hosted-runner/scripts/run-tests.sh offline
```

Runs only repository layout, collector syntax, and available snapshot checksum tests. It does **not** connect to the VPS, call `sudo`, or mutate services. If the full package snapshot has not been imported, the checksum test reports **SKIPPED**, not PASS.

On the actual `server-747585786` VPS, running **as `github-runner`**:

```bash
bash self-hosted-runner/scripts/run-tests.sh host-read-only
```

Asserts Debian 13, correct runner account/hostname, noninteractive passwordless `sudo -n /usr/bin/true`, and read-only checks of TCP listeners for ADCS, WebAdmin, Node and MariaDB. It never invokes the application login, mail delivery, production DB updates, service restarts or package installs.

To run both modes sequentially, explicitly invoke:

```bash
bash self-hosted-runner/scripts/run-tests.sh all
```

## Boundaries

- Runner-specific code stays inside `self-hosted-runner/tests/` and `self-hosted-runner/scripts/`.
- General C++, protocol, WebAdmin and database test suites stay in their established project directories.
- **No automatic PR-triggered execution** of the host-specific tests on the public-service VPS with passwordless `sudo`.
- A passing offline test suite is **not** equivalent to successful Debian 13 host qualification; check skipped test counts separately.
- Privileged service install, restart, restore/rollback and mail verification require distinct OWNER-authorized procedures and fixtures; they are deliberately not part of these read-only checks.
