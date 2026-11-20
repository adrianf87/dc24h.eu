# WebAdmin Management

alpha-0.8.13 adds a maintenance catalog of account, role and ban areas and a bilingual WebAdmin page that links to the existing flows. Create the branch from main.

`GET /api/v1/management` is served by the maintenance process. Any live `X-DC24H-Admin-Grant` can read it, the same discovery rule as `GET /api/v1/admin`. The path is exact. The hub process does not publish this route. Schema v9 is unchanged. The response does not include the actor id, the grant, account names or ban subjects.

The body is schema 1. `maintenance` and `restart_required` are true. `areas` has three objects, in order: `accounts` with permission `accounts.manage`, `roles` with permission `roles.manage`, and `bans` with permission `users.ban+blocks.manage`. Each object has boolean `allowed`.

`allowed` is computed on the server from the actor loaded for that grant and the default role table. A configured VIP mask is not applied. An operator can open bans and cannot open account or role writes. An administrator can open all three. A moderator can read the catalog and sees all three denied. The twelve grant route templates stay unchanged. `configuration.manage` and `hub.manage` stay unimplemented.

The Management page shows those three areas and links to Accounts, the read-only Roles catalog, and Bans. It does not submit those forms. Account, role and ban writes stay on those pages and still require a stopped hub, a fresh password proof and the current revision where that page already requires them. Password change stays on Accounts and is not one of these three areas. Owner bootstrap and transfer stay on the local owner tool. Configuration writes, live hub administration and browser kick, mute or unmute stay outside this page. `+kick`, `+mute` and `+unmute` still run from an authenticated Direct Connect client.

A missing grant, a hub API or a malformed catalog shows the page as unavailable instead of an empty allow-list. The grant remains in page memory and is not written to web storage.

Check status for this branch uses GitHub-hosted Ubuntu 24.04 runners. Each job checks four logical CPUs and uses `nproc` workers. Remote completion is not awaited.

No production deployment, schema migration or account mutation is part of this increment.
