# Stage IV public WebAdmin — implementation candidate and release blockers

**NOT DEPLOYED / NO PUBLIC WEBADMIN ACTIVATION.** Draft PR #25 targets Issue #10.
Public ADCS on `62.171.156.32:1512` must remain unchanged.

## What is implemented on the draft branch

- `webadmin/server/public-backend.ts`: fixed-path, bounded Unix-socket calls for
  the C++ password, grant/permission recheck and sanitized status projection.
- `webadmin/server/public-gateway.ts`: new, separate loopback-only handler with
  explicit read-only routes; password **and** WebAuthn passkey for a session;
  server-side opaque session cookies and CSRF; idle/absolute expiration;
  per-request C++ grant reauthorization; passkey device enrollment/revocation,
  same-actor session revocation; fixed, nonidentifying durable audit outcomes;
  persistent attempt budget; exact Origin/RP ID and reject-by-default headers.
- `webadmin/server/public-main.ts`: private configuration and durable state are
  required; startup rejects absent explicit OWNER activation acknowledgement;
  Node binds **only `127.0.0.1:4278`**, not the public interface.
- `webadmin/server/public-private-init.ts`: first-install-only private catalog
  creation, without creating users, passkeys, recovery secrets or public routes.
- `config/apache2-webadmin-public.conf.example`: separate, **disabled** HTTPS
  7278 Apache vhost with a fixed route/method/Host/Origin allowlist, Let's
  Encrypt file references and a tiny separate webroot.
- `webadmin/public-stage4/`: separate static HTML, CSS and browser WebAuthn
  client. It does not ship private `/remote.html` or the local development UI.
- `webadmin/tests/public-gateway.test.ts` and
  `webadmin/tests/public-stage4.spec.ts`: local synthetic HTTP and Playwright
  browser regressions. CI runs on GitHub-hosted Ubuntu only, never the VPS.

## Critical acceptance blockers — all must be closed BEFORE deployment

1. **Initial OWNER/admin passkey enrollment.** The public gateway deliberately
   cannot create a login session without an already-registered passkey for the
   exact production RP ID. The private `localhost` credential does not count.
   First-enrollment requires a separately designed, independently supervised
   OWNER-only process with proof checked by C++ and a protected HTTPS browser
   session under the actual FQDN and Origin. Do not expose a password-only
   public enrollment endpoint or import old RP credentials. No first OWNER
   passkey is provisioned by this branch.
2. **Recovery and separation of duties.** Existing
   `webadmin/server/remote-passkey-recovery.ts` provides offline two-operator
   **revocation**, not proof of independent identity or re-enrollment. Define
   and rehearse external identity verification, restoration/re-enrollment,
   immutable evidence, durable backups, session invalidation and VNC/console
   fallback. There is no public recovery bypass.
3. **Role/grant audit.** Validate against a *real* Debian 13 backend with
   a dedicated OWNER and least-privilege administrator, including demotion,
   password rotation, session-revocation races and C++ permission denial.
   The new public gateway permits only status reads and one's own passkeys,
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
6. **Build, tests and audit evidence.** GitHub-hosted Ubuntu 22.04, 24.04 and
   26.04 TypeScript/HTTP/regression CI plus isolated Chromium Playwright,
   real backend integration, malformed WebAuthn/CSRF/concurrency negative
   cases, exact-SHA artifacts and secure restart/restore rehearsal must pass.
   A synthetic passkey verifier only tests orchestration, not hardware
   authenticator cryptography or browser-to-Debian interoperability.
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
| C++ private API | Unix socket, server-owned token; not exposed |
| Credential/limiter/audit | Owner-only private files, RP ID pinned to FQDN |

**Release status: FIX/QUALIFICATION REQUIRED.** This candidate is not the final
public WebAdmin and cannot be safely activated until these gaps are resolved.
