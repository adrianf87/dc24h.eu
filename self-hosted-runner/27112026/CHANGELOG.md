# Runner maintenance log

This log tracks verified runner changes and captures the difference between OWNER-supplied observations, on-host inventory, and pending checks. Times in UTC unless stated otherwise.

| Date / time | Action | Evidence / outcome |
| --- | --- | --- |
| 2026-11-27 | Debian 13 VPS identity and hardware baseline recorded. | OWNER `screenfetch`; see [SYSTEM.md](SYSTEM.md). |
| 2026-11-27T16:42:49Z | On-host package collector run as `github-runner`. | 1,110 installed packages, 188 manual, 992 retained package events, 18 removal/purge rows for absent packages, zero warnings. Local path documented in [README](README.md). |
| 2026-11-27 | First inventory pushed to the now-deleted `adrianf87/self-hosted-runner` repository. | Historical commit `2b753ec33bf62d60e5fa3e9817ae270ed17f9f98`; local copy must be imported here. |
| 2026-11-27 | Runner documentation moved into project branch `feature/stage4-webadmin-dedicated-service`. | `self-hosted-runner/27112026/` created. No service or workflow configuration modified. |
| 2026-11-27 | Audited eight existing workflow files. | Ubuntu runner declarations remain; [CI_WORKFLOWS.md](CI_WORKFLOWS.md). |

## Add future entries

For package additions, upgrades or removals, record:

- UTC timestamp, package name and before/after version.
- Reason and approval, especially if change affects public services.
- Before/after inventory snapshots with SHA-256 verification.
- Exact code/workflow commit used and PASS/FAIL test result.

Do not backfill historical package names without an original inventory or retained `dpkg` records.
