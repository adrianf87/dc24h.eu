# beta-0.3.10 — optional private read-only access pilot

**Opt-in, development-only and not Internet-facing WebAdmin.** Stages
beta-0.3.8–beta-0.3.10 strengthen the existing **loopback HTTPS** example,
the dedicated Node systemd sandbox, and an operator-controlled **SSH local
port forward**. They do not enable a new public listener, browser login mode,
permission, route, production deployment or automatic SSH configuration.

## Security boundary

Pilot workstation browser (`https://localhost:8443`) → localhost-only SSH
forward (client `127.0.0.1:8443`) → authenticated SSH server → server loopback
Apache TLS (`127.0.0.1:8443`) → loopback Node (`127.0.0.1:4182`) → private
C++ API Unix socket. The browser **must still validate the Apache TLS
certificate** for `localhost`, in addition to verifying the SSH host key.

The private tunnel is intended for a short, supervised test with an
administrator/Owner who already has password + enrolled TOTP. It does not
make TOTP phishing-resistant. A malicious host, stolen SSH key, compromised
client or deliberate forwarding to an untrusted host remains a threat.

## Before opting in

1. Review `config/apache2-webadmin-remote.conf.example`. It allows TLS
   1.2/1.3, restricts TLS 1.2 to forward-secret AEAD ciphers, disables TLS
   compression/session tickets and adds privacy/isolation response headers.
   The server **must** remain bound to `127.0.0.1:8443`. Provision a valid
   certificate for DNS `localhost` and carefully distribute only its public
   CA/certificate, never the private key. Verify certificate expiry and trust.
2. Review `config/dc24h-remote-gateway.service.example`: private MFA state,
   read-only system directories, minimal network address families, blocked
   outbound non-loopback traffic, kernel/device/namespace protections.
   `PrivateNetwork=true` is deliberately absent because Apache needs host
   loopback; `MemoryDenyWriteExecute=true` is deliberately absent because it
   may break V8 JIT. The operator must test service startup, API socket/token
   access and read-only flows on the actual host before relying on it.
3. Secure SSH independently: key-only authentication, verified pinned host
   key, a restricted dedicated forwarding key/user, server-side destination
   limit (`PermitOpen 127.0.0.1:8443`) and appropriate firewall/rate limits.
   A per-key `authorized_keys` restriction can use the OpenSSH options
   `restrict,port-forwarding,permitopen="127.0.0.1:8443"`; review the entire
   account's permissions, shell access and SSH daemon configuration. Do not
   configure a broad remote forward or public bind for port 8443.
4. On the pilot workstation, with OpenSSH and the operator's verified key:

   ```sh
   ssh -N -T -o ExitOnForwardFailure=yes -o StrictHostKeyChecking=yes \
     -o IdentitiesOnly=yes \
     -L 127.0.0.1:8443:127.0.0.1:8443 pilot-user@trusted-ssh-host
   ```

   This command binds **only the workstation's own loopback**. Replace the
   SSH user/host with independently confirmed values. Do not change `-L`
   to a wildcard or `0.0.0.0` bind. The SSH tunnel does not bypass Apache
   or the gateway's same-origin, Host, CSRF, MFA or read-only restrictions.
5. Check the tunnel with the public CA/certificate copied securely to the
   workstation (from this repository):

   ```sh
   python3 scripts/remote_pilot_check.py --ca-file /secure/path/remote-ca.pem
   ```

   The checker uses Python's verifying TLS client, always connects to
   `localhost:8443`, and sends five **unauthenticated GETs** only. It verifies
   the portal is available, session/summary reject unauthenticated access,
   the local admin shell and `/admin-api` remain unavailable, and all checks
   have no-store/nosniff policies, and the portal response is actual `text/html`.
   It sends no password, OTP, cookie or token;
   it refuses redirects, unexpected success and TLS errors. It cannot certify
   full security or substitute for testing on the actual host.
6. On the same workstation open `https://localhost:8443/remote.html`, trust
   the certificate via the browser's normal trust store (do **not** click
   through a certificate warning), sign in with account password + TOTP,
   verify that only aggregate metrics appear and logout clears the session.
   Check the service journal for fixed-name events; establish local retention.

## Rollback and non-goals

Stop SSH (`Ctrl+C`) to end the pilot and invalidate its connectivity. Log
out in the portal; optionally restart the gateway to clear all sessions.
Never remove or roll back the MFA replay ledger while reusing a TOTP secret.
No production/public endpoint, reverse proxy, privilege escalation, server
write action, browser management of MFA devices or permanent access is
authorized by this stage. Before any Internet-facing release, design
phishing-resistant WebAuthn/passkeys, enrollment/recovery, edge controls,
abuse protection and independent operational audit.
