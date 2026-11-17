# GitHub Actions CI

From alpha-0.7.13, `Check status` in `.github/workflows/build.yml` runs on the
user-provided self-hosted runner `server-747585786`, with labels
`self-hosted`, `Linux`, `X64` and `server-747585786`. The job verifies Ubuntu 24.04,
runner name and the `github-runner` service user before building. Earlier releases
used other runner configurations; those historical notes do not describe current CI.

Pushes, same-repository pull requests targeting `main` or `alpha-0.7.*`, and manual
dispatches trigger checks. Fork pull requests do not execute on this persistent
runner. Repository permissions are read-only and checkout does not persist credentials.
The workflow never contains the runner login password or database credentials.

One repository-wide concurrency group runs a single job at a time. Debug and
Release are built and tested sequentially in separate build directories, followed
by WebAdmin. This also avoids concurrent checkout/build operations in the same
self-hosted workspace. CMake, CTest, Node and Playwright use `nproc` workers.

The administrator must provision C++20 build tools, CMake, Python 3, OpenSSL,
libutf8proc-dev, libgcrypt20-dev, libssl-dev, libmariadb-dev, MariaDB server/client,
Apache2, ncdc and Chromium system libraries. The workflow checks tool availability
and CMake validates headers/libraries. Packages and services are not installed or
reconfigured with sudo during jobs. Node 22 is selected with setup-node; npm ci
installs the lockfile and Playwright installs its user-local Chromium binary.

Both native builds require `DC24H_REQUIRE_INTEGRATION_TESTS=ON` and run all CTest
suites, including disposable MariaDB recovery/restart, actual ncdc ADC/ADCS and
Apache. WebAdmin runs typecheck, production build, unit tests, browser tests and
integration against the Release C++ API. Failure reports are retained for seven days.
No production database, account, schema, TLS key or service is modified by CI.
The overall job timeout is 180 minutes.

A pushed branch triggers the workflow. Local success does not establish runner
availability or remote success: inspect the matching commit under
[GitHub Actions](https://github.com/adrianf87/dc24h.eu/actions/workflows/build.yml).
