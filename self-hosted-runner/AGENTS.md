# Rules for files under self-hosted-runner/

1. This directory contains runner configuration, Docker orchestration scripts and sanitized operational documentation. Main application tests remain in repository-root `tests/`; do not recreate the deleted `self-hosted-runner/tests/` snapshot.
2. GitHub Actions workflow files belong in repository-root `.github/workflows/`. Workflow test jobs call the versioned scripts under `self-hosted-runner/scripts/`.
3. The OWNER-selected application test targets are Debian 12, Debian 13 and Ubuntu 22.04, 24.04 and 26.04, executed **one Docker container at a time in that order**. Never run them as a parallel GitHub Actions matrix.
4. Allocate exactly 16 logical CPU IDs using Docker `--cpuset-cpus`, **not** a CPU quota (`--cpus`), and 32 GiB memory per container. Stop and remove one test container before the next starts.
5. Use the shared host lock to serialize Docker application tests across workflow runs. Do not prune unrelated Docker images, volumes, networks or containers.
6. The approved runner is `server-747585786` on Debian 13 under `github-runner`, sharing the VPS with public ADCS and WebAdmin. Docker daemon access is root-equivalent. Do not use `--privileged`, host network, host-root mounts, Docker socket mounts or service credentials for application tests.
7. Never read, commit or log runner credentials, access tokens, SMTP/DB secrets, TLS keys, session cookies or production state. Use only sanitized evidence and synthetic fixtures.
8. Application tests must execute inside Docker, not install packages or restart services on the runner host. Tests that require host systemd or public-service modification must be separately approved and reported as NOT_TESTED by this Docker suite.
9. Report individual distribution PASS/FAIL accurately. Do not infer full host qualification from container results, or report SKIPPED/NOT_TESTED as PASS.

Current runner documentation: `self-hosted-runner/README.md`.
