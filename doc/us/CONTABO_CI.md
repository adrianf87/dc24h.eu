# Contabo CI

The branch's [Check status](../../.github/workflows/build.yml) workflow uses
only the existing self-hosted Ubuntu 24.04 runner `server-747585786` (ID 27).
Its required labels are `self-hosted`, `Linux`, `X64`, `server-747585786` and
`ubuntu-24-04`. The unique server label must remain exclusive to that runner.
There is no GitHub-hosted fallback. Offline Contabo jobs wait in the queue.

Pushes and same-repository pull requests to this branch trigger the workflow;
manual dispatch is also supported. Checkout tests the triggering commit.
The host, runner name/environment, x86_64 architecture, Ubuntu version and
non-root `github-runner` account are checked before tests. Fork pull requests
are skipped. The workflow requests only repository read permission and does not
persist checkout credentials.

One job runs C++ Debug and Release builds and CTest, then WebAdmin typecheck,
production build, Node tests, Chromium tests and real C++ API integration.
Build/CTest parallelism, Node test concurrency and Playwright workers are all
**6**. Integration-tool requirements stay enabled, including ncdc ADC/ADCS,
private MariaDB and Apache checks. No supported OS beyond Ubuntu 24.04 is
claimed tested by this workflow.

Concurrency uses `dc24h-contabo-server-747585786-${{ github.ref }}` so separate
branches do not replace one another's pending run. The single Contabo runner
serializes jobs. Within a branch, GitHub retains one pending run by default.
System packages are provisioned administratively through
`contabo/scripts/dependencies.py`; CI only checks them and never uses sudo.
Node 22 and user-local Chromium are handled by the workflow. See
[worker setup](../../contabo/README.md). Do not store passwords, registration
tokens or `.credentials*` files in the repository.

The previous Proxmox workflow is replaced on this branch. Other branches are
independent and are changed only when requested. Existing historical workflow
runs and their results are retained by GitHub; removing a workflow file does
not erase history or cancel previously queued runs.

CI is launched after publishing commits. The requested workflow is not polled
to completion during this task: a queued/running job is not reported as passed.

Checkout is pinned to v5 `fbc6f3992d24b796d5a048ff273f7fcc4a7b6c09` to use direct temporary HTTP-header authentication following v6 fetch failures on the persistent runner. `persist-credentials: false` removes authentication after checkout. No PAT or increased permission is needed. The new fetch must be verified by CI; this configuration change does not claim completed tests.
