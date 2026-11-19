# GitHub Actions CI

The ncdc cross-interface fixtures reserve ports on both loopback addresses, retaining the conflicting listener until the client is ready. Allocation-only EADDRINUSE collisions choose a fresh pair with bounded retries; test failures are never retried. `integration.ncdc_ports` covers deterministic TCP/UDP collisions and cleanup.

The `Check status` workflow in `.github/workflows/build.yml` runs on the custom
runner `server-747585786` with labels `self-hosted`, `Linux`, `X64` and
`server-747585786`. Pushes to `main`, `alpha-0.8.7`, `alpha-0.8.8` and `alpha-0.8.9`, and
manual dispatches, trigger CI. The workflow has read-only repository permissions
and disables persisted checkout credentials.

The runner host is Debian 13. The job confirms that operating system, installs
the C++ toolchain, MariaDB, Apache and ncdc on the host, and runs C++ Debug and
Release there with up to 16 build and CTest workers. WebAdmin then runs
typecheck, the production build, Node tests, Chromium and the live C++ API
integration on the same host, using Node.js 22. Non-TLS bytes on a TLS port may
end as EOF or one fatal alert record. A test client close accepts abrupt EOF or
a completed unwrap(); the server still does not call SSL_shutdown().

Concurrency uses one group, `dc24h-eu-docker-host-sequential`, with
`cancel-in-progress: false`. The job has a 180-minute timeout. Check each run
under
[GitHub Actions](https://github.com/adrianf87/dc24h.eu/actions/workflows/build.yml)
for actual results. CI completion is not awaited.

This runner selection supersedes the GitHub-hosted Ubuntu 24.04 jobs previously
described for this branch. Push triggers CI; remote completion is not awaited.
