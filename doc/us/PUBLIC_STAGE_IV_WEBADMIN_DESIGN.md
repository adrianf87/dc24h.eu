# Stage IV — OWNER-approved public WebAdmin design (P0)

**Status: IMPLEMENTATION CANDIDATE / NOT FULLY QUALIFIED / NO PUBLIC WEBADMIN ACTIVATION.**

Issue [#10](https://github.com/adrianf87/dc24h.eu-twilight/issues/10) requires a separate
OWNER decision on the administration boundary. The OWNER selected Internet-facing HTTPS,
the existing VPS hostname, and a distinct TCP port. OWNER authorized preparing
the Stage IV candidate on a draft branch. This does **not** authorize opening
a listener, provisioning production credentials, installing on or changing the VPS.
See [implementation and acceptance gaps](PUBLIC_STAGE_IV_WEBADMIN_IMPLEMENTATION.md).

## Fixed deployment identity

| Purpose | Approved value |
| --- | --- |
| Public guest ADCS hub | `adcs://server-747585786.dc24h.eu:1512` |
| Future browser WebAdmin | `https://server-747585786.dc24h.eu:7278` |
| IPv4 of the existing Debian 13 VPS | `62.171.156.32` |
| WebAuthn relying-party ID | `server-747585786.dc24h.eu` |
| Exact WebAuthn / browser API Origin | `https://server-747585786.dc24h.eu:7278` |
| WebAdmin authentication | OWNER and separate administrator accounts, verified passkeys/MFA |
| Release decision | Separate `OWNER WEBADMIN PUBLIC ACTIVATION GO` after Stage IV acceptance |

Do not use the retired public host or create an `admin.*` hostname. Do not
change TCP 1512, enable plaintext ADC on TCP 1511, or expose existing internal C++ APIs,
MariaDB, service-maintenance endpoints, or development WebAdmin ports.

The WebAuthn RP ID is the hostname **without** a scheme or port. The accepted Origin
includes HTTPS, the hostname, **and** port 7278. Reject a different port, IP-literal
Origin, untrusted forwarded host, and any non-exact RP ID. Credentials registered for
the existing `localhost` pilot must not be silently accepted for this public RP ID;
use explicit OWNER-approved enrollment/migration with rollback.

## Existing private gateway is not a public implementation

The current [`REMOTE_GATEWAY.md`](REMOTE_GATEWAY.md) documents a development-only,
loopback HTTPS read-only pilot. Its Apache vhost binds `127.0.0.1:8443`, and the Node
gateway binds `127.0.0.1:4182` while explicitly refusing public configuration.
**Do not remove those safety checks, proxy `/remote.html` to the Internet, or simply
rewrite `localhost` to the public hostname.** Stage IV needs an independently reviewed
public entrypoint, startup configuration, and route/method authorization model.

Proposed separation, not yet deployed:

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
- **OWNER decision: password + verified passkey are BOTH mandatory** for every
  public privileged login. First verify the account password through the private
  backend; only then begin a short-lived passkey challenge for that same account.
  A session or admin grant must not be issued until the WebAuthn assertion also
  passes user verification, exact `Origin`, RP ID hash, signature, credential
  ownership, replay/counter and revocation checks plus fresh backend role checks.
  Password-only, passkey-only, or password/TOTP-only fallback is **not approved**.
  The public two-factor state machine is an internal building block, not a public
  HTTP endpoint, and must be integrated only after gateway security review.
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
- Owner-only, auditable passkey enrollment and device revocation; independent
  recovery with an offline protected recovery mechanism, verified identity,
  session/device revocation and no silent MFA bypass. Never put recovery codes,
  passwords, OTP seeds or private WebAuthn key material in the repository or CI.
- Generic login errors, bounded rate limits, account lockout/recovery controls,
  no secrets in logs, session fixation protection and audited grant changes.

## TLS and host operations

- Use a publicly trusted Let's Encrypt certificate with a SAN for the approved
  hostname and a valid full chain, even though the listener is on port 7278.
- Provision the TLS private key only on the host with least-privilege access;
  never add it to the repository, build artifacts or browser bundle. Reuse of a
  host certificate requires a reviewed distribution/reload and rollback procedure.
- Configure the dedicated Apache listener only after verifying actual ownership of
  port 7278, provider ingress policy, DNS and independent reachability. This design
  does not change any firewall, certificate, DNS, systemd unit or listener.
- Keep the retired self-hosted runner removed. CI and integration tests use only
  GitHub-hosted Ubuntu 22.04, 24.04 and 26.04; no CI SSH or remote `sudo` on the VPS.
- Existing manual Stage III Let's Encrypt renewal responsibility and hub service
  remain unchanged. Any public WebAdmin activation is a new OWNER gate.

## Mandatory acceptance and negative-test evidence

Before requesting `OWNER WEBADMIN PUBLIC ACTIVATION GO`, demonstrate on isolated
GitHub-hosted environments with sanitized exact-SHA reports:

1. Dedicated TLS 7278 SNI/Host/Origin/RP ID verification; wrong hostname, port,
   insecure scheme, SAN/chain, missing/expired certificate and plaintext rejection.
2. Exact route/method allowlist, strict body framing, traversal/encoding bypass,
   prohibited `/admin-api`, private sockets, MariaDB, service-control and development
   paths denied at the edge and independently at the gateway.
3. Positive OWNER/admin access and negative guest, role-bypass, revoked-account,
   revoked-device, stale-session and cross-account privilege tests.
4. Valid/invalid passkeys, user-verification requirement, challenge expiry/replay,
   wrong Origin/RP ID, credential mismatch and account recovery failure modes.
5. Cookies across ports on the **same hostname**, CSRF, Host/proxy header spoofing,
   CORS, session fixation, logout, concurrent requests and rate limiting.
6. Backend authorization/audit for every allowed action and denial; failure of
   dependencies must produce safe errors with no cached sensitive data.
7. Apache tests, integration tests, Playwright browser tests, build and CTest;
   no secrets in reports; service shutdown/rollback rehearsal in isolation.

Stage IV cannot be accepted while the new public gateway is missing, mandatory
negative evidence is absent, or the cross-port cookie risk is unresolved. Maintain
the current public ADCS hub unchanged throughout development.

## Primary standards

- [W3C Web Authentication](https://www.w3.org/TR/webauthn-3/): RP ID and Origin
  semantics, registration and authentication verification.
- [RFC 6454](https://www.rfc-editor.org/rfc/rfc6454.html): scheme/host/port Origin.
- [RFC 6265](https://www.rfc-editor.org/rfc/rfc6265.html): cookies are not isolated
  by port.
- [OWASP Passkey Security](https://cheatsheetseries.owasp.org/cheatsheets/Passkey_Security_Cheat_Sheet.html)
  and [Session Management](https://cheatsheetseries.owasp.org/cheatsheets/Session_Management_Cheat_Sheet.html).
