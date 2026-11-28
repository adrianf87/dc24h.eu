# Stage V TLS renewal security review

Base commit `cfd979d4a9f14b158dfc2f35f270ab2d6e2a055e` on `main`. The Draft PR records the tested head SHA and the hosted CI runs for that SHA. The source qualifies the renewal design in isolated Ubuntu fixtures. Issue #10 remains open for Owner review and the deployment decisions below.

## Control and certificate boundaries

| Surface | Enforcement |
| --- | --- |
| `GET /stage4/v1/tls/renewal` | Current OTP-authenticated Owner or Administrator session |
| `POST /stage4/v1/tls/renewal/check` | Same roles, exact Origin, CSRF, an empty JSON object and a bounded request rate |
| `POST /stage4/v1/tls/renewal` | Owner, exact Origin, CSRF, boolean setting, a session from the last five minutes, and current-password verification in C++ |
| Stage IV Unix bridge | Kernel peer UID, exact private path and form method, only the account-service bearer is forwarded. The account service revalidates the MariaDB session and role, including a second check after password proof |
| Root control | An accepting systemd socket checks both the C++ account-service UID and its systemd cgroup. It accepts four fixed operations and the verified numeric actor ID. Audited intent precedes timer changes. Only the specific renewal timer is enabled or disabled |
| Worker and deploy hook | Certbot's standalone HTTP-01 authenticator for one pre-approved lineage, fixed Let's Encrypt ACME directory, exact DNS SAN, system trust, current validity and matching private key. A nonce-bound deploy receipt confirms a changed lineage. A root lock serializes controls, deployment and rollback |
| Serving boundary | A root-owned versioned bundle shared with the ADCS service group. The hub reload is authenticated on its existing private Unix API. Both endpoints must present the new trusted leaf. A durable pending record makes rollback recoverable after interruption |

The browser status is an allowlisted JSON object. The root audit stores timestamps, fixed operations, outcomes and numeric actors. It carries no request bodies, private key material or ACME credentials. Certbot diagnostics stay in its protected host log directory.

## Local evidence on Ubuntu 24.04.5 LTS

- Release build completed. The isolated CTest run passed 262 of 262 selected tests. The host systemd sandbox and Stage IV VM probe were excluded from that run. The new native test held logged-in ADC and ADCS sessions through successful replacement and rejected malformed, mismatched, wrong-SAN, expired, and foreign-CA certificates while new handshakes continued to use the last valid leaf.
- Real Certbot 2.9.0, Pebble 2.4.0, Apache2 and the C++ hub ran with temporary keys in a loopback-only namespace. Certbot issued and renewed through HTTP-01. The fixture forced one due event. Failure injection after Apache served the replacement caused both listeners to revert. Subsequent concurrent checks applied the already-renewed leaf once. HTTPS returned the expected page and both server PIDs stayed live. The fixture also rejected unsafe Certbot hooks and controller operations, checked expiry, confirmed audit outcomes and exercised idempotent scheduling with a test systemctl adapter.
- Native Stage IV C++ and disposable MariaDB exercised password and eight-digit OTP login, unauthorized or revoked sessions, insufficient roles, CSRF, password step-up and stale OTP. The fixture used mock SMTP.
- TypeScript and the production bundles built. Node tests reported 207 passed and 3 skipped. Chromium passed 75 tests, including six Stage IV cases and the UI paths for role gating, failure and confirmed schedule activation with a synthetic API. The root unit definitions passed `systemd-analyze verify` and a disposable, private-network transient systemd timer fired locally.
- The persistent enable-and-fire fixture is part of the hosted Ubuntu 22.04, 24.04 and 26.04 matrix. Use the Draft PR checks for results on the exact source. GitHub-hosted CI is outside the local proof.

## Residual risks and acceptance

The Ubuntu host inspection was read-only. Its Certbot configuration used standalone HTTP-01 with an explicit IPv4 address and a disabled distro timer. Apache used the current Certbot certificate and the serving ADCS hub still reported `CanReload=no` on the deployed binary. External challenge reachability, approved staging validation, production-configured reloads, continued operator access and exact-head hosted results require verification before an Owner acceptance decision. The isolated renewal test made its ACME requests only to Pebble.

The supported topology is one exact public DNS SAN and one approved IPv4 HTTP-01 binding. Other authenticators, competing schedulers, additional names and nonstandard global options require review. The local preflight does not establish Internet reachability. A last-known-good leaf can expire during an extended failure; operators must monitor expiry and recovery. Remote clients that pin a leaf require a separately authenticated KEYP update for each rotation. The root controls trust the installed code and the private C++ account-service cgroup.

Owner decisions are required for the HTTP-01 ingress and DNS plan, any resulting network/provider changes, merge acceptance, and the initial cutover to the new live binaries and managed paths. Disabling the schedule preserves the installed pair. Failed deployments restore the preceding bundle and verify both services. For full software rollback, disable the timer, retain state and use the approved maintenance window to restore the matched configuration and previously verified leaf. [Operator runbook](../../doc/us/TLS_AUTO_RENEWAL.md)
