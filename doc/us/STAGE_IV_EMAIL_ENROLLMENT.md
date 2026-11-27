# Stage IV — Administrator email verification and public WebAdmin qualification

**PR candidate; no VPS write, SMTP delivery or public WebAdmin activation has occurred.**
OWNER designated **`admin@dc24h.eu`** as the destination for the pre-existing
**Administrator** account. This designation is NOT evidence of mailbox control:
verification is completed only when the actual account holder receives and
enters the challenge from that mailbox on the VPS provider console.

## Critical identity boundary

- The C++ identity is a **numeric actor/account ID**, not the display name
  `Administrator`. Resolve and double-check the active administrator's ID in
  the trusted private account-management tool before running enrollment. Do not
  guess an ID or map a nickname from the public ADC `EM` field.
- C++ must have an existing, active **owner/administrator** account with a
  password. Public users cannot sign up for admin roles. A new staff account is
  created/activated by the already trusted, offline C++ OWNER workflow only;
  separate account creation from public email verification.
- The new CLI proves that C++ account password, rechecks its role, sends an
  8-digit code with `msmtp -t`, takes the code from an interactive console
  without echoing it, checks a 5-minute TTL and proves the role again. Only
  after all checks does it atomically publish a 0600 verified-email roster.
- **First binding only**: the tool refuses to overwrite an existing roster.
  Additional operators and mailbox changes need separately reviewed offline
  backup/rotation, session invalidation and evidence. Do not delete the roster
  merely to rerun this command. The gateway fails closed if its roster changes
  while running.

## PRE-FLIGHT — provider console on Debian 13

These commands are an **operator procedure, not an automated deployment**.
The code must be audited and compiled on a disposable test system before any
host change. The public ADCS listener on `:1512` must stay available.

1. Verify the real running C++ service uses a private API and that its socket,
   API token and permissions belong to the same effective UID selected for the
   public gateway (see example systemd unit). Exposing admin routes on a TCP
   listener is forbidden. The serving API permits only private password proof
   and read-only grant discovery; other account/management paths must return
   404. Validate against a real seeded C++/MariaDB fixture before production.
   This new serving API capability is **OFF BY DEFAULT**; it requires an
   explicit `DC24H_STAGE4_PRIVATE_AUTH_ACK=OWNER_WEBADMIN_PUBLIC_ACTIVATION_GO`
   in the C++ service environment and a separately approved hub restart. Do
   not add that environment variable before live qualification and OWNER GO.
2. Check `/usr/bin/node --version` meets package requirements. Node installed
   under `/root/.nvm` is not usable by an unprivileged systemd service.
3. Confirm that `/usr/bin/msmtp` works as the **service UID**, not just root.
   The mail transport must authenticate to the OWNER-approved SMTP provider
   over verified TLS, send as `admin@dc24h.eu`, and read secrets only from a
   0600 protected service-owned configuration. Do not place SMTP passwords in
   Git, command arguments, environment output, scripts or log artifacts.
4. With the gateway still stopped, send one non-sensitive test message using
   the service account's configured `msmtp -t` and confirm actual arrival in
   `admin@dc24h.eu` (including delay/bounce/error behavior). Do not interpret
   exit 0 alone as delivery proof. In particular, DO NOT reuse root's
   `/root/.msmtprc` from an unprivileged service.
5. Create an **owner-private 0700** StateDirectory only after reviewing the
   service ownership and backup plan. Initialize audit and rate-limit files
   using the existing `public-private-init.ts` first-install-only command.
   The verified email roster must not exist before the first enrollment.

## Interactive first enrollment (gateway stopped)

Build and review both bundles in the checked-out, pinned source:

```sh
cd webadmin
npm ci --no-audit --no-fund
npm run typecheck
npm run build:public:ci
npm run build:enrollment:ci
```

After selecting the **verified numeric account ID** and arranging safe access
to the private C++ API token/socket, run as the correct service UID from the
provider console (example placeholders are NOT shell-ready):

```text
DC24H_API_SOCKET=<PRIVATE_UNIX_SOCKET> \
DC24H_API_TOKEN_FILE=<PRIVATE_TOKEN_FILE> \
/usr/bin/node /opt/dc24h/webadmin/dist/public-email-enroll-main.mjs \
  --actor-id <VERIFIED_ADMIN_ACTOR_ID> \
  --email admin@dc24h.eu \
  --catalog /var/lib/dc24h-stage4/verified-emails.json \
  --owner-ack STAGE4_EMAIL_ENROLLMENT_GO
```

The two interactive prompts take the **C++ account password** and then the
**code received in `admin@dc24h.eu`**. Neither is echoed or included in CI.
The command must fail without creating a catalog for a wrong password,
unavailable SMTP, wrong code, expiration, role revocation, wrong permissions,
existing binding or changed parent. A success binds exactly one numeric account
to exactly one address and does NOT start a web listener.

## Deployment/acceptance remains separate

1. Run GitHub-hosted Ubuntu 22.04/24.04/26.04 matrix and Chromium; verify
   C++ account role/grant revocation, private write-path denials, browser CSRF,
   cookie overlap on the same DNS hostname, negative OTP cases, full build and
   audit/limiter restore. In CI, use injected fake mail only; never real SMTP
   secrets or SSH to the VPS.
2. On a disposable Debian 13 replica, validate the example **disabled**
   `config/dc24h-twilight-stage4-webadmin.service.example` and Apache HTTPS
   7278 vhost, TLS chain/SNI, exact Host/Origin, denial of unknown paths,
   process UID, service restart and rollback. Do not enable the real vhost or
   create its OWNER activation env file yet.
3. On the real VPS, capture before/after `systemctl status`, private API
   liveness, `ss -lntp`, Apache syntax, `:1512` ADCS continuity, TLS SAN/chain,
   DNS and ingress from an **independent external network**. Verify public
   `:1511`, SQL, dev and Node `:4278` are not exposed. Document a provider
   console/VNC rollback and a verified snapshot before changing systemd.
4. Rehearse mailbox loss/recovery, service restart, session invalidation,
   fresh-role checks, password rotation, and audited roster backup/restore.
5. Obtain a **new, explicit** OWNER WebAdmin activation decision after all
   evidence. Initial HTTP scope remains read-only; no public registration or
   remote systemd/SQL/write operations.

**NOT VERIFIED:** real SMTP delivery, actual Administrator account ID, separate
OWNER mailbox, live Debian 13 tests, external ingress and public activation.
Keep Issue #10 Stage IV open until those checks are evidenced.
