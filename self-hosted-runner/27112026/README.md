# Active runner baseline — 27 November 2026

**Snapshot date:** 2026-11-27 · **host:** `server-747585786` · **source:** OWNER console output plus the validated GitHub inventory reported before deletion of the old inventory repository.

| Record | Contents |
| --- | --- |
| [System and software](SYSTEM.md) | OS, CPU, kernel, RAM, storage, tool and service versions |
| [GitHub Actions runner](RUNNER.md) | Registration, access model, safeguards, CI migration status |
| [Installed packages](INSTALLED_PACKAGES.md) | Verified counts, sampled versions, exact snapshot provenance |
| [Removed packages](REMOVED_PACKAGES.md) | Retained `dpkg` event evidence and limits |
| [Security](SECURITY.md) | Passwordless `sudo` risk and safe inventory constraints |
| [CI workflows](CI_WORKFLOWS.md) | Current runners and Debian 13 migration gaps |
| [Change log](CHANGELOG.md) | Dated runner configuration and package changes |
| [Runner-specific tests](../tests/README.md) | Offline validation and manual read-only VPS diagnostics |
| [Inventory subdirectory](inventory/README.md) | Full package files once imported from the local VPS |

## Previous verified inventory

- Collector timestamp: **`2026-11-27T16:42:49Z`**
- VPS path at time of collection: **`/home/github-runner/self-hosted-runner/inventory/20261127T164249Z`**
- Original inventory commit (now-deleted repository): **`2b753ec33bf62d60e5fa3e9817ae270ed17f9f98`** (historical reference, not a currently accessible link).
- 8 files: `SHA256SUMS`, `apt-manual.txt`, `dpkg-package-events.tsv`, `installed-packages.tsv`, `inventory-warnings.txt`, `removed-not-installed.tsv`, `system.txt`, `tool-versions.tsv`.
- No warnings reported by the collector.

## Import the exact on-host inventory

The old GitHub repository was deleted, **but the local clone may still exist on the VPS**. On `server-747585786`, from the project checkout **after switching to this feature branch**, check for the files:

```bash
test -f /home/github-runner/self-hosted-runner/inventory/20261127T164249Z/SHA256SUMS
```

If the check succeeds, import without reading service secrets:

```bash
mkdir -p self-hosted-runner/27112026/inventory
cp -a /home/github-runner/self-hosted-runner/inventory/20261127T164249Z self-hosted-runner/27112026/inventory/
(
  cd self-hosted-runner/27112026/inventory/20261127T164249Z
  sha256sum -c SHA256SUMS
)
```

Review the copied files and ensure `git status --short` shows only intended inventory additions before committing:

```bash
git add self-hosted-runner/27112026/inventory/20261127T164249Z
git commit -m "inventory: import verified Debian 13 runner snapshot"
git push origin feature/stage4-webadmin-dedicated-service
```

**Do not run these commands from the old deleted-repository clone.** The working directory must be the `dc24h.eu-twilight` checkout on the target branch.

If the local clone is unavailable, generate a fresh report using `bash self-hosted-runner/scripts/collect-inventory.sh` and note that its timestamp and package counts may differ. Do not invent historical package names.
