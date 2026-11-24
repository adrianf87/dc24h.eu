# dc24h.eu WebAdmin

The current development UI is `beta-0.5.12` (dc24h.eu High Starter). Stage III adds a private gateway startup/restart resource budget and strict offline recovery catalog checks; the remote page shows estimated session expiry, passkey creation/last-use, explicit revoke confirmation and an outage state that never displays stale device controls ([EN/US](../doc/us/BETA_0.5_STAGE_III.md), [PL](../doc/pl/BETA_0.5_STAGE_III.md)). Stage II preserves TOTP replay, passkeys, rate-limit and audit history across failures ([EN/US](../doc/us/BETA_0.5_STAGE_II.md), [PL](../doc/pl/BETA_0.5_STAGE_II.md)). Stage I supplied GitHub-hosted Ubuntu 22.04/24.04/26.04 CI and synthetic MariaDB/private-state recovery ([EN/US](../doc/us/BETA_0.5_STAGE_I.md), [PL](../doc/pl/BETA_0.5_STAGE_I.md)). Debian 12/13 and target Xeon tests are outside this issue's CI scope. Browser sessions remain protected, authentication is **password + passkey** or **password + TOTP**, not passwordless. The local Owner-only service controls remain in [Hub WebAdmin](../doc/us/HUB_WEBADMIN.md). The public Apache example and remote gateway forward no service-control writes. Public exposure and production deployment remain disabled.

alpha-0.8.1: see [Permissions](../doc/us/PERMISSIONS.md) for the seven-role catalog, pending-action flags, safe VIP delegation and stopped-hub role changes.

Account maintenance in alpha-0.7.14: see [WebAdmin Accounts](../doc/us/WEBADMIN_ACCOUNTS.md). Use the separate non-serving maintenance executable; the ordinary hub API remains read-only.

React + TypeScript + Vite, with a responsive EN/US and PL read-only panel.
Unavailable data is shown explicitly; no fake live metrics or browser token storage.

The Chat navigation item (`#chat`) shows all nine aggregate chat counters from the
existing authenticated API, with US/PL labels and separate message/recipient queue
sections. Overview keeps its four-counter summary and links to the full page.
Totals are cumulative since server startup and reset on restart. A queued copy
is not a delivery/read receipt. Missing or invalid API data clears values to dashes
and disables export; live zeros remain zeros. See [chat statistics](../doc/us/WEBADMIN_CHAT.md).

```sh
npm ci
npm run build
npm run dev
```

Requires Node.js `^20.19.0 || >=22.12.0`. Default: http://127.0.0.1:5173.
Optional real C++ connection: server-only `DC24H_API_SOCKET` and
`DC24H_API_TOKEN_FILE` absolute paths, same effective UID; keep the token outside
this WebAdmin root. Never use `VITE_*` for credentials.

`npm run preview` serves a static build with API disabled. beta-0.2 adds
an optional independent loopback gateway with server-side sessions:
set private DC24H_API_SOCKET, DC24H_API_TOKEN_FILE,
DC24H_REMOTE_MFA_FILE, DC24H_REMOTE_MFA_REPLAY_FILE,
DC24H_REMOTE_AUDIT_FILE, DC24H_REMOTE_PASSKEY_FILE,
DC24H_REMOTE_RATE_LIMIT_FILE,
DC24H_REMOTE_RP_ID=localhost and DC24H_REMOTE_ORIGIN=https://localhost:8443,
then use npm run remote:local.
Only the separate localhost HTTPS Apache example exposes its fixed /remote/v1
routes. Public authenticated access is **not** enabled; local password+TOTP
and password+passkey are available. Keep the passkey file in the same private
owner-only location as the TOTP/replay/audit files, never the static root.
Production web sessions, public access and remote mutations remain planned.

See [setup and architecture (US)](../doc/us/WEBADMIN.md) or
[instrukcja PL](../doc/pl/WEBADMIN.md) for API connection, Apache2, installation
and verification. Do not expose the Vite development server to a network.

alpha-0.8.2 adds a filtered staff directory with proven maintenance operations; local ownership tooling is documented in [Operators](../doc/us/OPERATORS.md) ([PL](../doc/pl/OPERATORS.md)).

alpha-0.8.3 adds the read-only [Admin Core](../doc/us/ADMIN_CORE.md) ([PL](../doc/pl/ADMIN_CORE.md)) command catalog and process outcomes through the private maintenance adapter.

alpha-0.8.4 adds a read-only [Kick](../doc/us/KICK.md) ([PL](../doc/pl/KICK.md)) statistics page. Issue live kicks from your authenticated Direct Connect client.

alpha-0.8.5 adds private maintenance [Ban Core](../doc/us/BAN_CORE.md) ([PL](../doc/pl/BAN_CORE.md)) create/revoke forms with restart guidance.
