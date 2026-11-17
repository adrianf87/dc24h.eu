# GitHub Actions CI

The `Check status` workflow in `.github/workflows/build.yml` runs on disposable
GitHub-hosted `ubuntu-24.04` runners. Pushes and pull requests targeting `main`
or `alpha-0.7.8` / `alpha-0.7.9` / `alpha-0.7.10` / `alpha-0.7.11` / `alpha-0.7.12` / `alpha-0.7.13` / `alpha-0.7.14` / `alpha-0.7.15` / `alpha-0.8.1` / `alpha-0.8.2` / `alpha-0.8.3` / `alpha-0.8.4`, including fork pull requests, and manual dispatches trigger CI.
The workflow has read-only repository permissions and disables persisted checkout credentials.

Three jobs can run independently:

- **C++ Debug** and **C++ Release**: separate matrix jobs install build dependencies
  and MariaDB, Apache and ncdc, configure C++20 with `BUILD_TESTING=ON` and
  `DC24H_REQUIRE_INTEGRATION_TESTS=ON`, then build and run every CTest suite.
  Missing integration tools fail configuration. Matrix failures do not cancel the other build.
- **WebAdmin**: Node 22, `npm ci`, typecheck/production build, Node unit tests,
  Chromium browser tests and real C++ API/React integration. This job builds
  its own Release `dc24h` binary with `BUILD_TESTING=OFF`; the matrix above
  covers the full C++ test suite. Playwright installs Chromium and its system dependencies.

Each job uses a separate GitHub-hosted Ubuntu 24.04 runner with four logical
CPUs (vCPUs), as provided for this public repository. The workflow verifies
`nproc == 4` in each job. Build/test commands use `nproc`; there is no fixed
worker-count cap or matrix max-parallel setting. CI completion is not awaited.
See [GitHub's runner specifications](https://docs.github.com/en/actions/reference/runners/github-hosted-runners). Tests start their own isolated services. Packages are installed
with sudo on disposable GitHub machines; builds and tests run as the runner user.
No self-hosted runner registration, Contabo host or Proxmox VM is required.

Concurrency is grouped by workflow and Git ref with `cancel-in-progress: false`.
C++ jobs have a 120-minute timeout; WebAdmin has 60 minutes. Check each run under
[GitHub Actions](https://github.com/adrianf87/dc24h.eu/actions/workflows/build.yml)
for actual results.

From alpha-0.7.13, all three jobs use the same GitHub-hosted Ubuntu 24.04 configuration as main. This supersedes the initial self-hosted configuration of this release. Push triggers CI; remote completion is not awaited.
