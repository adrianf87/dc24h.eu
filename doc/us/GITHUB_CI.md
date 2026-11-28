# GitHub Actions CI

The ncdc cross-interface fixtures reserve ports on both loopback addresses, retaining the conflicting listener until the client is ready. Allocation-only EADDRINUSE collisions choose a fresh pair with bounded retries; test failures are never retried. `integration.ncdc_ports` covers deterministic TCP/UDP collisions and cleanup.

The `Check status` workflow runs on GitHub-hosted `ubuntu-22.04`, `ubuntu-24.04`, and `ubuntu-26.04`. Pushes to `main` and `feature/stage4-webadmin-dedicated-service`, pull requests targeting `main`, and manual dispatches trigger this workflow. It uses read-only repository permissions and disables persisted checkout credentials. [Workflow definition](../../.github/workflows/build.yml)

The application job builds C++ Release, runs the full CTest inventory including hosted systemd tests, builds and tests WebAdmin with Node.js 24, runs Chromium/Playwright, and checks high-severity production npm advisories on each Ubuntu image. ASan and UBSan run on all three images and jointly gate `CI (C++ and WebAdmin)`. Separate workflows cover CodeQL, bounded TSAN and fuzzing, Stage III offline and loopback qualification, and Stage IV storage and integration tests. Public Stage III probes run on Ubuntu 22.04 only after manual Owner dispatch. [Dependencies](DEPENDENCIES.md) · [Beta baseline](BETA.md)

The Stage V security and end-to-end workflow runs isolated checks for C++, WebAdmin, Chromium, and the MariaDB-backed public gateway on all three Ubuntu images. Pull requests with relevant source, configuration, or test changes use the PR head. Pushes to `main` qualify the resulting main commit. The reports include a source SHA and a SHA-256 manifest. [Stage V workflow](../../.github/workflows/stage5-security-e2e.yml)

Concurrency uses one group per workflow and ref with `cancel-in-progress: false`. The application job has a 120-minute timeout and sanitizer jobs have 90 minutes. Each run qualifies its recorded source SHA. [Current GitHub Actions results](https://github.com/adrianf87/dc24h.eu/actions)
