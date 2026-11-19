# GitHub Actions CI

The ncdc cross-interface fixtures reserve ports on both loopback addresses, retaining the conflicting listener until the client is ready. Allocation-only EADDRINUSE collisions choose a fresh pair with bounded retries; test failures are never retried. `integration.ncdc_ports` covers deterministic TCP/UDP collisions and cleanup.

The `Check status` workflow in `.github/workflows/build.yml` runs on GitHub-hosted `ubuntu-24.04` runners. Pushes to `main`, `alpha-0.8.7`, `alpha-0.8.8`, `alpha-0.8.9`, `alpha-0.8.10`, `alpha-0.8.11`, `alpha-0.8.12`, `alpha-0.8.13`, `alpha-0.8.14`, `alpha-0.8.15`, `alpha-0.9.1` and `alpha-0.9.2`, and manual dispatches, trigger CI. Fork pull requests do not run. The workflow has read-only repository permissions and disables persisted checkout credentials.

Two jobs run. Each job checks that the runner has four logical CPUs and then uses `nproc` workers. The C++ job builds and tests Debug and Release, including integration tests, after installing the toolchain, MariaDB, Apache and ncdc. The WebAdmin job builds the Release hub binary, then runs typecheck, the production build, Node tests, Chromium and the live C++ API integration with Node.js 22. Non-TLS bytes on a TLS port may end as EOF or one fatal alert record. A test client close accepts abrupt EOF or a completed unwrap(); the server still does not call SSL_shutdown().

Concurrency uses one group per workflow and ref, `dc24h-github-` plus the workflow name and ref, with `cancel-in-progress: false`. The C++ job has a 120-minute timeout and the WebAdmin job has a 60-minute timeout. Check each run under [GitHub Actions](https://github.com/adrianf87/dc24h.eu/actions/workflows/build.yml) for actual results. CI completion is not awaited.

This selection replaces the custom runner `server-747585786` for new pushes of these branches. Push triggers CI; remote completion is not awaited.
