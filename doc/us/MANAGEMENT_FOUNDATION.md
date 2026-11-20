# Management foundation — beta-0.1.1

beta-0.1.1 prepares the administrative contract for configuration and hub operations. It starts from main `075081f`. The branch name is `beta-0.1.1`.

The hub, the API and WebAdmin report `beta-0.1.1`. The CMake project number and the WebAdmin package number stay `0.1.0`. Schema v9 is unchanged. This is a developmental Beta. This is not a production release. No residual threat is closed.

`configuration.manage` and `hub.manage` stay unimplemented. `permission_implemented` stays false for both. This increment does not read a configuration file, write an INI file, restore a backup, start a process, stop a process, or call systemd. A later version implements those engines.

## Catalog

`GET /api/v1/management` stays a discovery read. Any live grant can read it when the same slot also presents `X-DC24H-Session` and `X-DC24H-Csrf`. The path is exact. A query string is not this route. The response has no actor id, grant, account name, ban subject, password, or token.

The body is schema 2. `maintenance` and `restart_required` are true. `areas` has five objects, in this order:

1. `accounts`, permission `accounts.manage`, implemented true.
2. `roles`, permission `roles.manage`, implemented true.
3. `bans`, permission `users.ban+blocks.manage`, implemented true.
4. `configuration`, permission `configuration.manage`, implemented false.
5. `hub`, permission `hub.manage`, implemented false.

`allowed` is computed on the server from the actor loaded for that grant and the default role table. A configured VIP mask is not applied. An owner sees configuration and hub allowed and unimplemented. An administrator sees configuration allowed and hub denied. An operator sees both denied. The twelve grant route templates stay unchanged and do not list these operations.

## Authorize contract

`POST /api/v1/management/authorize` is the only new route. It is not one of the twelve templates. The maintenance process requires the bearer token, a live grant, `X-DC24H-Session`, and `X-DC24H-Csrf` from that same slot. The gate accepts any live grant so the handler can tell a missing permission from a missing grant. The handler then checks the operation permission. A missing, expired, or unknown grant is `401` `admin_grant_required`. A missing or mismatched session or CSRF secret is `403` `csrf_required`. `GET` on this path is `405` with `Allow: POST`. `POST /api/v1/management` stays `405` with `Allow: GET`.

The form has exactly one field, `operation`. Any other field, including `path`, `command`, `service`, and `file`, is `400` `invalid_input`. The value must be one of these names:

- `configuration.read`, `configuration.validate`, `configuration.write`, and `configuration.restore` require `configuration.manage`.
- `hub.status`, `hub.start`, `hub.stop`, and `hub.restart` require `hub.manage`.

An unknown name is `400` `invalid_input`. A known name without the permission is `403` `management_refused`. A known name with the permission is `501` `not_implemented`. The response does not echo the grant, the password, or a command. The handler does not open a file and does not spawn a process.

Status and health stay bearer-only. `Require local` stays. Browser kick and mute stay undispatched.

## WebAdmin and the proxy

The Management page lists the five areas. Configuration and hub show that they are not implemented. The page has no configuration form, no password field for those areas, and no Start, Stop, or Restart button. Account, role, and ban links stay on the implemented areas. A catalog that marks configuration or hub implemented is rejected and the page stays unavailable.

Apache forwards `POST /admin-api/v1/management/authorize` and still refuses `POST /admin-api/v1/management`, a longer suffix, and `GET` on the authorize path. The loopback adapter uses the same exact path. Client secrets stay stripped. See [WebAdmin Management](WEBADMIN_MANAGEMENT.md) and [Admin API](ADMIN_API.md).

## Not in this version

Configuration read, the saved-versus-active distinction, validation of a proposed file, conflict detection, atomic INI write, backup, restore, file permission checks, audit rows for these operations, schema v10, the WebAdmin editor, an independent service controller, and Start, Stop, and Restart execution belong to later versions.
