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

Three jobs share that one runner:

- **C++ Debug** and **C++ Release**: separate matrix jobs configure C++20 with
  `BUILD_TESTING=ON` and `DC24H_REQUIRE_INTEGRATION_TESTS=ON`, then build and run
  every CTest suite. The runner must already provide the compiler, MariaDB, Apache
  and ncdc. Jobs do not install packages and do not use sudo. Missing tools fail
  the runner check. Matrix failures do not cancel the other build.
- **WebAdmin**: Node 22, `npm ci`, typecheck/production build, Node unit tests,
  Chromium browser tests and real C++ API/React integration. This job builds
  its own Release `dc24h` binary with `BUILD_TESTING=OFF`; the matrix above
  covers the full C++ test suite. Playwright installs Chromium for the runner user
  and does not install system packages.

Each job checks that `RUNNER_NAME` is `server-747585786` and that the OS is
Ubuntu 24.04. Build and test commands use `nproc` on that machine. There is no
fixed worker-count cap and no matrix max-parallel setting. CI completion is not
awaited. Tests start their own isolated services.

Concurrency is grouped by workflow and Git ref with `cancel-in-progress: false`.
C++ jobs have a 120-minute timeout; WebAdmin has 60 minutes. Check each run under
[GitHub Actions](https://github.com/adrianf87/dc24h.eu/actions/workflows/build.yml)
for actual results.

This runner selection supersedes the GitHub-hosted Ubuntu 24.04 jobs previously
described for this branch. Push triggers CI; remote completion is not awaited.
