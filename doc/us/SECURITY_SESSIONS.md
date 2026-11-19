# Security sessions — alpha-0.8.15

Maintenance access keeps the four in-memory grants from [Admin API](ADMIN_API.md). Each grant now has a public session id and a separate CSRF secret. This branch starts from main `6175458`. The branch name is `alpha-0.8.15`.

Schema v9 is unchanged. Nothing in this increment is written to SQL or to the audit table. `configuration.manage` and `hub.manage` stay unimplemented. Browser kick and mute are still not dispatched. The hub still does not trust proxy headers or `X-Forwarded-For`.

## What a proof returns

`POST /api/v1/admin/access` still requires an active account and its password. The JSON adds `session` and `csrf` beside the existing `grant`.

- `grant` is 64 lowercase hexadecimal characters. It remains the credential for later maintenance calls.
- `session` is 32 lowercase hexadecimal characters. It names the slot. It is not a credential.
- `csrf` is 64 lowercase hexadecimal characters. It is a second secret and it is different from the grant.
- `expires_in` remains 600. The monotonic lifetime is shared. Restart drops the slot. A new proof for the same actor replaces that actor's slot, including the session id and the CSRF secret. A full table still replaces the soonest expiry.

The password proof does not require a CSRF header. That response is the only maintenance body that contains these secrets.

## Writes

Every other maintenance POST requires three headers from the same slot:

- `X-DC24H-Admin-Grant`: 64 hexadecimal characters
- `X-DC24H-Session`: 32 lowercase hexadecimal characters
- `X-DC24H-Csrf`: 64 hexadecimal characters

The grant is compared in constant time. The CSRF secret is then compared in constant time against that same slot. A missing or different session id or CSRF secret returns 403 `csrf_required`. Reads (`GET`) stay grant-only and do not require the CSRF secret. A header with the wrong shape is a bad request before the slot is consulted.

WebAdmin keeps the grant, the session id and the CSRF secret in page memory. It does not write them to `localStorage`, `sessionStorage` or a cookie. Writers send the session and CSRF headers. Readers send only the grant. Forget clears all three together.

The loopback adapter forwards a well-formed session and CSRF header. Any other maintenance write is refused with `csrf_required` before the Unix socket. The access proof is the exception. The adapter still injects the service bearer and still ignores a browser `Authorization` header. It does not log these secrets.

The [Apache proxy example](APACHE2_PROXY.md) forwards a session only when it is 32 lowercase hexadecimal characters, and a CSRF secret only when it is 64 hexadecimal characters. Any other value is removed. The example does not contain a grant or a CSRF secret. `Require local` stays.

## Request limits

Two limits apply together.

- The process still accepts at most one maintenance POST per second. A faster POST returns 429 `rate_limited`.
- Each live session accepts 24 requests in 10 seconds. The count includes reads and writes that present that grant. The next request in the window returns 429 `rate_limited`. A missing grant does not consume the budget. Replacing the session starts a new budget.

The Unix socket has no client IP. The budget is not taken from `X-Forwarded-For` or any other proxy header. This is not a hub-wide denial-of-service control. Status and health stay bearer-only and are not part of a maintenance session.

## What this does not do

There is no cookie session, no browser-storage session and no public remote administrator login. HTTP/2, OCSP, automatic certificate renewal and a production audit of these checks are not implemented. Removing `Require local` would still let a remote client use the adapter's service token. Do not do that.
