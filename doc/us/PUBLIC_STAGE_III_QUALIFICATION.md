# Stage III public ADCS qualification report

**STAGE III — NOT READY FOR FINAL ACCEPTANCE**

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

## Evidence matrix

The initial matrix records the disposition before exact-commit evidence review.
Use the Markdown result matrix and artifacts from the commit under review.

| Requirement | Exact-commit evidence | Initial disposition |
| --- | --- | --- |
| C++ build, CTest, config, security, and deployed binary identity | Three Ubuntu images, `offline` results and deployed identity in the operator record | NOT TESTED in this report |
| Privileged tool sandbox and service example verification | Three Ubuntu images, `sandbox` results | NOT TESTED in this report |
| DNS, public CA, SAN, SNI, validity, fingerprint, TLS 1.2/1.3 and downgrade denial | `tls-public` on independent Ubuntu | NOT TESTED in this report |
| Real ncdc login, active hub, inbound and observed outbound chat, search, reconnect and cleanup | `ncdc` on independent Ubuntu and the real loopback regression | NOT TESTED in this report |
| Two guests, visible identities, chat, directed/broadcast search, result routing, CTM/RCM and quit/rejoin | `two-client` across Ubuntu and `two-client-public` externally | NOT TESTED in this report |
| Public ADCS and bounded forbidden ports | `exposure` probe from independent Ubuntu | NOT TESTED in this report |
| Foreign CA, wrong DNS SAN, time validity, incorrect/stale KEYP, rotation and rollback rehearsal | `tls-fixtures` across Ubuntu using synthetic certificates | NOT TESTED in this report |
| Malformed or oversize frames, unsupported commands, invalid fields and order, invalid SID, duplicate identity, permission denials and disclosure limits | Isolated full CTest and guest search/chat policy regression | NOT TESTED in this report |
| Twenty cycles, ten returns after refusal, idle disconnect/rejoin, abrupt TLS interruption and descriptor cleanup | `stability` across Ubuntu on loopback | NOT TESTED in this report |
| Thirty-minute public observation, paced cycles and same-identity reconnects | Exact-ref OWNER extended run | NOT TESTED in this report |
| DNS-zone ownership and provider/Debian routing and inbound policy | OWNER provider and host record plus independent external check | NOT TESTED |
| Running service identity, SSH, controlled restart, external guest reconnect, chat and recovery readiness | [Debian 13 systemd procedure](PUBLIC_STAGE_III_SYSTEMD.md) | NOT TESTED |
| Actual replacement and recovery of the service-owned Let's Encrypt pair | [Manual replacement and rollback](PUBLIC_STAGE_III_CERTIFICATE.md) | NOT TESTED |
| Real DC++ interoperability | Independent DC++ client session and paired observer | NOT TESTED |

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

## Evidence review

Open the [pre-merge workflow and evidence steps](PUBLIC_STAGE_III_PREMERGE.md).
For the PR run, download the exact-attempt `stage3-qualification-report-<attempt>`
artifact and the case bundles it names. For the OWNER extended run, download
`stage3-extended-evidence-<attempt>`. Each case bundle contains JSON, JUnit,
a sanitized result log, and `SHA256SUMS`. The evidence job validates the
bundle contents, source SHA, runner image, run ID, run attempt and expected
case inventory. Compare the published Actions artifact digest when retrieving
the artifact archive. Preserve reviewed copies with the manual evidence.

For the required real DC++ check, use a fresh profile on a separate client
system and a second synthetic guest as observer. Record DC++ version and OS,
the hostname and trust mode, guest login and visible identity, bidirectional
public/private chat, search and result reception, applicable CTM/RCM signaling,
and disconnect/reconnect. Verify delivered chat in the independent observer.
Use only synthetic data and, if needed, a small synthetic share. Record the exact
deployed identity, qualification SHA, UTC interval, each result and sanitized
evidence checksum. Stop on a certificate warning, plaintext fallback, or absent
independent attribution.

Issue #10 remains open for the independent client, exposure, stability and
operator requirements above. Its accepted activation smoke is historical
evidence. Formal Stage III acceptance requires reviewed successful automated
and manual evidence for the intended scope and separate OWNER authorization.
