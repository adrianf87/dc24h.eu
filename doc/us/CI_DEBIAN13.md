# Self-hosted Ubuntu 24.04 CI

`.github/workflows/build.yml` is the only active workflow. Every job targets the existing runner `server-747585786` using all five labels: `[self-hosted, Linux, X64, server-747585786, ubuntu-24.04]`. There is no GitHub-hosted runner target or fallback. If this runner is offline or busy, jobs wait in the queue.

The server matrix retains **Debug and Release**, `fail-fast: false` and a maximum of one matrix job at a time. It checks pre-provisioned build tools and required dependencies, builds C++20 and runs all CTest suites with required integrations, including ncdc, MariaDB and Apache2. The WebAdmin job uses the same runner with Node 22, production build/typecheck, unit tests, Chromium browser tests and the real C++ API integration. A single registered runner executes one job at a time.

The runner account needs Git, Python 3, network access to package registries and a writable workspace. CI never invokes sudo or requests a password. The runner labels shown above must remain assigned in GitHub settings.

## One-time administrator setup

The first self-hosted run failed before compilation because the runner account could not use sudo without a password. From a reviewed checkout on **server-747585786 (Ubuntu 24.04)**, an administrator installs the required server and Chromium system packages once:

```sh
sudo python3 scripts/ci/runner_dependencies.py install all
```

This explicit setup uses apt and may start the packaged MariaDB/Apache services; use the dedicated CI machine. It does not change sudoers, runner permissions, passwords or repository secrets. The script refuses installation on another OS or without root. Review the package list without privileges with `python3 scripts/ci/runner_dependencies.py list all`.

Normal jobs only run `check server` / `check webadmin`. Missing packages produce an actionable failure; tests are not skipped. Playwright downloads its Chromium build into the runner user's cache with `npx playwright install chromium`, without `--with-deps` or sudo. Repeat administrator provisioning if dependencies change. The package list mirrors Chromium dependencies in the locked Playwright Ubuntu 24.04 x64 catalog.

Push and pull-request events run this workflow. Configuration checks are performed locally; publication does not wait for remote **Check status**. Remote execution results are available in GitHub Actions. Existing runs use their original workflow revision; this change routes new runs to the self-hosted runner.

The former Proxmox/Debian VM workflows remain retired. Archived workflows outside `.github/workflows/` are not active GitHub Actions workflows. Supported application platforms and historical test results are unchanged.
