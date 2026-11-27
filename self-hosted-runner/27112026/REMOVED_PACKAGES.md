# Removed / purged packages — November 27, 2026

The prior on-host snapshot contained:

- **992** retained `dpkg` install/upgrade/remove/purge events.
- **18** logged `remove` or `purge` rows concerning packages **not installed** when the collector ran.
- **0** warning lines about collecting the available package history.

The previous GitHub repository and its local VPS clone were both deleted; the former 18 rows cannot be imported from that location. [Recollect a new dated inventory](README.md#recovery-after-local-inventory-deletion) from the retained `dpkg` logs. Its removal event count may differ if logs rotated or packages changed. Do **not** invent names or versions.

After import:

| File | Role |
| --- | --- |
| `inventory/<new_utc_timestamp>/dpkg-package-events.tsv` | Dated `install`, `upgrade`, `remove`, `purge` events retained in `/var/log/dpkg.log*` |
| `inventory/20261127T164249Z/removed-not-installed.tsv` | Events concerning packages that are currently absent |
| `inventory/20261127T164249Z/inventory-warnings.txt` | Missing or unreadable source warnings |
| `inventory/20261127T164249Z/SHA256SUMS` | Integrity checks |

**Interpretation:** 18 are removal/purge **event rows**, not necessarily 18 distinct packages. Log rotation may have erased older events. Packages may be removed and subsequently reinstalled; event history cannot be equated with an all-time inventory. Preserve the source timestamps and avoid relying on incomplete historical reconstruction.

For future removals, append a dated change record with exact package, version, reason and before/after snapshots. Never commit raw `apt/history.log` command histories or user shell history, which could include private arguments.
