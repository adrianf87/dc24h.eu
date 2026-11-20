# API security — alpha-0.9.7

Permissions, sessions, CSRF and WebAdmin. The branch starts from main `14792f7`. The branch name is `alpha-0.9.7`.

The maintenance process already reloads the stored role on every accepted call. Each in-memory grant already has a public session id and a separate CSRF secret, and a write already required both. This increment adds one fixed rule. It is not optional and it is not an INI key.

## Bound reads

Every maintenance call except `POST /api/v1/admin/access` requires three values from the same slot:

- `X-DC24H-Admin-Grant`: 64 hexadecimal characters
- `X-DC24H-Session`: 32 lowercase hexadecimal characters
- `X-DC24H-Csrf`: 64 hexadecimal characters

The grant, the session id and the CSRF secret are compared in constant time. A missing or different session id or CSRF secret returns 403 `csrf_required`. That refusal does not reload the role and does not return maintenance rows. A header with the wrong shape is still a bad request before the slot is consulted. The password proof does not require these headers, because that response is what issues the three secrets.

The loopback adapter applies the same rule before it opens the Unix socket. A read or a write that presents a grant without both values is refused with `csrf_required` and is not forwarded. A read that presents no grant is still forwarded, so the process can answer `admin_grant_required`. The access proof remains the exception. The adapter still injects the service bearer and still ignores a browser `Authorization` header. It does not log these secrets.

WebAdmin sends all three headers on every maintenance call. They stay in page memory. They are not written to `localStorage`, `sessionStorage` or a cookie. Forget still clears all three together.

Status and health stay bearer-only. Each live session still accepts 24 requests in 10 seconds, and a refused binding still counts. The next request in that window returns 429 `rate_limited`. The process still accepts at most one maintenance POST per second. The budget is not taken from a client address or a proxy header.

## Permissions

The stored role is loaded only after the binding matches. An active account that no longer holds the route permission is still forbidden. Unknown maintenance targets stay forbidden. `configuration.manage` and `hub.manage` stay unimplemented and have no route. Browser kick and mute are still not dispatched. The permission catalog and the twelve maintenance routes are unchanged.

## Later management routes (beta-0.1.1 through beta-0.1.4)

The preceding section documents the original alpha-0.9.7 security boundary. The current maintenance process also has the exact `POST /api/v1/management/authorize` contract from beta-0.1.1. It requires the bound grant and checks `configuration.manage` or `hub.manage` according to the named operation, but returns `501 not_implemented` for an allowed request. It never carries out a configuration change or service command.

Beta-0.1.2 through beta-0.1.4 separately introduce `GET` and `POST /api/v1/management/configuration` and `POST /api/v1/management/configuration/write` and `/restore`. These routes require `configuration.manage` in the C++ authorization gate, in addition to the same bearer, live grant, session and CSRF binding. Only four nonsecret settings are exposed and no caller-supplied filesystem path or shell command is accepted. Write and restore are auditable, atomic operations on the saved INI; they do not apply settings to a running hub. The database schema is now v10 for the configuration audit. `hub.manage` still has no executable service route. The original twelve grant route templates are unchanged. See [Configuration review](CONFIGURATION_REVIEW.md), [Configuration write](CONFIGURATION_WRITE.md), and [Management foundation](MANAGEMENT_FOUNDATION.md).

## Unchanged in alpha-0.9.7

Copying the grant, the session id and the CSRF secret from page memory still authorizes that slot until it expires or the actor replaces it. The proof response still carries the three secrets. This does not close residual threats T04 or T05. Schema v9 is unchanged. No status counter, new WebAdmin page, INI key or Apache setting is added. `Require local` stays. There is no cookie session and no public authenticated gateway. HTTP/2, OCSP and automatic certificate renewal remain unimplemented.

`accounts.webadmin.process` checks that a grant without the session binding returns `csrf_required` and no account rows, that a wrong session id does the same, that a malformed CSRF header is a bad request, and that a bound read still lists the accounts. The adapter test checks that a presented grant without the binding never reaches the Unix socket.
