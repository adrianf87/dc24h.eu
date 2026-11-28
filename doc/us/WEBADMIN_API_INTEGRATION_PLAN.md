# WebAdmin secure API integration

Status **implemented in Draft PR #30, local validation complete**. The branch integrates the Stage IV OTP gateway with the account service and an optional read-only connection to the serving hub. The protected branch and running deployment require a separate review and rollout.

## Route and permission inventory

| Public WebAdmin operation | Internal source | Authorization and behavior |
| --- | --- | --- |
| Login, enrollment, session, logout, own password, revoke sessions | Stage IV gateway and C++ `POST /api/v1/admin/access` for password proof | Password plus email OTP. Writes use the applicable login, enrollment or session CSRF token. |
| `GET identity-summary` | Account service `GET /api/v1/stage4/identity-summary` | Owner or Administrator. Aggregate counts, without account names or email addresses. |
| `GET hub/health`, `hub/ready`, `hub/status`, `hub/users` | Serving hub `GET /api/v1/{health,ready,status,users}` through four exact bridge mappings | Owner or Administrator. Live dashboard, process/connection telemetry, effective startup configuration, roles, moderation rules, chat, search, transfer, kick and mute counters. Directory omits session IDs and SIDs. |
| `POST admin` with `management` or `admin` | Account service, native role catalog and administration counters | Owner or Administrator with CSRF and current server-side session. |
| `POST admin` with `accounts.list`, `accounts.staff`, `accounts.detail` | Native account directory and staff filtering | `accounts.manage`. Bounded cursor or exact record ID. |
| `POST admin` with `accounts.create`, `accounts.activate`, `accounts.disable`, `accounts.reset`, `accounts.role` | Native account engine and transactional audit | `accounts.manage`, plus `roles.manage` for role changes. Current password and expected target revision are required for mutations of existing accounts. Native rank rules protect self, equal and higher ranks. Creation produces a disabled account without email enrollment. Reset and role changes follow native status rules. |
| `POST admin` with `bans.list`, `bans.create`, `bans.revoke` | Native ban directory and transactional audit | `users.ban` for list, plus `blocks.manage` and a current password for writes. Creation supports permanent, relative and absolute expiry. Revoke uses the expected revision. The hub enforces a stored ban only when its database and active ban snapshot include that record. |
| `POST admin` with `audit.list` | Native bounded audit directory | `audit.read` and current password. Returns event identifiers, timestamps, actions and outcomes. |

Public paths in the first column are relative to `/stage4/v1/`. The browser sends a fixed operation and typed fields to the single administrative endpoint. Node validates fields, CSRF and rate limits, derives the session digest from the server-read cookie and rechecks the session after the call. The account process validates that digest and the current verified account from MariaDB. C++ checks the native permission before dispatch and uses actor revision, password proof and transactional checks for writes. Apache and both adapters keep finite path/method allowlists. Browser inputs never select an upstream request path or supply an actor ID, bearer token or C++ grant. On an uncertain write outcome, refresh the record before retrying.

## Serving hub read connection

The new bridge mappings use `/run/dc24h-stage3-api/hub.sock` and `/var/lib/dc24h-stage3/api.token`. To provide the optional source in a reviewed rollout, configure the hub's existing `[api]` listener with those paths. Let the hub unit create `RuntimeDirectory=dc24h-stage3-api` with mode `0700`. Provision an independently generated token owned by the hub UID, mode `0600`, and validate the configuration as that UID. The bridge checks socket and token permissions under the hub UID and refuses administrative headers on live reads. Missing or unsafe material leaves telemetry unavailable while the account panel remains operational.

The upgraded account binary, gateway, assets, bridge and Apache vhost must be deployed together. During the approved Stage IV maintenance window, rerun the protected initializer to reconcile the account service's socket-only, table-scoped privileges, then validate the stack before opening ingress. The branch work did not change the active VPS configuration or services.

## Private tools and acceptance

- Configuration review of saved values, validation of candidate changes, configuration write and restore remain in the private maintenance UI and API. The public panel displays effective startup settings from the live hub.
- The controller's service status and start/stop/restart, owner bootstrap/transfer, C++ grant issuance and raw administrative routes remain private. The public health and readiness probes use the serving hub.
- Live kick and mute actions use the authorized DC client commands. The panel exposes their counters and rule metadata. No existing HTTP kick/mute mutation contract is available.
- PR #30 adds isolated C++/MariaDB tests for account mutations, pagination, bans and audit, Unix bridge route and credential tests, TypeScript projections and Chromium coverage for the OTP-gated React account view. Local tests used the Ubuntu 24.04 VPS and disposable state. Hosted CI and release/production approval are tracked separately.
