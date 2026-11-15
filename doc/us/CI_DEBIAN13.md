# Self-hosted Proxmox CI

The alpha-0.7.5 caller `.github/workflows/build.yml` uses the reusable [VM workflow](https://github.com/adrianf87/dc24h.eu/blob/20ca46bd5f297a98bbbbe59604bcb0a041839b6d/.github/workflows/proxmox-vm.yml) from **github-runner-v0.1**, pinned at `20ca46bd5f297a98bbbbe59604bcb0a041839b6d`. The development branch was created from main; no merge, cherry-pick or parent from the runner branch is added. The source runner branch remains unchanged.

| Runner label | Responsibility |
| --- | --- |
| `proxmox-worker-1` | Prepare and clean disposable clones using the installed host controller |
| `proxmox-worker-2` | VM 200, cloned from stopped VM 100; Ubuntu 24.04 |
| `proxmox-worker-3` | VM 201, cloned from stopped VM 101; Ubuntu 22.04 with G++ 12 |

Every job uses self-hosted Linux X64 runners. The complete VM 200 cycle finishes, including deletion, before VM 201 starts. Shared concurrency group `dc24h-proxmox-vms-200-201` serializes this branch with the infrastructure branch; `cancel-in-progress: false` lets active cleanup finish. Existing source-branch runs can delay this branch in the queue. Source VM packages stay unchanged; VMs 300/301 are outside this workflow.

The provision job checks out only the exact guest helper snapshot at `proxmox-worker/scripts/guest/runner_dependencies.py`, compares it with the root-owned installed helper and installs dependencies inside the matching marked clone through the restricted `sudo -n /usr/local/sbin/dc24h-ci-dependencies install all` command. This is the only privileged provisioning action; application builds/tests run without sudo. The helper refuses package installation in source VMs or the wrong OS/DMI serial. No passwords, registration tokens, new generic sudo rights or infrastructure installers are added here.

After provisioning, each Ubuntu version runs C++20 Debug and Release with required MariaDB/Apache/ncdc CTest integrations, then WebAdmin build/typecheck, Node, Chromium and real C++ API/React integration. Build/CTest/Node/Playwright concurrency is four. Application checkouts exclude `proxmox-worker`; source archives also omit it using `.gitattributes`. Ordinary clones require explicit sparse checkout to omit tracked files.

The runner branch's installed host controller and guest bootstrap remain prerequisites. See the pinned [infrastructure instructions](https://github.com/adrianf87/dc24h.eu/blob/20ca46bd5f297a98bbbbe59604bcb0a041839b6d/proxmox-worker/README.md) for setup, clone ownership, leases, six-hour reaper, recovery and rerun rules. Do not start a source VM together with its clone, bypass ownership checks or replay only later jobs after cleanup: use **Re-run all jobs**. Cleanup reports failure if provisioning/tests/deletion fail. Fork PRs are excluded before any infrastructure action.

Pushes and PRs for `alpha-0.7.5` and `main`, plus manual dispatch, trigger the caller. The main filter takes effect only after a separately authorized merge; this increment does not merge main. Validate locally with:

```sh
actionlint -config-file .github/actionlint.yaml .github/workflows/build.yml
python3 -m py_compile proxmox-worker/scripts/guest/runner_dependencies.py
```

Local application test results are in [Testing](TESTING.md); GitHub Actions supplies the actual remote runner outcome. The old `server-747585786` runner is no longer targeted; its deleted GitHub registration is not recreated.
