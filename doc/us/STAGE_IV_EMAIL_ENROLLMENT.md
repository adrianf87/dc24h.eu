# Stage IV — account email verification and enrollment

**Status (November 27, 2026): IMPLEMENTED / OWNER-REPORTED LIVE LOGIN / FINAL ACCEPTANCE PENDING.**

The OWNER reports that the public WebAdmin at
`https://server-747585786.dc24h.eu:7278` successfully authenticated
`Administrator` with a password followed by an eight-digit code delivered to
`admin@dc24h.eu`. The OWNER also reported a working owner dashboard,
account-service health and password change. These are operator observations,
not independent external security qualification or formal `OWNER STAGE IV ACCEPTANCE`.
Code is in [Draft PR #27](https://github.com/adrianf87/dc24h.eu-twilight/pull/27);
[Issue #10](https://github.com/adrianf87/dc24h.eu-twilight/issues/10) remains open.

## Current authoritative account model

- The protected `dc24h-stage4-init` bootstrap consumes a root-owned, mode-0600
  `/etc/dc24h/stage4-webadmin-bootstrap.env` file. It provisions the reserved
  `Administrator` account and its native C++ password verifier in Stage IV MariaDB.
  Production and disposable test databases remain separate. Never commit
  password, SMTP or database secrets or copy production records into CI.
- On the **first Administrator login**, the gateway requires the initial
  account password and sends a code to its configured mailbox. Successful
  email verification activates the reserved account. Later Administrator
  and other user logins require the password **and a fresh** email code every time.
- Public registration is allowed only for a `registered` account. A new user
  enters a username, password and mailbox on **Załóż konto / Register**,
  verifies the delivered code, and then signs in separately. A submitted
  registration mailbox is not a trusted login recipient until verification.
  Registration does **not** assign Owner/administrator roles.
- Login reads the verified address from the private account record; callers
  cannot select a recipient with the login request or the public ADC `EM` field.
  Password authority, account state, credential revision and privileged grants
  are checked against the backend. Email change, account recovery and privileged
  role assignment require an audited, OWNER-approved offline procedure.
- OTPs are eight digits, HMAC-protected in storage, single-use, expire after
  five minutes and reject after five incorrect attempts. Delivery, replay,
  suspension, role changes, revocation and session expiry fail closed.
  Password-only and code-only privileged login are not supported.
- This public Stage IV uses **password + email OTP**, not passkeys.
  The historical local passkey pilot is separate and grants no Stage IV access.

## SMTP and isolated gateway

- Apache terminates HTTPS on TCP 7278 with an exact Host/Origin and path/method
  allowlist. The dedicated Node gateway binds only `127.0.0.1:4278`, runs as
  `dc24h-webadmin`, and reaches the private C++ account API through a restricted
  UID-checked Unix bridge. The ADCS guest hub stays on TCP 1512.
- The sender is `admin@dc24h.eu` through the service user's `/usr/bin/msmtp`
  with TLS verification. Keep the private 0600
  `/var/lib/dc24h-stage4/.msmtprc` readable only by `dc24h-webadmin`.
  For port 465 the OWNER reported implicit TLS (`tls_starttls off`); port 587
  normally uses STARTTLS. Validate the actual provider configuration,
  certificate and mailbox receipt without revealing credentials or codes.
- Cookies are `Secure`, `HttpOnly`, `SameSite=Strict`, but cookies are **not**
  isolated by TCP port on the same hostname. Cross-port cookie forwarding,
  session acceptance/logging, proxy headers, CSRF and all denied routes
  still require independent live review.

## Installation and controlled operations

Use [the current Debian 13 dedicated-service runbook](STAGE_IV_DEDICATED_SERVICE.md)
for bootstrap, package installation, service startup, SMTP checks, backup,
restore and rollback. Do **not** use the superseded offline
`public-email-enroll-main.mjs --catalog verified-emails.json` instructions:
the current supported Stage IV flow persists account-bound verification in
MariaDB and performs first Administrator enrollment through the WebAdmin form.

Before any further VPS change, identify the reviewed source SHA, installed
binary/configuration identities and relevant OWNER authorization. A successful
live login does not authorize a new restart, firewall change, migration or
release decision.

## Remaining acceptance evidence

1. Independent external HTTPS/TLS/SAN/SNI/Host/Origin tests and forbidden
   route/method/header checks, including proof that private API, MariaDB,
   Node port 4278 and development endpoints are not Internet-accessible.
2. Live negative authorization checks for expired/replayed OTP, denied and
   revoked roles, wrong account/session, CSRF, rate limits and cookie overlap
   with other HTTPS services on the same hostname.
3. New-account registration/verification against the actual mailbox,
   mailbox-loss recovery, session invalidation and auditable role/credential
   changes, without leaking secrets in reports.
4. Documented non-disruptive recovery, backup/restore and rollback evidence
   on the approved target (do not reboot the VPS or restart the live Stage III
   hub merely for qualification).
5. Explicit and distinct OWNER public-WebAdmin activation record,
   `OWNER STAGE IV ACCEPTANCE` and later `OWNER MERGE GO`; no automatic merge.

CI on the reviewed implementation SHA has passed on GitHub-hosted
Ubuntu 22.04, 24.04 and 26.04. New documentation commits advance the
branch SHA and need their own status verification. Do not count intentionally
skipped Stage III external tests as executed tests.
