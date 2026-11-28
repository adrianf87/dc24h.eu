# Stage V Security and End-to-End Qualification

**Qualification remains open. OWNER STAGE V ACCEPTANCE and approval to merge are pending.** [Issue #10](https://github.com/adrianf87/dc24h.eu-twilight/issues/10) · [Polski](../pl/STAGE-V-SECURITY-E2E-ACCEPTANCE.md)

## Source and environment

The Stage V branch started at `3486b268bb6c44e73072f4e7490ae0417d6dd5b7` after PR #30. The baseline advanced to `6ff15ad4f03be3b3cdce475fe7b0c31ca0134168` after PR #31 merged. The local qualification used `d6f80e5fe2f0dee8c14db995407deaef0bf47f89` on Ubuntu 24.04.5 LTS on 2026-11-28 UTC. The qualification commit adds tests and CI evidence collection. Report-only commits following it require their own GitHub Actions results. [PR #30](https://github.com/adrianf87/dc24h.eu-twilight/pull/30) · [PR #31](https://github.com/adrianf87/dc24h.eu-twilight/pull/31)

The deployed WebAdmin on TCP 7178 uses backend `6011b09d089af4fc837493a9a703a93601a35bf8` with the local port overlay and UI `be1d0d232608277e5d26bedfd0910db4598f9421`. The backend source tree matches `3486b26` and the UI source tree matches `6ff15ad`. Eleven installed artifacts matched the deployment manifest. The running ADCS image on TCP 1512 matched its binary digest for `795b9a91bacea49d4a9eca7745430fa316d64133`. Production service PIDs remained stable throughout qualification. [Deployment observations](../../logs/tests/stage5-ubuntu-24.04-d6f80e5/deployed-observations.log)

PASS means the stated check met its expectation. FAIL records an unmet expectation. SKIP records a deliberately omitted case. NOT TESTED records a missing result. WebAuthn, passkeys and rpID are N/A for the approved password and email OTP design.

## WebAdmin and HTTPS

| Requirement | Result | Evidence and scope |
| --- | --- | --- |
| Password and eight-digit OTP, invalid password and code, expired and consumed code | PASS | Real C++ account service and MariaDB behind the Node gateway with synthetic SMTP. Missing OTP leaves the request unauthenticated. Concurrent verification accepted the code once. Chromium covered the UI flow with fixture API responses. |
| Registration and email verification | PASS | Disposable registration required mailbox proof and a fresh login. The SMTP fixture passed six tests using locally trusted TLS, delivery, rejection and timeout. |
| External SMTP delivery and a live authenticated browser flow | NOT TESTED | This run used disposable identities and a local SMTP fixture. Live inspection used unauthenticated HTTPS requests. |
| Session expiry, logout, revocation and password changes | PASS | The real store rejected sessions at 120 seconds idle and 600 seconds absolute, and revoked active sessions after a password change, explicit revocation and account revision changes. |
| Login and OTP-send limits | PASS | Bounded attempts reached the expected refusal. The fifth send within the per-account window produced no message. |
| Owner, Administrator, Registered and disabled accounts | PASS | The disposable fixture verified authenticated role assignment, Registered denial on accounts, bans and audit, and revocation on role or status changes. Native account service regressions checked hierarchy and credential handling. |
| Privileged proof and audit | PASS | Wrong current password denied sensitive changes and audit reads. The refused write left the account table and success audit unchanged. Authorized account and ban operations added audit events. |
| HTTPS certificate, CA trust, hostname and SAN | PASS | The Ubuntu VPS connected with the system CA trust store on 7178 and 1512. Both served the same expected leaf as the local certificate, valid until 2027-02-25 22:51:28 UTC. |
| Cookie attributes and session issuance | PASS | Live challenge cookie had `Secure`, `HttpOnly`, `SameSite=Strict`, `Path=/` and no `Domain`. The isolated authenticated flow checked these attributes and confirmed an independently issued session after OTP. |
| Expected request denials | PASS | Isolated regressions checked missing CSRF and request origin, host and proxy-header policy. Live benign requests confirmed 401 for unauthenticated session and identity reads and 403 for an unsupported method and unknown route. |
| Active bypass and vulnerability reproduction scenarios | SKIP | The qualification exercised defensive policy, expected denials and successful state changes in fixtures. These results cover the specific checks listed in the matrix. |
| Local private listeners | PASS | C++ and the bridge used permission-restricted Unix sockets. MariaDB had `skip_networking=1`. Node listened on `127.0.0.1:4278`. TCP 1511 and 3306 had no listener. |
| Independent Internet privacy and ingress verification | NOT TESTED | A separate authorized vantage point is required to check the deployed HTTPS boundary, private ports and management exposure. |

## ADC and ADCS clients

| Client or property | Result | Evidence and scope |
| --- | --- | --- |
| ncdc 1.23.1 on ADC and ADCS | PASS | Loopback guest login, public and private chat, search, active/passive flows, CTM, RCM and reconnect. Ten native client profiles passed. |
| ADC account access | PASS | Native C++/MariaDB process tests exercised credentials and refusal separately from ncdc guest login. |
| Reference TLS and ncdc KEYP | PASS | Isolated TLS tests checked trusted chain and SAN, rejected a foreign chain, wrong SAN and expired leaf, and exercised certificate cutover. A separate ncdc test rejected a pin for another leaf and reached login with the correct pin. |
| Close, quotas and bounded resource use | PASS | Native tests covered slot reuse, refusal at configured limits, abrupt disconnect, descriptor pressure and bounded chat throttling. The loopback probe completed 12/12 ADC and 12/12 ADCS operations. |
| AirDC++ Web Client 2.14.0 on ADC and ADCS | PASS | The official portable Linux client completed two guest sessions per transport and the hub cleaned up all four connections. The ADCS run used an explicit test pin. This smoke test covers login, disconnect and reconnect. [Portable Linux client](https://airdcpp.net/docs/installation/linux-binaries.html) · [Result](../../logs/tests/stage5-ubuntu-24.04-d6f80e5/airdcpp.log) |
| DC++ | SKIP | The upstream Windows client requires a Windows or Wine test setup, which was unavailable on the Ubuntu VPS. [DC++ downloads](https://dcplusplus.sourceforge.io/download.html) |
| Debian 13 qualification | SKIP | The authorized host for this run was the current Ubuntu VPS. Earlier Debian port 7278 evidence is a separate deployment. |

## Recovery and open P0 items

| Requirement | Result | Evidence and scope |
| --- | --- | --- |
| Isolated restart and storage recovery | PASS | A transient non-root systemd hub recovered after an induced process exit. A disposable MariaDB restarted, rejected a changed migration checksum, and restored a matched data/configuration backup with sessions invalidated. |
| Certificate renewal readiness | FAIL | `certbot.timer` was disabled and the packaged cron command is bypassed on this systemd host. There were no executable Certbot deploy hooks and ADCS reported `CanReload=no`. Isolated TLS rotation passed. A scheduled renewal and service-adoption procedure still needs verification. |
| Log and artifact review | PASS | The bounded journal and Apache log sample contained zero matches for the labelled secret checks. Published evidence retains status and case names without raw transcripts or credentials. |
| Lost Owner mailbox | NOT TESTED | The fixture verified mail-delivery refusal and native maintenance covered ownership transfer. The deployed database had one active verified Owner and zero active verified Administrators. Provider mailbox recovery or a qualified alternate-owner route remains open. |
| Rollback between merged UI revisions | PASS | Chromium verified the isolated asset switch from the PR #31 UI back to PR #30, including the sign-in screen and four asset hashes. |
| Cross-version full release rollback | NOT TESTED | The MariaDB restore used matched data and configuration at one version. A full rollback between distinct accepted backend releases remains unqualified. |
| Exact-head hosted CI | NOT TESTED | GitHub Actions is configured for Ubuntu 22.04, 24.04 and 26.04. Results from these jobs are pending in this report. |

The P0 review remains open for certificate renewal, independent ingress and privacy, Owner mailbox recovery, full release rollback and the outstanding client and CI evidence. The OWNER decision and separate merge decision follow review of the matrix. [Issue #10](https://github.com/adrianf87/dc24h.eu-twilight/issues/10)

## Reproducible evidence

The TLS process suite previously exited without executing its assertions. Stage V restored its entry point and fixed the expired certificate and resumption fixtures. All eight checks in the suite ran and passed. Local results include 33 native CTest cases passed and one hosted-only recovery case skipped inside CTest, 22 Node cases, five Chromium cases, one systemd sandbox case, the MariaDB qualification and additional SMTP, Unix bridge, ncdc pin and loopback checks. The workflow records the checkout SHA and uploads status-only JUnit, logs and SHA-256 sums on each hosted Ubuntu image. Its CTest filter and fixture commands document the reproducible hosted path. [Ubuntu workflow](../../.github/workflows/stage5-security-e2e.yml)

- [JUnit](../../logs/tests/stage5-ubuntu-24.04-d6f80e5/qualification.xml), [sanitized log](../../logs/tests/stage5-ubuntu-24.04-d6f80e5/qualification.log) and [result summary](../../logs/tests/stage5-ubuntu-24.04-d6f80e5/qualification.json)
- [Environment, exact SHA and binary hashes](../../logs/tests/stage5-ubuntu-24.04-d6f80e5/provenance.json), [loopback measurements](../../logs/tests/stage5-ubuntu-24.04-d6f80e5/loopback-probes.json) and [SHA-256 manifest](../../logs/tests/stage5-ubuntu-24.04-d6f80e5/SHA256SUMS)
- [Local operations observations](../../logs/tests/stage5-ubuntu-24.04-d6f80e5/operations-observations.log), [isolated restart](../../logs/tests/stage5-ubuntu-24.04-d6f80e5/systemd-restart.log), [AirDC++ smoke test](../../logs/tests/stage5-ubuntu-24.04-d6f80e5/airdcpp.log) and [UI rollback](../../logs/tests/stage5-ubuntu-24.04-d6f80e5/ui-rollback.log)
