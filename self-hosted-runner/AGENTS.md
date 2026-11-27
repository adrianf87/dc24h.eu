# Rules for files under self-hosted-runner/

This directory is the canonical home for **self-hosted runner-specific** documentation, host/tool inventories, package histories, scripts, test harnesses and fixtures.

1. Store runner-specific test implementations under `self-hosted-runner/tests/`; store executable entry points and inventory utilities under `self-hosted-runner/scripts/`.
2. Store point-in-time system/runner/package information in `self-hosted-runner/DDMMYYYY/` (for example `27112026/`). Never overwrite an earlier dated inventory; record updates in `CHANGELOG.md` and new timestamped snapshots.
3. A GitHub Actions workflow definition must remain under repository-root `.github/workflows/` because GitHub requires this location. Such workflows should call runner scripts from `self-hosted-runner/scripts/`; do not duplicate runner-specific implementation in workflow YAML.
4. Generic application test originals remain under repository-root `tests/`. The Debian 13 complete copy lives in `self-hosted-runner/tests/debian-13/` and is overlaid into the CI checkout by the activation script, preserving existing CMake and test call sites.
5. Debian GNU/Linux 13 is the OWNER-selected CI target. Ubuntu 22.04/24.04/26.04 and Debian 12 matrices are not part of the desired future runner qualification.
6. OWNER explicitly authorized CI on the shared test VPS, including automatic pull-request testing and passwordless `sudo` under `github-runner`. This is an accepted root-capable test-host risk. Record actual host-mutating and destructive rollback coverage honestly; do not infer PASS from skipped work.
7. Never read, commit, or log runner credentials, access tokens, SMTP/DB secrets, TLS keys, session cookies or production state. Keep test output redacted.
8. Offline tests must be callable without root or network. Host read-only checks must not restart services, alter package installations or modify public ports.
9. Report SKIPPED / NOT_TESTED separately from PASS; avoid claiming host qualification based solely on repository tests.

Current runner record: `self-hosted-runner/27112026/`.
