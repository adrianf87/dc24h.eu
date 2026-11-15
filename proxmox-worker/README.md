# CI guest dependency snapshot

`scripts/guest/runner_dependencies.py` is an exact snapshot from
`github-runner-v0.1` commit `20ca46bd5f297a98bbbbe59604bcb0a041839b6d`
(Git blob `ad08ad1e6bc058d101464e157696cda27ca7fa11`).
The pinned reusable workflow checks out this file for provisioning and compares
it with the installed root-owned helper. Application build/test checkouts and
source archives exclude this directory.

The runner branch has not been merged. Host controllers, installers and worker
lifecycle implementations remain in the pinned
[infrastructure branch](https://github.com/adrianf87/dc24h.eu/tree/20ca46bd5f297a98bbbbe59604bcb0a041839b6d/proxmox-worker).
See [EN/US CI](../doc/us/CI_DEBIAN13.md) and [PL CI](../doc/pl/CI_DEBIAN13.md).
