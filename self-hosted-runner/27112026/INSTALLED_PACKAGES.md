# Installed Debian packages — November 27, 2026

**Baseline:** exact on-host collector at `2026-11-27T16:42:49Z`, run as `github-runner`.

| Evidence | Observed value |
| --- | --- |
| Current installed package rows (`dpkg-query`) | **1,110** |
| Packages marked manually installed (`apt-mark showmanual`) | **188** |
| Retained structured `dpkg` package events | **992** |
| Collected tool versions | **10** |
| Collector warning lines | **0** |
| Original inventory snapshot | `/home/github-runner/self-hosted-runner/inventory/20261127T164249Z` |
| Earlier Git commit in deleted repository | `2b753ec33bf62d60e5fa3e9817ae270ed17f9f98` |

**Full 1,110 name/version rows are not currently available in this new repo.** The previous remote repository was deleted. The original snapshot was already collected on the VPS; [copy it from the local clone](README.md#import-the-exact-on-host-inventory) and verify `SHA256SUMS`. Do not substitute guessed packages or treat the `screenfetch` count as a list.

Once imported, the exact names and versions will be in:
`inventory/20261127T164249Z/installed-packages.tsv`, with explicitly installed packages in `apt-manual.txt`.

## Previously sampled tool versions

`git 2.47.3`, `bash 5.2.37`, `python3 3.13.5`, `node 20.19.2`, `npm 9.2.0`, `cmake 3.31.6`, `g++ 14.2.0`, `msmtp 1.8.28`, `mariadb client 11.8.9`, `openssl 3.5.9`.

Tools not reported by the collector (e.g. Docker/Podman) must be checked with `dpkg-query` before classifying them as absent or removed.

## Repeatable capture

From the project checkout:

```bash
bash self-hosted-runner/scripts/collect-inventory.sh
```

Review reports before committing. The collector does not execute package installs/upgrades or access application secrets.
