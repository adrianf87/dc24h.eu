# Self-hosted Ubuntu 24.04 CI

`.github/workflows/build.yml` is the only active workflow. Every job targets the existing runner `server-747585786` using all five labels: `[self-hosted, Linux, X64, server-747585786, ubuntu-24.04]`. There is no GitHub-hosted runner target or fallback. If this runner is offline or busy, jobs wait in the queue.

The server matrix retains **Debug and Release**, `fail-fast: false` and a maximum of one matrix job at a time. It installs build tools and required dependencies, builds C++20 and runs all CTest suites with required integrations, including ncdc, MariaDB and Apache2. The WebAdmin job uses the same runner with Node 22, production build/typecheck, unit tests, Chromium browser tests and the real C++ API integration. A single registered runner executes one job at a time.

The runner account needs Git, network access to package registries, a writable workspace and passwordless sudo for the apt and Playwright dependency installation steps. CMake, the C++ compiler and Python are installed explicitly rather than assumed to come from a GitHub-hosted image. The runner labels shown above must remain assigned in GitHub settings.

Push and pull-request events run this workflow. Configuration checks are performed locally; publication does not wait for remote **Check status**. Remote execution results are available in GitHub Actions. Existing runs use their original workflow revision; this change routes new runs to the self-hosted runner.

The former Proxmox/Debian VM workflows remain retired. Archived workflows outside `.github/workflows/` are not active GitHub Actions workflows. Supported application platforms and historical test results are unchanged.
