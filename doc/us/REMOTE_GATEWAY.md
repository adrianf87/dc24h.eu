# Remote WebAdmin — beta-0.2 (local-only read access)

**Development-only.** The beta-0.2 gateway is an **independent** WebAdmin entry
point: /remote.html and five precisely defined /remote/v1 routes. It is
**local-only** on 127.0.0.1 and requires HTTPS, an active **administrator**
or **Owner** account and a valid account password. There is **no MFA** in this
release; exposing this gateway on a public network, through a forwarding proxy
or an Internet-facing tunnel is **not supported**. Nothing is deployed or
enabled by the build. The existing public Apache configuration is unchanged.

## Scope and trust boundaries

Browser (loopback HTTPS) → dedicated Apache vhost (127.0.0.1:8443) →
separate Node gateway (127.0.0.1:4182) → private C++ API Unix socket.
The gateway uses its own server-side service token from a private file,
never sends it to the browser and never accepts a browser process token,
maintenance grant or forwarded identity. The account password is accepted
only on login, checked by the existing C++ password-proof route and not saved
in the gateway session. The resulting grant, API session and API CSRF are
held in server memory only; the browser gets an independent random opaque
session cookie (Secure, HttpOnly, SameSite=Strict, scoped to /remote/v1).

The gateway rechecks the live authenticated management catalog on **every**
session or summary read. An inactive account, revoked grant or reduced role
fails closed. Upstream timeouts/errors cause 503 with no cached metrics.
Sessions expire after at most 10 minutes, 2 minutes idle; at most four
concurrent sessions, short one-time login challenges and rate-limited login
attempts. Logout requires same-origin Origin and an independent CSRF secret.
Audit events contain only fixed event names (login_ok, login_refused, logout,
session_revoked, route_refused, api_unavailable), never passwords, session
secrets, IP addresses or response payloads.

### Exactly allowed endpoints

| Route | Method | Effect |
| --- | --- | --- |
| /remote/v1/challenge | GET | Short-lived login CSRF challenge; no hub data |
| /remote/v1/login | POST | Authenticated password proof and role recheck |
| /remote/v1/session | GET | Revalidate current account/role, return session status |
| /remote/v1/summary | GET | Revalidate, return explicit aggregate projection |
| /remote/v1/logout | POST | CSRF-protected session invalidation |

The summary includes only version, hub-running state, uptime, aggregate
connection/user/chat counters and optional aggregate search/transfer counts.
No account list, nicknames, peer IP/CID, credentials, configuration, audit
rows, raw API data or administrative command response is exposed. There
is **no writable API**: no users, roles, bans, config, Start/Stop/Restart,
systemd, SQL, migrations or ADC/ADCS changes. A login is not a grant to the
existing local WebAdmin or its maintenance routes.

## Optional isolated local setup (not automatic)

1. Build the existing UI with Node.js as in webadmin/README.md:
   run npm ci and npm run build inside webadmin/. The output includes
   remote.html alongside the ordinary index.html.
2. Provide the existing **private** DC24H_API_SOCKET and
   DC24H_API_TOKEN_FILE for the independent maintenance API, readable by
   the selected gateway user. Keep credentials outside the WebAdmin root,
   HTTP headers, Vite variables and Git.
3. Set DC24H_REMOTE_ORIGIN=https://localhost:8443. Run
   npm run remote:local inside webadmin/ with those server-side variables.
   The listener is hard-coded to 127.0.0.1:4182. Public bind switches are
   rejected. Review config/dc24h-remote-gateway.service.example if a
   separately managed unit is desired; it is only a template.
4. Review config/apache2-webadmin-remote.conf.example, install a trusted
   **local** HTTPS certificate and key outside the web root, enable
   mod_ssl, mod_headers, mod_proxy and mod_proxy_http, and start a
   dedicated localhost-only vhost. Its origin must match the gateway.
   Load this vhost only after independently checking your Apache layout.
5. Visit https://localhost:8443/remote.html **on the same machine**.
   Use the numeric account ID and account password for an active
   administrator or Owner. Switch EN/PL at the top; Logout invalidates
   the session. The old local WebAdmin and its admin actions are separate.

If the local C++ API, account store, HTTPS certificate, loopback listener or
Apache protection is unavailable, stop and correct that component. Do not
weaken the bind or proxy an unprotected port to make the feature reachable.

## Tests / known limitations

- Node gateway tests cover proof and account-role revalidation, CSRF,
  malformed input, denied routes/origins/headers, expiry and status projection.
- Browser tests cover EN/PL UI, login, summary, logout and refusal.
- Apache integration launches a disposable localhost HTTPS server and checks
  the exact allowlist, secret header stripping, private-file denial and TLS.
- CTest checks the static configuration, documentation, metadata and unchanged
  database migration catalog. The normal C++ and WebAdmin suites remain.

Residual risks: localhost users/processes with access to the socket or token,
compromised browsers or servers, stolen account passwords and local TLS trust
configuration. Rate limits are **per gateway process** and reset on restart;
they are not a distributed identity defense. The gateway has **no MFA and no
Internet exposure mode**. A localhost-only implementation is not a secure
public-access deployment. Schema v11 and package tuple 0.1.0 are unchanged.
