# Timestamped package inventories

Store reviewed and sanitized Debian 13 inventory snapshots here, one folder per UTC timestamp.

The first verified snapshot was collected on the VPS before the old separate GitHub repository was deleted:

`/home/github-runner/self-hosted-runner/inventory/20261127T164249Z`

The OWNER later deleted that local clone as well as the old GitHub repository. Newly created empty directories are not evidence. **Do not copy a vacant directory or invent the former checksum file.** Follow the [fresh collection instructions](../README.md#recovery-after-local-inventory-deletion) from a checkout of `dc24h.eu-twilight`. Each new report uses a new UTC timestamp.

The full 1,110 package names/versions and 18 removed/purged event rows have **not yet been imported** into this repository. No historical package file should be reconstructed from guessed data.

Fresh snapshots can also be generated with:

```bash
bash self-hosted-runner/scripts/collect-inventory.sh
```

The script determines the DDMMYYYY date folder from the host local calendar date, and uses a UTC timestamp for the inventory name. Review every report before committing; do not commit credentials, shell history, application configurations or raw private logs.
