# Self-hosted Debian 13 runner tests

Runner-specific tests live here. The full Debian 13 application test snapshot is in [debian-13/](debian-13/README.md): 267 copied source files and 13 environment-specific ports. Root `tests/` is retained and receives a CI-only snapshot overlay after checkout. If GitHub Actions is later integrated, the workflow definition must stay in `.github/workflows/` (GitHub's required location), but should call entry points under `self-hosted-runner/scripts/`.

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
- General C++, protocol, WebAdmin and database suites remain in root `tests/`, and the Debian 13 snapshot is copied into that checkout by `activate-debian13-tests.sh` before testing.
- OWNER explicitly approved Debian 13 self-hosted PR CI on the shared test VPS with passwordless `sudo`. The risk affects the hub/WebAdmin test services on the same host.
- A passing offline test suite is **not** equivalent to successful Debian 13 host qualification; check skipped test counts separately.
- Privileged service install, restart, restore/rollback and mail verification require distinct OWNER-authorized procedures and fixtures; they are deliberately not part of these read-only checks.
