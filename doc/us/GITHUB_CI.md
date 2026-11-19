# GitHub Actions CI

The ncdc cross-interface fixtures reserve ports on both loopback addresses, retaining the conflicting listener until the client is ready. Allocation-only EADDRINUSE collisions choose a fresh pair with bounded retries; test failures are never retried. `integration.ncdc_ports` covers deterministic TCP/UDP collisions and cleanup.

The `Check status` workflow in `.github/workflows/build.yml` runs on the custom
runner `server-747585786` with labels `self-hosted`, `Linux`, `X64` and
`server-747585786`. That runner is not copied from branch `contabo-docker-v0.1`;
this branch does not merge `contabo-docker-v0.1` or change `main` by itself.
Pushes and same-repository pull requests targeting `main`
or `alpha-0.7.8` / `alpha-0.7.9` / `alpha-0.7.10` / `alpha-0.7.11` / `alpha-0.7.12` / `alpha-0.7.13` / `alpha-0.7.14` / `alpha-0.7.15` / `alpha-0.8.1` / `alpha-0.8.2` / `alpha-0.8.3` / `alpha-0.8.4` / `alpha-0.8.5` / `alpha-0.8.6` / `alpha-0.8.7` / `alpha-0.8.8`, and manual dispatches, trigger CI.
Fork pull requests do not run on this persistent host. The workflow has read-only
repository permissions and disables persisted checkout credentials.

One job on that Debian 13 host starts each test container only for its own check,
then stops it before the next one starts. The containers are
`ubuntu2204-github-runner`, `ubuntu2404-github-runner`, `ubuntu2604-github-runner`,
`debian12-github-runner` and `debian13-github-runner`. Each check sets 16 CPU
threads and 32 GiB RAM, refuses to start when another of these containers is
already running, and leaves the container stopped. Inside the container the job
installs the toolchain, MariaDB, Apache and ncdc, then runs C++ Debug and Release
with 16 build and CTest workers, plus WebAdmin typecheck, Node tests, Chromium
and the live C++ API integration. The host itself is not the build machine.

The job checks that `RUNNER_NAME` is `server-747585786`, that the user is
`github-runner`, and that Docker can see all five stopped containers. CI
completion is not awaited.

Concurrency is grouped by workflow and Git ref with `cancel-in-progress: false`.
The container job has a 360-minute timeout. Check each run under
[GitHub Actions](https://github.com/adrianf87/dc24h.eu/actions/workflows/build.yml)
for actual results.

This runner selection supersedes the GitHub-hosted Ubuntu 24.04 jobs previously
described for this branch. Push triggers CI; remote completion is not awaited.
