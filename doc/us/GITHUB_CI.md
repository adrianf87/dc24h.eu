# GitHub Actions CI

The ncdc cross-interface fixtures reserve ports on both loopback addresses, retaining the conflicting listener until the client is ready. Allocation-only EADDRINUSE collisions choose a fresh pair with bounded retries; test failures are never retried. `integration.ncdc_ports` covers deterministic TCP/UDP collisions and cleanup.

The `Check status` workflow in `.github/workflows/build.yml` runs on the custom
runner `server-747585786` with labels `self-hosted`, `Linux`, `X64` and
`server-747585786`. Pushes to `main`, `alpha-0.8.7`, `alpha-0.8.8` and `alpha-0.8.9`, and
manual dispatches, trigger CI. The workflow has read-only repository permissions
and disables persisted checkout credentials.

One job on that Debian 13 host retires obsolete per-container runner services,
then checks Ubuntu 22.04, Ubuntu 24.04, Ubuntu 26.04, Debian 12 and Debian 13
one container at a time. Each check sets 16 CPU threads and 32 GiB RAM, stops
an idle container before the check, and stops it again afterward. A container
that is already executing a GitHub runner worker stops the job instead of being
interrupted. Non-TLS bytes on a TLS port may end as EOF or one fatal alert
record, depending on that container's OpenSSL. A test client close accepts
abrupt EOF or a completed unwrap(); the server still does not call
SSL_shutdown(). Inside the container the job installs the toolchain, MariaDB,
Apache and ncdc, then runs C++ Debug and Release with 16 build and CTest
workers, plus WebAdmin typecheck, Node tests, Chromium and the live C++ API
integration. The host itself is not the build machine. After the five checks,
every container is stopped.

Concurrency uses one group, `dc24h-eu-docker-host-sequential`, with
`cancel-in-progress: false`. The job has a 360-minute timeout. Check each run
under
[GitHub Actions](https://github.com/adrianf87/dc24h.eu/actions/workflows/build.yml)
for actual results. CI completion is not awaited.

This runner selection supersedes the GitHub-hosted Ubuntu 24.04 jobs previously
described for this branch. Push triggers CI; remote completion is not awaited.
