# Beta 0.4.9–0.4.12 — Stage III: session and edge security

Development implementation for [Issue #222](https://github.com/adrianf87/dc24h.eu-high-starter/issues/222), on the single `beta-0.4.9-0.4.12-stage3` branch. Four ordered patch commits and one PR to `main` are required. **Wait until every required GitHub CI run and job completes**, including after any fix; no automatic merge, public exposure or production deployment.

| Patch | Behavior and validation |
| --- | --- |
| beta-0.4.9 — Session Protection | Rotate the browser session ID and CSRF after passkey enrollment or device rename, revoke other actor sessions after enrollment, invalidate all actor sessions after key revocation and recheck authorization after asynchronous request reads. Deny old cookies and delayed body races. |
| beta-0.4.10 — Anti-Bruteforce | Apply one minute, HMAC-keyed, persistent login/challenge/enrollment/device budgets. Both password+TOTP and password+passkey consume the same actor/global login quotas. Restart does not clear limits; unhealthy storage fails closed. |
| beta-0.4.11 — HTTPS Edge Security | Continue the private loopback-only TLS Apache2 vhost; require matching SNI/Host and Origin for every privileged POST, reject query/wrong methods and streamed or oversize bodies, strip proxy identity, and test TLS certificate trust/hostname failures. |
| beta-0.4.12 — Monitoring & Audit | Rehash and verify the append-only audit chain periodically; detect truncation, modification and unexpected replacement; deny access on audit loss; emit rate-limited, reason-only journal alerts for repeated denials, upstream outages and ledger/rate-limit failures. |

## Unchanged security boundaries

The C++20 hub still speaks ADC/ADCS and owns passwords, roles, grants and the read-only private API. The MariaDB schema remains **v11**; no migration is needed. WebAdmin remains React + TypeScript + Vite and the gateway remains a **separate Node process listening on `127.0.0.1:4182`**, behind an **optional, manually configured Apache2 TLS vhost at `127.0.0.1:8443`**. The public Apache vhost never proxies these routes. No arbitrary remote management commands, service token exposure, remote write, passwordless login or public listener are introduced. Existing TOTP is a fallback, not a phishing-resistant factor.

Target distributions: Ubuntu Server 22.04, 24.04, 26.04 and Debian 12/13. The independent operator validation target is Debian 13 Trixie, Xeon D-1521 (8 × 2.7 GHz), 31,997 MiB RAM. Ubuntu CI is **not** a substitute for target-host certificate, privilege and service checks.

## Private files and service preparation

Keep Stage I's MFA enrollment, replay ledger and durable audit, and Stage II's credential catalog. Before starting the gateway, configure the new owner-only rate-limit path in the **example** systemd unit (the build does not install or enable it):

```ini
Environment=DC24H_REMOTE_RATE_LIMIT_FILE=/var/lib/dc24h-remote/rate-limits.json
```

The gateway UID must own the parent directory with mode `0700`. The catalog is created with mode `0600`, refuses symlinks/hard links, unsafe ownership, corruption and concurrent writers, and uses an atomic rename and `fsync` for durable updates. It contains only HMAC-SHA256 bucket keys, counts and window start times; it does **not** contain account IDs, names, IPs, credentials, TOTP, passkey private keys or C++ token material. The HMAC key is derived from the C++ service token kept outside the catalog. **Rotating the service token changes the bucket namespace**: treat this as a reviewed security event and retain the old log/catalog evidence; it cannot preserve per-actor quotas across a token change. Never reset the file casually to remove a lockout.

Shared quotas: at most 20 anonymous challenges, 12 global login attempts and four per actor in a one-minute window; enrollment and device changes have separate actor budgets. Denied buckets saturate instead of forcing a disk write for every abusive request. All requests are private/loopback; no personal-data or source-IP rate limits are implemented. The gateway is intentionally a **single writer**; a second process using a stale file fails closed instead of sharing optimistic counters. If the limiter becomes unreadable or is externally replaced, sessions are dropped and authenticated traffic is refused. A full bucket catalog denies new attempts rather than evicting active quotas.

After passkey enrollment or device rename, use the new CSRF token returned in the response and the newly issued Secure/HttpOnly/SameSite=Strict cookie. **An old cookie cannot continue using an existing session.** Key revocation ends every session for the actor. C++ role/grant changes are rechecked before protected reads or credential changes, including after a delayed request body. Do not preserve browser session state across unexpected authorization failures.

## Isolated HTTPS testing

The Apache2 example at `config/apache2-webadmin-remote.conf.example` binds **only** `127.0.0.1:8443`. Replace certificate and document-root paths in a disposable private test; use a certificate whose SAN includes `localhost` and verify it with a trusted CA file. The Origin/RP ID remain fixed (`https://localhost:8443` / `localhost`). The test harness at `tests/apache_remote_gateway_tests.py` checks syntax and performs negative requests through a disposable Apache instance, **not** through a production vhost. It verifies SNI/hostname and trust-chain rejection, wrong Host, Origin, Fetch Metadata, methods, query strings, unsafe assets, proxy headers and size/framing limits.

For a separately reviewed local-only installation, a manual TLS smoke test with a trusted issuer may use:

```sh
openssl s_client -connect 127.0.0.1:8443 -servername localhost -verify_return_error -CAfile /path/to/trusted-ca.pem
curl --fail --cacert /path/to/trusted-ca.pem https://localhost:8443/remote.html
```

Neither command enables the listener; never use `curl -k` as evidence of certificate validation. Do not bind Apache2 or Node to `0.0.0.0`, loosen `Require local`, forward C++ control routes, or expose the Vite development server.

## Audit monitoring, retention and incident response

The private 8 MiB-limited audit JSONL file remains an fsync-checked SHA-256 chain with monotonically increasing sequence numbers. Every 15 seconds (when requests arrive), the gateway re-verifies all existing records, including in-place rewrites that preserve file length. Validation is bounded by the 8 MiB file cap. A mismatch, replaced file, full file or failed append **fails closed**. This unkeyed chain detects corruption, but it is **not** a signature against a privileged attacker able to rewrite the full history and restart the service.

The operator journal receives only fixed event names and alerts with a fixed reason and timestamp. A one-minute window emits at most one alert per reason: 10 authentication denials, three upstream outages, detected audit loss/corruption or lost rate-limit storage. It never records account names/IDs, passwords, OTPs, source addresses, cookies or credentials. The journal is monitoring, **not** a substitute for the private durable audit authority.

Incident procedure:

1. On `audit_loss`, `audit_corruption` or `rate_limiter_loss`, preserve the service journal and stop the **local gateway**. Do not clear the audit or rate-limit files to make logins work.
2. Record the audit file's final sequence/digest, check ownership/mode and verify the chain with the offline operator procedure. Quarantine changed files and investigate service or storage failure.
3. Keep encrypted, access-limited backups of the audit file, rate-limit catalog, TOTP/replay ledger and passkey catalog together; follow the existing two-operator offline recovery rules. Before the 8 MiB cap, stop the gateway, archive the complete verified audit chain, apply the approved retention schedule and restart with a newly initialized file.
4. Forward or export **only the privacy-limited journal/audit records** over an independently approved authenticated, encrypted off-host channel. Configure retention, access auditing and deletion on both ends; no automatic network exporter is installed by this patch.
5. Restore service only after root cause, grants, session invalidation and fresh factor validation are reviewed. Public deployment remains a separate explicit decision.

## Verification and merge gate

```sh
cd webadmin
npm ci
npm run typecheck
npm test
npm run build
npm run test:e2e
```

Run the C++20 Debug/Release builds, CTest, sanitizer subsets and disposable Apache integration on the CI matrix. Unit tests additionally check restart-surviving rate limits, loss of the storage file, same-size audit rewrites, alert cooldowns, and session/CSRF rotation. Complete Debian 12/13 operator tests and independent security review before any approved production change. **Wait for the entire CI run on the current head commit**, then report actual URLs, SHA and per-job results; a green earlier commit is not a substitute. Keep PR open until required reviews, approvals and validation are complete.
