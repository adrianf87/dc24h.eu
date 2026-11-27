# Self-hosted GitHub Actions runner: sequential Docker application tests

This branch tests the C++ server and WebAdmin in **five disposable Docker containers**, in exactly this order:

1. `debian:12`
2. `debian:13`
3. `ubuntu:22.04`
4. `ubuntu:24.04`
5. `ubuntu:26.04`

The runner is `server-747585786`, Debian 13, running as `github-runner`. The host itself is **not** reconfigured or upgraded by the application test scripts. Docker must be installed and available to that account before the job starts.

## Resource limits and sequencing

- Each container uses exactly **16 logical CPU IDs**, selected from the runner account's CPU affinity, passed through Docker's `--cpuset-cpus`. No `--cpus` or CPU time quota is set.
- Each container uses `--memory 32g --memory-swap 32g` (32 GiB RAM and no additional swap allowance).
- The script holds a host-wide `flock` for the full five-image run. Only one application-test container is started at a time. After its tests finish (PASS or FAIL), its reports are copied and the container is stopped and removed before the next image starts.
- A failure in one distribution does not skip the remaining distributions. The overall GitHub Action fails if **any** distribution fails.
- Docker images may remain cached for faster future runs; running containers are removed. The script never uses `docker system prune` or removes unrelated containers.

## Tests

Workflow entry point: `self-hosted-runner/scripts/run-docker-sequential.sh`.

For each OS, `test-app-in-container.sh` installs its build tools and verified official Node.js 24, then runs:

- C++ Debug and Release compilation and CTest;
- AddressSanitizer and UndefinedBehaviorSanitizer regression subsets;
- WebAdmin typecheck, public/Stage IV builds and unit tests;
- Playwright Chromium regressions and WebAdmin/C++ integration tests.

The original application tests remain in repository-root `tests/`; no extra test copy exists under `self-hosted-runner/`.

Reports are stored in `${RUNNER_TEMP}/dc24h-docker-results/<distribution>/`, uploaded as workflow artifacts and summarized per OS. `FAIL` and `NOT_TESTED` must not be reported as `PASS`.

## Safety and limitations

The host also runs public ADCS and WebAdmin. Test containers are disposable, have no published ports, and do not bind-mount host services, database directories, credentials or the Docker socket. They can reach external package registries while provisioning dependencies.

The `github-runner` account can use the Docker daemon; access to Docker on a shared service host remains root-equivalent. Only trusted workflow code should invoke the Docker CLI. Never expose secrets through container environment or logs.

The workflow does not claim that host-level systemd service restarts, production migrations or rollback are tested in Docker. Browser dependencies for newer OS images must be verified by actual CI runs; an unsupported dependency must fail visibly.

To check the local orchestration script syntax without running Docker:

```bash
bash -n self-hosted-runner/scripts/run-docker-sequential.sh
bash -n self-hosted-runner/scripts/test-app-in-container.sh
```

To run on the authorized self-hosted VPS from an exact clean checkout:

```bash
bash self-hosted-runner/scripts/run-docker-sequential.sh
```

Do not commit `.credentials*`, PATs, database/SMTP secrets, TLS private keys, user data or raw process environments.
