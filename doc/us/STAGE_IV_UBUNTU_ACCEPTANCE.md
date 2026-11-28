# Stage IV WebAdmin validation on Ubuntu 24.04

On November 28, 2026, the owner authorized the Stage IV completion work on the Ubuntu 24.04 VPS. The HTTPS panel remains on the requested `server-747585786.dc24h.eu:7178`. The local validation below passed. Independent ingress and security acceptance remain open for Issue #10. The historical Debian 13 endpoint on port 7278 is a separate deployment record.

## Deployment evidence

The operating system was Ubuntu 24.04.5 LTS. The source was `795b9a91bacea49d4a9eca7745430fa316d64133`, with a deployment-only `7278` to `7178` substitution in the Apache vhost, WebAdmin unit, bootstrap template and two Node origin/configuration files. The SHA-256 of that Git diff is `d851436e81752e14c8c43684af4de95f45eb3cedf8dc048f9b46de2497818150`. A systemd drop-in adds the same bridge socket readiness check as the updated unit template. After an initial automatic retry during installation and a subsequent controlled Stage IV start with the check, the gateway was active with zero automatic restarts. Stage III ADCS stayed active on TCP 1512.

| Deployed artifact | SHA-256 |
| --- | --- |
| Native account service executable | `4146d58e84469c02c13deff742ed55628b5687741e714ec0f571566a21e083e5` |
| Node gateway bundle | `b43b4c54cb7f5e38dc5789f98a432a68015483d1f92331c3c2d875f1922081d1` |
| Node initializer bundle | `f7f2fed05a85c4d23e8e44c7324729f4848e32f6567829917d083ac288210a17` |
| Python bridge | `d66a718babb45f823d45880e6dcaef8f705f8889eb8bf02f447246db8f49c410` |
| Apache virtual host | `6c1f9cffb16258b6670c77f5d395805bf4f872d188aca2e346236e99b3a1ede1` |

Apache was the sole HTTPS vhost and passed `configtest`. Node 24.22.3 listened on `127.0.0.1:4278` under `dc24h-webadmin`. The C++ service and bridge used `dc24h-stage3` with separate Unix sockets and primary groups `dc24h-stage3` and `dc24h-webadmin`, respectively. MariaDB 10.11.14 used a Unix socket with TCP disabled. The production database had 11 native migrations and two Stage IV migrations with checksums matching the source catalog. No account credentials or cookie values were captured in the evidence.

## Validation results

| Scope | Result |
| --- | --- |
| Running Ubuntu deployment | TLS 1.3 with trusted CA, SNI and expected DNS identity. Chromium opened the login page in a secure context. Apache denied wrong Host, wrong Origin, wrong method, legacy and private routes. The gateway denied missing CSRF and unauthenticated summary access. The challenge cookie had a `__Host-` name, `Secure`, `HttpOnly`, `SameSite=Strict`, `Path=/` and no `Domain`. Cookies are scoped to the host across HTTPS ports. |
| Real Administrator login on this VPS | Password and eight-digit OTP delivery succeeded. The account service reported available for the Owner. Logout rejected subsequent summary access. The audit database contained email verification, login and logout events. |
| Disposable application regression tests | 20 Node public WebAdmin tests, 3 Chromium UI tests, 16 Python boundary tests, 4 Unix bridge tests and 2 preflight tests passed. |
| Disposable C++/MariaDB integration | Mock SMTP failure, invalid and expired OTP, concurrent replay, five wrong guesses, login and registration rate limits, registration role-field rejection, verified registration and login, Owner/Administrator/Registered authorization, CSRF on password change, invalidation of both active sessions after password change, revoke-all and role/status changes passed. |
| Disposable recovery | MariaDB restart, rejection of a mismatched migration checksum with account data retained, matched SQL/configuration restore, revalidation of schema and SQL grants, and session/challenge invalidation passed. The fixture used a separate datadir, Unix socket and mount namespace on this VPS. The restore used the same application version. |

GitHub recorded 6/6 successful workflows and 41/41 successful checks on `795b9a9`. The Ubuntu 24.04 isolated workflow ran Python, Node and Chromium regressions, and the private storage workflow ran fault cases. The dedicated public WebAdmin design workflow with the disposable MariaDB suite was absent from that push. The earlier Stage IV squash commit `1faf188c8b495734e618af656f68cc4ff1240264` had three failed Stage III checks (public TLS, public exposure and evidence aggregation). PR #28 removed the automatic public checks from the Stage III run. Links are available for [current Ubuntu 24.04 validation](https://github.com/adrianf87/dc24h.eu-twilight/actions/runs/46083343545/job/140787158373), [the current hosted job list](https://github.com/adrianf87/dc24h.eu-twilight/commit/795b9a91bacea49d4a9eca7745430fa316d64133/checks), and [the earlier Stage III failure](https://github.com/adrianf87/dc24h.eu-twilight/actions/runs/46079787777).

## Recovery and acceptance boundaries

For suspected loss or compromise of the Owner mailbox, use an independently authenticated operator session and the [dedicated service runbook](STAGE_IV_DEDICATED_SERVICE.md). Withdraw the WebAdmin vhost if mailbox control or application integrity is uncertain. Preserve a matched encrypted database and configuration backup before recovery. Recover the mailbox through the provider's verified process or use the approved offline owner-transfer procedure only if an eligible verified administrator and the required proofs are available. The current setup has one verified privileged account. A backup can restore application state, while restoring mailbox access requires the provider or a separately qualified owner-transfer route. Stop and escalate on missing proofs, checksum mismatch, uncertain backup pairing or a failed authorization check. After an authorized restore, invalidate sessions and challenges and verify a fresh password plus OTP login before restoring ingress.

The VPS-only scope leaves independent HTTPS/ingress validation, external email registration, real mailbox-loss or alternate-owner recovery, and a rollback across distinct accepted releases open. Formal Stage IV security acceptance requires those results or an explicit owner decision changing their scope. See [Issue #10](https://github.com/adrianf87/dc24h.eu-twilight/issues/10).
