# Self-hosted GitHub Actions runner

This directory is the new **in-repository home** for all inventory, configuration facts, installed/removed package history, and operational documentation for the Debian 13 runner. The previously separate `adrianf87/self-hosted-runner` repository was deleted by the OWNER. Do not rely on that old remote.

- Registered runner: **`server-747585786`**
- Runner settings: [repository runner #22](https://github.com/adrianf87/dc24h.eu-twilight/settings/actions/runners/22)
- OS: **Debian 13 (Trixie)**; OWNER's future CI target
- System account: `github-runner`, with passwordless `sudo` (OWNER-confirmed)
- Host: `server-747585786.dc24h.eu` / `62.171.156.32`
- Lives on a **shared public-service VPS**, also running the public ADCS hub and WebAdmin

## Dated runner records

- [27 November 2026 — active runner](27112026/README.md)
- [Read-only inventory collector](scripts/collect-inventory.sh)

Create a **new dated directory in DDMMYYYY format** when the runner is replaced or a later baseline is taken. Keep older snapshots for comparison and record actual installs, upgrades and removals; never rewrite a historic inventory.

## Source and confidence

The 27 November 2026 baseline was supplied through OWNER shell output. A package inventory previously generated on the VPS at `2026-11-27T16:42:49Z` was confirmed uploaded to the now-deleted repository before deletion: 1,110 installed packages, 188 marked manually installed, 992 retained `dpkg` package events, 18 remove/purge rows for currently absent packages, and an empty warning file.

**The complete package-by-package files are NOT yet copied into this repository.** They should be imported from the local clone still on the VPS if present. See [import instructions](27112026/README.md#import-the-exact-on-host-inventory).

## CI migration status

OWNER policy is **Debian 13 only** for future testing. Existing project workflow files previously still referenced Ubuntu 22.04/24.04/26.04, and must be deliberately migrated in a reviewed change. Adding this directory does not register a runner, change job routing, or launch tests. Do not automatically run PR builds or dependencies with passwordless `sudo` on this live host.

## Sensitive data policy

Never commit runner `.credentials*`, registration/PAT tokens, passwords, `/etc/dc24h`, SMTP configurations, TLS private keys, session cookies, database dumps, raw process environments, or shell/command histories. Capture only sanitized package metadata, high-level service versions, non-secret runner facts and verified status changes. Review each generated inventory before `git add`.
