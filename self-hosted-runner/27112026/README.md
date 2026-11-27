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

## Recovery after local inventory deletion

The OWNER deleted `/home/github-runner/self-hosted-runner` with `rm -rf` on November 27, 2026. The former separate GitHub repository had already been deleted. The original eight files from `20261127T164249Z` are therefore **not available through those paths**. Empty directories recreated afterward do **not** restore the files: do not copy or commit them as historical evidence, and do not add a replacement `SHA256SUMS` to the empty historical timestamp.

**Create a fresh, accurately timestamped inventory in a checkout of the current project.** As `github-runner` on the approved VPS, clone the target feature branch if you do not already have a project checkout:

```bash
cd /home/github-runner
git clone --branch feature/stage4-webadmin-dedicated-service --single-branch \
  https://github.com/adrianf87/dc24h.eu-twilight.git dc24h-stage4-ci
cd dc24h-stage4-ci
```

If a project checkout exists instead, use that directory and update it to the same feature branch without discarding unrelated edits. Do **not** run the collector in the separate, empty `~/self-hosted-runner` directory.

```bash
git branch --show-current
bash self-hosted-runner/scripts/collect-inventory.sh
bash self-hosted-runner/scripts/run-tests.sh offline
```

The collector prints the **new** timestamped report path. To verify the newest snapshot in this date folder:

```bash
snapshot_dir="$(find self-hosted-runner/27112026/inventory -mindepth 1 -maxdepth 1 \
  -type d -name '20*T*Z' | sort | tail -n 1)"
test -n "$snapshot_dir"
( cd "$snapshot_dir" && sha256sum -c SHA256SUMS )
```

Review the report and `git status --short` before recording it:

```bash
git add self-hosted-runner/27112026/inventory/
git commit -m "inventory: recollect Debian 13 runner after local deletion"
git push origin feature/stage4-webadmin-dedicated-service
```

This new report may have the same package counts if the host has not changed, but it is **not** the original snapshot. The earlier `20261127T164249Z` counts remain independently observed historical facts; only backups or retained logs could recover missing original file contents.
