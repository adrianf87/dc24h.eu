# WebAdmin Management

alpha-0.8.13 adds a maintenance catalog of account, role and ban areas and a bilingual WebAdmin page that links to the existing flows. Create the branch from main.

`GET /api/v1/management` is served by the maintenance process. Any live `X-DC24H-Admin-Grant` can read it, the same discovery rule as `GET /api/v1/admin`. The path is exact. The hub process does not publish this route. Schema v9 is unchanged. The response does not include the actor id, the grant, account names or ban subjects.

alpha-0.8.13 used schema 1 with three areas: `accounts`, `roles` and `bans`. beta-0.1.1 uses schema 2. `maintenance` and `restart_required` are true. `areas` has five objects, in order: `accounts` with `accounts.manage`, `roles` with `roles.manage`, `bans` with `users.ban+blocks.manage`, `configuration` with `configuration.manage`, and `hub` with `hub.manage`. Each object has boolean `allowed` and boolean `implemented`. The first three are implemented. Configuration and hub are not.

`allowed` is computed on the server from the actor loaded for that grant and the default role table. A configured VIP mask is not applied. An operator can open bans and cannot open account, role, configuration or hub operations. An administrator can open the first three and configuration, and cannot open hub. An owner can open configuration and hub, and both stay unimplemented. A moderator can read the catalog and sees the write areas denied. The twelve grant route templates stay unchanged. See [Management foundation](MANAGEMENT_FOUNDATION.md).

The Management page shows the five areas. It links to Accounts, the read-only Roles catalog, and Bans. It does not submit those forms. Configuration and hub show that they are not implemented. The page has no configuration form and no Start, Stop or Restart button. Account, role and ban writes stay on those pages and still require a stopped hub, a fresh password proof and the current revision where that page already requires them. Password change stays on Accounts. Owner bootstrap and transfer stay on the local owner tool. Browser kick, mute or unmute stay outside this page. `+kick`, `+mute` and `+unmute` still run from an authenticated Direct Connect client.

A missing grant, a hub API or a malformed catalog shows the page as unavailable instead of an empty allow-list. The grant remains in page memory and is not written to web storage.

Check status for this branch uses GitHub-hosted Ubuntu 24.04 runners. Each job checks four logical CPUs and uses `nproc` workers. Remote completion is not awaited.

No production deployment, schema migration or account mutation is part of this historical alpha-0.8.13 increment.

## Current behavior after beta-0.1.4

The five-area catalog and its `implemented` flags still describe the original broad management contract. The `configuration` area therefore remains `implemented: false` for that contract, even though a separate, narrowly authorized four-setting editor is now available from the Configuration page for Administrator/Owner grants. The Management page links to that editor only when the grant allows `configuration.manage`; it does not submit configuration changes itself. The hub area remains unimplemented, with no service Start/Stop/Restart controls. See [Configuration editor](CONFIGURATION_EDITOR.md) and [Management foundation](MANAGEMENT_FOUNDATION.md). Schema v10 belongs to the later configuration audit, not to the original catalog release.
