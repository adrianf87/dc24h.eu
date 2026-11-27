# Timestamped package inventories

Store reviewed and sanitized Debian 13 inventory snapshots here, one folder per UTC timestamp.

The first verified snapshot was collected on the VPS before the old separate GitHub repository was deleted:

`/home/github-runner/self-hosted-runner/inventory/20261127T164249Z`

Copy it here as `20261127T164249Z/` from the **local VPS clone**, then run `sha256sum -c SHA256SUMS` from inside that directory. See [import instructions](../README.md#import-the-exact-on-host-inventory).

The full 1,110 package names/versions and 18 removed/purged event rows have **not yet been imported** into this repository. No historical package file should be reconstructed from guessed data.

Fresh snapshots can also be generated with:

```bash
bash self-hosted-runner/scripts/collect-inventory.sh
```

The script determines the DDMMYYYY date folder from the host local calendar date, and uses a UTC timestamp for the inventory name. Review every report before committing; do not commit credentials, shell history, application configurations or raw private logs.
