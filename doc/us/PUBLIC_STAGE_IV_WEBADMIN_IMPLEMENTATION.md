# Stage IV public WebAdmin — implementation candidate and release blockers

**NOT DEPLOYED / NO PUBLIC WEBADMIN ACTIVATION.** PR #25 was merged to
`main` as `2985c3d`; further account verification and scoped live C++
integration are under review in a separate Stage IV qualification branch.
Public ADCS on `62.171.156.32:1512` must remain unchanged.

## What is implemented on the draft branch

- `webadmin/server/public-backend.ts`: fixed-path, bounded Unix-socket calls for
  the C++ password, grant/permission recheck and sanitized status projection.
- `webadmin/server/public-gateway.ts`: new, separate loopback-only handler with
  explicit read-only routes; C++ password **and** emailed code for a session;
  server-side opaque session cookies and CSRF; idle/absolute expiration;
  per-request C++ grant reauthorization, same-actor session revocation; fixed,
  nonidentifying durable audit outcomes, persistent attempt budget and strict
  Origin/Host checks.
- `webadmin/server/public-email-code.ts`: CSPRNG eight-digit single-use codes,
  HMAC-only pending state, five-minute expiry, five-guess cap, replay denial.
- `webadmin/server/public-verified-email.ts`: restricted, verified owner/admin
  account-to-mailbox bindings, never client-specified recipients.
- `webadmin/server/public-msmtp.ts`: `admin@dc24h.eu` sender with fixed
  dedicated-UID `msmtp --file=/var/lib/dc24h-stage4/.msmtprc -t` invocation,
  private configuration ownership checks, and no shell/header injection.
- `webadmin/server/public-main.ts`: private configuration and durable state are
  required; startup rejects absent explicit OWNER activation acknowledgement;
  Node binds **only `127.0.0.1:4278`**, not the public interface.
- `scripts/stage4_webadmin_bridge.py`: restricted Unix socket bridge from
  `dc24h-webadmin` to hub-UID C++ API, verified by kernel `SO_PEERCRED` with
  exactly three paths. The C++ token never enters Node. See
  [dedicated service deployment/rollback](STAGE_IV_DEDICATED_SERVICE.md).
- `webadmin/server/public-private-init.ts`: first-install-only audit/limiter
  catalog creation, without creating users, emails or public routes.
- `webadmin/server/public-email-enrollment.ts` and
  `public-email-enroll-main.ts`: new **offline, one-time** C++ password/role and
  mailbox OTP verification before first catalog publication (never overwrites).
- `api::public_account_access_handler`: new private serving C++ API subset for
  password proof and read-only role discovery; all maintenance writes are 404.
  It is OFF BY DEFAULT behind a new C++ OWNER environment acknowledgement and
  needs real Debian 13 integration before enabling the public gateway.
- `config/apache2-webadmin-public.conf.example`: separate, **disabled** HTTPS
  7278 Apache vhost with a fixed route/method/Host/Origin allowlist, Let's
  Encrypt file references and a tiny separate webroot.
- `webadmin/public-stage4/`: separate static HTML, CSS and email-code form
  client. It does not ship private `/remote.html` or the local development UI.
- `webadmin/tests/public-email-gateway.test.ts` and
  `webadmin/tests/public-stage4.spec.ts`: local synthetic HTTP and Playwright
  browser regressions. CI runs on GitHub-hosted Ubuntu only, never the VPS.

## Critical acceptance blockers — all must be closed BEFORE deployment

1. **Verified OWNER/admin email enrollment.** The C++ account model has no
   verified contact email field; the ADC user's public `EM` profile is NOT
   authoritative. An OWNER must verify mailbox ownership out of band and
   provision an owner-private catalog:
   `{"schema":1,"accounts":[{"actor":"7","email":"person@example.test","verified":true}]}`.
   This is a schema example, **not** a preverified address or authorization
   to send mail. OWNER selected `admin@dc24h.eu` for the account named
   Administrator, but its numeric C++ actor ID and actual mailbox proof remain
   unverified. Use the interactive first-enrollment runbook in
   [STAGE_IV_EMAIL_ENROLLMENT.md](STAGE_IV_EMAIL_ENROLLMENT.md).
   Without a correctly provisioned catalog, login refuses to issue codes.
   The recipient must never be taken from the login request.
2. **Recovery and separation of duties.** Implement and rehearse independent
   mailbox change/reverification, account identity proof, durable backups,
   session revocation and VNC/console fallback. There is **no public change
   email or password-only recovery endpoint**. A catalog entry marked
   `verified` by an operator is not proof that out-of-band verification
   actually occurred; this process needs acceptance evidence.
3. **Role/grant audit.** Validate against a *real* Debian 13 backend with
   a dedicated OWNER and least-privilege administrator, including demotion,
   password rotation, session-revocation races and C++ permission denial.
   The new public gateway permits only status reads and session revocation,
   not management, SQL, hub control or systemd operations. Verify this is the
   approved initial feature scope.
4. **Same-host cookies.** TCP port is NOT a cookie isolation boundary. Audit
   every other HTTPS endpoint of `server-747585786.dc24h.eu` for transmission,
   acceptance, forwarding, storage and logging of the `__Host-dc24h_stage4_*`
   cookies. Verify no C++ grant/API token can cross between public/private
   services. Refuse activation if this cannot be proved.
5. **Apache/TLS operational qualification.** Validate the actual Apache vhost
   on an isolated Debian 13 replica with real Apache modules and exact
   certificate SAN/chain, SNI, TLS1.3/1.2, allowed routes and denials;
   verify `:7278` ingress externally and forbid local API/DB/dev ports. Check
   cert renewal/rollback policy, Apache reload validation and production
   service ownership. Do not blindly enable the example configuration.
6. **Build, email transport, tests and audit evidence.** Validate Debian 13
   `msmtp -t` configuration and authenticated sender identity
   `admin@dc24h.eu`, successful delivery to the **verified** mailbox,
   relay TLS, bounce/error handling, mail header injection and non-delivery
   failure. GitHub-hosted Ubuntu 22.04, 24.04 and
   26.04 TypeScript/HTTP/regression CI plus isolated Chromium Playwright,
   real backend integration, malformed email code/CSRF/concurrency negative
   cases, exact-SHA artifacts and secure restart/restore rehearsal must pass.
   Synthetic mailer tests do not prove real SMTP delivery or mailbox security.
7. **Host operations and OWNER release.** No deployment, firewall/DNS/Apache
   enablement, public listener or production credentials are written by
   GitHub Actions. Independently verify provider console/VNC rollback and
   on-host backups before a separate `OWNER WEBADMIN PUBLIC ACTIVATION GO`.

## Proposed host state (not applied)

| Function | Address / scope |
| --- | --- |
| Guest ADCS | `adcs://server-747585786.dc24h.eu:1512` — unchanged |
| Public WebAdmin TLS | `https://server-747585786.dc24h.eu:7278` — DISABLED |
| New internal Node API | `127.0.0.1:4278` — DISABLED |
| C++ private API | Hub-owned Unix socket and token; restricted Unix bridge to WebAdmin only |
| Verified mail/limiter/audit | WebAdmin UID-only files, sender `admin@dc24h.eu` via pinned `msmtp --file ... -t` |

**Release status: FIX/QUALIFICATION REQUIRED.** This candidate is not the final
public WebAdmin and cannot be safely activated until these gaps are resolved.
