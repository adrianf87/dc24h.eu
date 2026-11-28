# GitHub Actions CI

The ncdc cross-interface fixtures reserve ports on both loopback addresses, retaining the conflicting listener until the client is ready. Allocation-only EADDRINUSE collisions choose a fresh pair with bounded retries; test failures are never retried. `integration.ncdc_ports` covers deterministic TCP/UDP collisions and cleanup.

The Stage IV branch's `Check status` workflow in `.github/workflows/build.yml` runs on GitHub-hosted `ubuntu-22.04`, `ubuntu-24.04` and `ubuntu-26.04`. Pushes to `feature/stage4-webadmin-dedicated-service` and `main`, pull requests targeting `main`, and manual dispatches trigger this workflow. It has read-only repository permissions and disables persisted checkout credentials. `main` remains unchanged until the branch is merged; use the workflow file for the authoritative triggers.

The application job builds C++ Release, runs the full CTest inventory with systemd-host enforcement, builds and tests WebAdmin with Node.js 24, runs Chromium/Playwright, and checks high-severity production npm advisories on each Ubuntu image. ASan and UBSan also run on every image and jointly gate `CI (C++ and WebAdmin)`. Separate three-image workflows retain CodeQL, bounded TSAN/fuzz, Stage III offline and loopback qualification, Stage IV private-storage fault tests and isolated hub/bridge/DB/browser checks. Protected-main and manual OWNER-gated public Stage III probes retain Ubuntu 22.04 and do not modify the VPS. See [Dependencies](DEPENDENCIES.md) and [Beta baseline](BETA.md).

Concurrency uses one group per workflow and ref with `cancel-in-progress: false`. The application job has a 120-minute timeout; sanitizer jobs have 90 minutes. Check **current dc24h.eu** results under [GitHub Actions](https://github.com/adrianf87/dc24h.eu/actions/workflows/build.yml); historical runs only qualify their original source SHA. CI completion is not awaited.

Push triggers CI; remote completion is not awaited.
