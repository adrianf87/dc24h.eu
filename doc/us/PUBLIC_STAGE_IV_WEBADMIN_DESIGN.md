# Stage IV — OWNER-approved public WebAdmin design (P0)

**Status: IMPLEMENTED / OWNER-REPORTED PUBLIC PILOT OPERATING / FINAL SECURITY ACCEPTANCE PENDING.**

The OWNER selected Internet-facing HTTPS on the existing VPS hostname and port 7278.
Per [Issue #10](https://github.com/adrianf87/dc24h.eu-twilight/issues/10), the
OWNER reported a successful external Administrator password + eight-digit email OTP
login, mailbox delivery, dashboard access and password change on November 27, 2026.
That is operator-provided evidence, **not** independent verification, formal
`OWNER STAGE IV ACCEPTANCE`, `OWNER MERGE GO`, or general production/release GO.
The implementation remains in [Draft PR #27](https://github.com/adrianf87/dc24h.eu-twilight/pull/27); no further host,
firewall, DNS, listener, certificate or production-state changes are authorized
by this document. The current installation and recovery steps are in
[the dedicated service runbook](STAGE_IV_DEDICATED_SERVICE.md).

## Fixed deployment identity

| Purpose | Approved value |
| --- | --- |
| Public guest ADCS hub | `adcs://server-747585786.dc24h.eu:1512` |
| WebAdmin (OWNER-reported public pilot) | `https://server-747585786.dc24h.eu:7278` |
| IPv4 of the existing Debian 13 VPS | `62.171.156.32` |
| Exact browser/API Origin | `https://server-747585786.dc24h.eu:7278` |
| WebAdmin authentication | OWNER and administrators: password **and 8-digit emailed code** |
| Fixed sender | `admin@dc24h.eu` via local `msmtp -t` |
| Recipient | Verified account-bound email in private Stage IV MariaDB; login form cannot choose a mailbox |
| Decision record | Explicit OWNER public-activation record and separate Stage IV acceptance, merge and release decisions |

Do not use the retired public host or create an `admin.*` hostname. Do not
change TCP 1512, enable plaintext ADC on TCP 1511, or expose existing internal C++ APIs,
MariaDB, service-maintenance endpoints, or development WebAdmin ports.

The accepted Origin includes HTTPS, the hostname, **and** port 7278. Reject a
different port, IP-literal Origin or untrusted forwarded host. The OWNER
**superseded the earlier passkey decision**: public Stage IV login now requires
the C++ account password followed by a code sent to that account's preverified
email address. The private localhost passkey pilot remains unchanged.

## Existing private gateway is not a public implementation

The current [`REMOTE_GATEWAY.md`](REMOTE_GATEWAY.md) documents a development-only,
loopback HTTPS read-only pilot. Its Apache vhost binds `127.0.0.1:8443`, and the Node
gateway binds `127.0.0.1:4182` while explicitly refusing public configuration.
**Do not remove those safety checks, proxy `/remote.html` to the Internet, or simply
rewrite `localhost` to the public hostname.** Stage IV implements a dedicated
public entrypoint and route/method boundary; independent external review remains
part of acceptance.

Implemented separation (live status is OWNER-reported, not independently verified):

1. A dedicated Apache TLS vhost on external TCP 7278 with exact SNI, HTTP Host, route,
   method, body and header checks. All unspecified routes, methods, paths and query
   variants fail closed. Do not inherit broad `/admin-api`, `/remote`, or `/api` proxy
   directives from another vhost.
2. A new purpose-built gateway listening only on a private loopback socket/port,
   reachable solely from that Apache vhost. No privileged C++ token, grants, raw
   audit events, user credentials or internal API socket paths reach the browser.
3. The C++ private API remains on a Unix socket with backend authorization on every
   operation. The browser and Apache cannot fabricate Owner/admin authority through
   headers. Separate the existing guest-only ADCS service from administrator state.
4. Public browser assets and allowlisted authentication/session/status operations are
   isolated from the existing writable local WebAdmin. Grant any additional management
   operation only after explicit backend permission checks, audit and negative tests.

## Authentication, sessions and recovery contract

- OWNER and administrator are separate least-privilege principals; never use public
  guest admission as administrator authentication. Define the account bootstrap and
  scoped privilege grants before the first public test.
- **Current OWNER decision: password + emailed single-use code are BOTH mandatory**
  for every public login; public Stage IV does not use passkeys. The dedicated
  bootstrap provisions an initial Administrator against native C++ password
  verifiers. The first successful Administrator password plus emailed OTP
  verifies the configured account-bound mailbox and activates that account.
  Each later login requires a fresh code. The address is loaded from private
  account records, never selected by a login POST or the ADC public profile.
  New public users may register as `registered` only after verifying their
  submitted mailbox via a one-time code; this does **not** confer admin rights.
  Codes are eight digits, HMAC-protected, single-use, valid for five minutes
  and limited to five invalid attempts. The sender is `admin@dc24h.eu` via
  fixed-argument `msmtp` without a shell. The C++ authority and account role,
  status and credential revision must be rechecked for privileged access.
  Mail delivery, database, audit or limiter failure must fail closed.
  No password-only or email-code-only privileged session is allowed.
- **Security tradeoff:** email OTP is less phishing-resistant than passkeys and
  may not constitute a fully independent possession factor when the mailbox
  reuses the account password or lacks its own MFA. OWNER requested this change;
  verify strong mailbox protections before activation.
- Server-side short-lived opaque sessions; secure logout, idle/absolute expiry,
  session rotation, concurrent-session bounds, live role/revocation checks, and
  explicit step-up for sensitive management operations.
- Cookies must be `Secure`, `HttpOnly`, host-only, and `SameSite=Strict`; enforce
  exact Origin, CSRF tokens and `Sec-Fetch-Site` checks separately. **Important:**
  cookies do **not** isolate by TCP port. A cookie issued on `:7278` can be sent to
  HTTPS services on the same hostname at other ports. Do not treat port separation
  as a session boundary. Audit all HTTP services on this host for cookie forwarding,
  acceptance, logging and collision; use a dedicated session name, server-side
  origin/authority binding, strict header stripping, and refuse activation if a
  trustworthy boundary cannot be demonstrated.
- Administrative role assignment, mailbox change and recovery remain offline,
  owner-approved operations with identity checks and session revocation. Public
  registration can create only a verified `registered` account; it cannot grant
  Owner/admin roles. Do not put email codes, SMTP passwords or private database
  records into the repository or CI.
- Generic login errors, bounded rate limits, account lockout/recovery controls,
  no secrets in logs, session fixation protection and audited grant changes.

## TLS and host operations

- Use a publicly trusted Let's Encrypt certificate with a SAN for the approved
  hostname and a valid full chain, even though the listener is on port 7278.
- Provision the TLS private key only on the host with least-privilege access;
  never add it to the repository, build artifacts or browser bundle. Reuse of a
  host certificate requires a reviewed distribution/reload and rollback procedure.
- The OWNER reports Apache HTTPS already listening publicly on 7278. Independently
  verify its certificate/chain, SNI/Host/Origin, route and header denials, ingress
  scope and rollback before final acceptance. This documentation edit makes no
  firewall, certificate, DNS, systemd or listener change.
- Keep the retired self-hosted runner removed. CI and integration tests use
  GitHub-hosted Ubuntu 22.04, 24.04 and 26.04; no CI SSH or remote `sudo` on the VPS.
- Existing Stage III Let's Encrypt renewal responsibility and hub service
  remain unchanged. Record the OWNER's specific public-WebAdmin activation
  decision separately; reported operation does not imply final security acceptance.

## Mandatory acceptance and negative-test evidence

Before final `OWNER STAGE IV ACCEPTANCE` and `OWNER MERGE GO`, independently
review the actual public host and retain sanitized exact-SHA CI evidence:

1. Dedicated TLS 7278 SNI/Host/Origin verification; wrong hostname, port,
   insecure scheme, SAN/chain, missing/expired certificate and plaintext rejection.
2. Exact route/method allowlist, strict body framing, traversal/encoding bypass,
   prohibited `/admin-api`, private sockets, MariaDB, service-control and development
   paths denied at the edge and independently at the gateway.
3. Positive OWNER/admin access and negative guest, role-bypass, revoked-account,
   wrong-bound-mailbox, stale-session and cross-account privilege tests.
4. Valid/invalid/expired/replayed email codes, incorrect mailbox, missing email,
   failed `msmtp` delivery, mail header injection, five-guess lockout, rate limits,
   unverified/changed account email bindings and operator recovery failure modes.
5. Cookies across ports on the **same hostname**, CSRF, Host/proxy header spoofing,
   CORS, session fixation, logout, concurrent requests and rate limiting.
6. Backend authorization/audit for every allowed action and denial; failure of
   dependencies must produce safe errors with no cached sensitive data.
7. Apache tests, integration tests, Playwright browser tests, build and CTest;
   no secrets in reports; service shutdown/rollback rehearsal in isolation.

The gateway is implemented and current hosted-Ubuntu CI on the tested code SHA
passed, but green CI does not prove real-host TLS/cookie isolation, negative-path
coverage, account recovery or rollback. Stage IV acceptance and merge remain
pending; do not disturb the public Stage III ADCS hub.

## Primary standards

- [msmtp official manual](https://marlam.de/msmtp/msmtp.html): `-t` recipient
  handling from MIME message headers.
- [OWASP Email Verification](https://cheatsheetseries.owasp.org/cheatsheets/Email_Validation_and_Verification_Cheat_Sheet.html)
  and [Multifactor Authentication](https://cheatsheetseries.owasp.org/cheatsheets/Multifactor_Authentication_Cheat_Sheet.html):
  verified account mailboxes, short-lived codes, phishing and independence risks.
- [RFC 6454](https://www.rfc-editor.org/rfc/rfc6454.html): scheme/host/port Origin.
- [RFC 6265](https://www.rfc-editor.org/rfc/rfc6265.html): cookies are not isolated
  by port.
- [OWASP Session Management](https://cheatsheetseries.owasp.org/cheatsheets/Session_Management_Cheat_Sheet.html).
