# Stage III public ADCS qualification report

**STAGE III — ACCEPTANCE PENDING CURRENT CI AND FORMAL OWNER DECISION.**

**OWNER scope revision (November 27, 2026):** Latest-SHA independent public DNS A/PTR and ADCS testing, interactive real DC++ testing, and additional Contabo firewall/port verification are **removed from Stage III completion requirements**. Do not dispatch or execute them, and do not assign PASS. Controlled VPS/live-ADCS restart testing remains excluded. Wait for existing GitHub-hosted CI; **formal OWNER STAGE III ACCEPTANCE GO** and a **separate OWNER MERGE GO** remain mandatory. Historical external results below are preserved for traceability, not new test requirements.

| Scope | Identity |
| --- | --- |
| Tracking issue | [Issue #10](https://github.com/adrianf87/dc24h.eu-twilight/issues/10) |
| Verified protected main baseline | `9c51c5e1a6b39b20d738fc0d1abb552d3c32b56b` |
| Candidate branch | `test/stage3-public-adcs-final-qualification` |
| Public guest endpoint | `adcs://server-747585786.dc24h.eu:1512` at `62.171.156.32` |
| Production unit | `dc24h-stage3-adcs.service` on Debian 13 |
| Historical activation | [Run 45862581211](https://github.com/adrianf87/dc24h.eu-twilight/actions/runs/45862581211) qualified initial external TLS and two guest logins on its deployment SHA |

The retired Debian 13 runner and host deployment workflows were removed in
[PR #23](https://github.com/adrianf87/dc24h.eu-twilight/pull/23). The current
workflows run exclusively on GitHub-hosted Ubuntu. The Debian 13 VPS is the
public service target. Host maintenance is performed by the authorized
operator in a separate window.

**OWNER SCOPE DECISION (November 26, 2026):** The controlled VPS reboot and the intentional live `dc24h-stage3-adcs.service` restart/recovery qualification test are **removed** from Stage III. Do not run them and do not use a missing result as a gate or report it as BLOCKED, FAIL, PENDING, NOT TESTED or PASS. This leaves live restart/reboot recovery unverified; the OWNER accepts that residual risk for this stage. Service restarts needed for a future certificate renewal or actual incident remain separately authorized operational maintenance, not qualification tests.

## OWNER domain declaration and A/PTR evidence

On November 27, 2026 (Europe/Warsaw), the OWNER declared that `dc24h.eu` is their domain and supplied (1) terminal output showing forward `server-747585786.dc24h.eu → 62.171.156.32` and reverse `62.171.156.32 → server-747585786.dc24h.eu` lookups, and (2) a Contabo **Reverse DNS Management** screenshot showing that same IP/PTR pair. See the [sanitized OWNER DNS and CI evidence record](PUBLIC_STAGE_III_OWNER_DNS_AND_CI.md).

**Evidence classification updated November 27: DNS ZONE CONTROL VERIFIED — PASS.** A unique TXT challenge was independently observed through both Google Public DNS and Cloudflare DNS (exact token, TTL 300) at 2026-11-26 23:39:32 UTC; a later OWNER shell screenshot confirms the same record. Registrar/subscriber identity remains OWNER-ATTESTED by private evidence, not independently identity-checked. The external `dns.ptr.forward-confirmed` check remains in source but is not a Stage III acceptance gate under the revised OWNER scope; it must not be dispatched as a separate test.

## Evidence matrix

The initial matrix records the disposition before exact-commit evidence review.
Use the Markdown result matrix and artifacts from the commit under review.

### Hosted systemd regression

The [Stage III run for `f1771b1`](https://github.com/adrianf87/dc24h.eu-twilight/actions/runs/45893716889)
recorded `systemd.sandbox` failures on all three Ubuntu images. The
[Stage IV](https://github.com/adrianf87/dc24h.eu-twilight/actions/runs/45893716980)
and [C++ Debug/Release](https://github.com/adrianf87/dc24h.eu-twilight/actions/runs/45893716891)
logs show the unprivileged runner was denied access while inspecting the
protected `/root` path or the file in the service's mode-0700 state directory.
The Stage III evidence gate reported the missing dependent loopback and public
bundles. The timing and summary checks also failed after their CTest inputs
failed.

The fix uses the hosted runner's packaged tools to verify the exact temporary
paths and inspect state-file ownership and mode. The test still runs the
service write probes against `/root`, `/etc`, and `/usr` and checks cleanup.
The added regression exercises absent, present, and permission-denied
outcomes through the unprivileged runner's host-tool path. The replacement
commit requires its own Ubuntu matrix and external artifacts.

### Ubuntu 22.04 TLS fixture

The [Stage III run for `e8c4d51`](https://github.com/adrianf87/dc24h.eu-twilight/actions/runs/45896074528)
passed all three full CTest and systemd matrices, two-client loopback,
stability, public TLS and exposure. Its Ubuntu 22.04 TLS fixture was the
remaining failure. The missing public ncdc and two-client bundles caused the
evidence gate to fail.

The two synthetic certificate authorities shared a subject name. A loopback
reproduction with the same Ubuntu OpenSSL 3.0.2 package returned code 30
(authority and subject key identifier mismatch), while OpenSSL 3.5.9 returned
code 20 (local issuer not found) with certificates generated by either tool.
The fixtures now use distinct issuer names and require code 20 for the foreign
leaf. The regression also verifies that the same leaf succeeds with its own
CA. Date and hostname tests retain their case-specific checks. Diagnostics
record only the numeric code and a controlled description. The replacement
commit requires its own qualification evidence.

### Acceptance checklist

| Requirement | Exact-commit evidence | Initial disposition |
| --- | --- | --- |
| C++ build, CTest, config, security, and deployed binary identity | Three Ubuntu images, `offline` results and deployed identity in the operator record | NOT TESTED in this report |
| Privileged tool sandbox and service example verification | Three Ubuntu images, `sandbox` results | NOT TESTED in this report |
| Two isolated Ubuntu guests, visible identities, chat, search, result routing, CTM/RCM and reconnect | `two-client` on GitHub-hosted Ubuntu | Required standard CI |
| Foreign CA, wrong DNS SAN, time validity, incorrect/stale KEYP, rotation and rollback rehearsal | `tls-fixtures` across Ubuntu using synthetic certificates | NOT TESTED in this report |
| Malformed or oversize frames, unsupported commands, invalid fields and order, invalid SID, duplicate identity, permission denials and disclosure limits | Isolated full CTest and guest search/chat policy regression | NOT TESTED in this report |
| Twenty cycles, ten returns after refusal, idle disconnect/rejoin, abrupt TLS interruption and descriptor cleanup | `stability` across Ubuntu on loopback | NOT TESTED in this report |
| Thirty-minute public observation, paced cycles and same-identity reconnects | Exact-ref OWNER extended run | NOT TESTED in this report |
| OWNER declaration and fresh independent TXT challenge | [OWNER DNS evidence record](PUBLIC_STAGE_III_OWNER_DNS_AND_CI.md) | DNS ZONE CONTROL VERIFIED — PASS; subscriber identity OWNER-ATTESTED |
| Running service identity, SSH and read-only live-state inventory | OWNER Debian 13 read-only operator record | PASS for recorded scope |
| Actual replacement and recovery of the service-owned Let's Encrypt pair | [Manual replacement and rollback](PUBLIC_STAGE_III_CERTIFICATE.md) | NOT TESTED |

The negative protocol coverage reuses the existing ADC syntax, SID/identity,
login, authorization, search security, client close, protocol status, and
connection- and message-limit CTests. Controlled input failure, partial
message transmission and TLS interruption run on loopback. Public jobs use
bounded guest traffic and the fixed ADCS endpoint.

## Exposure and certificate evidence

The external source verifies TCP 1512 and probes TCP
`1511, 3000, 3306, 5173, 5432, 6379, 8000, 8080, 9090` once each.
An active refusal, a timeout, and an inconclusive network error have distinct
results. A timeout describes reachability from that runner during the test.
Review the operator's provider firewall and host listener inventory for a
complete exposure determination. Approved exceptions are TCP 1512 for ADCS,
restricted SSH from approved management sources, and the existing HTTPS/ACME
service where applicable. The OWNER records each actual exception and checks
that existing HTTPS routes exclude WebAdmin, development, and internal
administration endpoints. Account and database migration remain outside this gate.

The public certificate check uses system trust, exact SAN and SNI, and independent
OpenSSL and Python leaf fingerprint calculations. Packaged ncdc exercises its
separate KEYP mode through a fingerprint obtained only after verified TLS.
For actual pinned clients the OWNER supplies an independently authenticated
fingerprint. The synthetic TLS tests change only temporary loopback certificates.
The operator handles live Let's Encrypt renewal under the manual runbook.

## OWNER-authorized Debian 13.8 evidence

The local checks took place on November 26, 2026 UTC.
`dc24h-stage3-adcs.service` runs as `dc24h-stage3` and identifies the
deployed release as `beta-0.9.20` from
`7bd5600ea55c1914cf156f48d6a2250a64b6fa0f`. The on-disk and running
binary matched SHA-256 `f31795e77661fd03f68c8c6b51e7d6fd8a11ed776fe5319111101cb64b562082`.
This is the deployed identity. GitHub qualification records identify the
candidate test source separately.

| Check | Status | Evidence |
| --- | --- | --- |
| Exact service and read-only configuration validation | PASS | Active unit, successful service-account configuration check, no drop-ins or automatic restarts. ADCS-only bind, database, registered authentication, and API disabled in the hub configuration. |
| Local DNS and listening sockets | PASS | Both certificate names resolved to the directly assigned IPv4 `62.171.156.32`. TCP 1512 listened on that address. TCP 1511 had no listener. MariaDB listened on IPv4 loopback only. |
| HTTPS administrative routes from the VPS | PASS | The approved HTTPS vhost returned HTTP 404 for `/remote.html`, `/remote/v1/summary`, `/admin-api/v1/status`, and `/api/v1/status`. |
| Independent public TLS, TCP exposure, and guest communication | PASS | Historical exact-commit scope `7c961238066e3de2bef5ab9303fd3945a4b85157`. The independent Ubuntu 24.04 run passed the bounded TCP matrix, real ncdc, and two-guest cases. [Regular evidence](https://github.com/adrianf87/dc24h.eu-twilight/actions/runs/45898438109/artifacts/15154663771). |
| OWNER extended public stability | PASS | Historical exact-commit scope `7c961238066e3de2bef5ab9303fd3945a4b85157`. Observed 20 guest cycles, 19 same-identity reconnects, and 1,800 seconds of two authenticated sessions. [Extended evidence](https://github.com/adrianf87/dc24h.eu-twilight/actions/runs/45900205211/artifacts/15157510321). |
| OWNER declaration and independently confirmed TXT challenge | DNS ZONE CONTROL VERIFIED — PASS | Private registrar and DNS management screenshots support OWNER attestation; a fresh TXT token matched exact values through Google DNS and Cloudflare DNS at 2026-11-26 23:39:32 UTC and in a later OWNER shell screenshot. |
| Latest-SHA public A/PTR/ADCS and additional provider ingress tests | EXCLUDED BY OWNER | No execution required for Stage III; unresolved provider exposure is a documented risk, not a PASS. |
| Current production certificate and final TLS from the VPS | PASS | Deployed and Let's Encrypt lineage leaves shared the same fingerprint. Each key matched its leaf. CA chain, SAN and hostname verification passed. The final handshake negotiated TLS 1.3. Issuer Let's Encrypt YE2, expiration February 16, 2027 at 02:56:52 UTC. Leaf SHA-256 `518618f863566cbb4195645e48daa6c9a30bdb6d9f29d64a8999b5e1d248de6a`. |
| Renewal configuration inspection and ACME staging rehearsal | PASS | The installed renewal profile uses the Apache authenticator and installer. Certbot 4.0.0 separately completed a staging dry run with the existing webroot and both SANs, using private temporary state and with directory hooks disabled. The production certificate fingerprint and Apache and hub process identities stayed unchanged. |
| Staged restore and wrong-key rejection on Debian 13 | PASS | A byte-exact temporary backup was restored and validated under the hub service account. The configuration checker rejected a mismatched key, then accepted the restored pair. The live hub remained active. |
| Future Apache-plugin renewal and production pair replacement/rollback | OUT OF STAGE III SCOPE | The staging rehearsal used the webroot method and the current pair is valid. A future live replacement requires separately authorized maintenance; the withdrawn controlled-restart test is not an acceptance gate. |
| Real DC++ on an independent machine | EXCLUDED BY OWNER | No execution required for Stage III; no interoperability PASS is claimed. |

Pull requests and pushes to `main` produce isolated Ubuntu CI evidence for
the exact source commit. The CI report marks the public checks
`OUT OF SCOPE BY OWNER`. Prior public snapshot and extended stability runs
remain historical evidence.

## Evidence review

Open the [pre-merge workflow and evidence steps](PUBLIC_STAGE_III_PREMERGE.md).
For the PR run, download the exact-attempt `stage3-qualification-report-<attempt>`
artifact and the case bundles it names. Each CI case bundle contains JSON, JUnit,
a sanitized result log, and `SHA256SUMS`. The evidence job validates the
bundle contents, source SHA, runner image, run ID, run attempt and expected
case inventory. Compare the published Actions artifact digest when retrieving
the artifact archive. Preserve reviewed copies with the manual evidence.

Issue #10 remains open pending the completion of existing GitHub-hosted CI for the actual PR #24 head and explicit `OWNER STAGE III ACCEPTANCE GO`. A separate `OWNER MERGE GO` is still required before any merge. The withdrawn external DNS/ADCS, DC++, Contabo provider checks and controlled restarts are not to be performed or reintroduced as Stage III gates.
