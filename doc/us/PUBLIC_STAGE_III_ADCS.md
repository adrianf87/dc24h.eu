# Public deployment Stage III — ADCS exposure and identity preflight (P0)

**Tracking:** [Issue #10](https://github.com/adrianf87/dc24h.eu-twilight/issues/10), Stage III.  
**Status:** IN PROGRESS — documentation and read-only preflight preparation only; **public deployment NO-GO**.  
**Starting source:** protected main commit 7e46787c8a0a901fa08b5af6bb3cac7c8eb77a2d, after the [Stage II OWNER acceptance](https://github.com/adrianf87/dc24h.eu-twilight/issues/10#issuecomment-7195123564).  
**OWNER-nominated hub FQDN:** server-759505093.dc24h.eu (**DNS A record, zone control, and live certificate not yet independently verified**).  
**Candidate hub IPv4:** 51.255.80.233 (the original Issue #10 target; OWNER has **not yet confirmed** that this IP belongs to the separate intended hub host or maps to this hostname).  
**Infrastructure separation:** The OWNER-designated self-hosted Debian 13 GitHub runner and isolated test VPS are **not** the public hub deployment target. Its network address and test-runner hostname are deliberately omitted from this public runbook. Do not activate ADCS on the test VPS by inference.

This document starts engineering work. Nothing here authorizes changing a VPS firewall, provider ACL, DNS record, TLS private key, service unit, process, or listener. The existing systemd example requires a separate OWNER migration marker and approval; do not create the marker, enable a unit, or stop a legacy service as a side effect of Stage III planning.

## 1. OWNER decisions and blocking dependencies

The OWNER has approved **starting Stage III preparation**, not yet a specific public activation. Record the following before preparing any actual public rollout:

| Decision / evidence | Required value | Current disposition |
| --- | --- | --- |
| Public hub IPv4 | Confirm the actual public address of the host intended to serve the ADCS hub (51.255.80.233 is the earlier candidate) | **BLOCKED: OWNER must confirm target IP and actual host; do not use the test VPS IP** |
| Hostname | Exact OWNER-controlled FQDN to place in DNS A record and certificate SAN | OWNER nominated **server-759505093.dc24h.eu**; zone ownership, current A record and SAN still unverified |
| DNS authority | Proven control of the chosen DNS zone and intended TTL | **BLOCKED** |
| Network path | Direct assignment vs. provider NAT/port forwarding; external reachability | **BLOCKED** |
| ADCS port | Explicit TCP port, suggested 1512 only as a proposal | **BLOCKED: no final port decision** |
| Plaintext ADC | TLS-only preferred; do not expose TCP 1511 without a separate risk acceptance | TLS-only proposed; public binding not approved |
| Certificates | Publicly trusted CA, correct DNS SAN, valid full chain, matching key, renewal/restart owner | **BLOCKED** |
| External testing | OWNER-controlled independent network and authorized bounded scan scope | **BLOCKED** |
| Production activation | Exact named services, ports, hostname, source SHA, fallback plan and rollback approval | Separate OWNER PUBLIC HUB ACTIVATION GO required |

The internal WebAdmin/C++ value **server.name = dc24h.eu-twilight** is not a DNS hostname and must not be used as a certificate identity, Origin, or WebAuthn RP ID merely because its text resembles a domain. WebAdmin Internet exposure is a **different** OWNER decision and is not part of this gate.

## 2. Current implementation capabilities and constraints

- [config/dc24h.ini.example](../../config/dc24h.ini.example) defaults to loopback plaintext ADC (127.0.0.1:1511); ADCS is commented out. A future TLS-only profile uses **listen = none** plus an explicit **adcs_listen**. Neither is automatically installed or enabled.
- [ADCS/TLS behavior](ADCS_TLS.md): the C++ service accepts explicit certificate and matching private-key files, uses TLS 1.2/1.3 and checks key correspondence before binding. It does **not** obtain certificates, perform ACME challenges, hot-reload TLS material, or implement renewal. Plan a controlled validation and restart for renewal.
- [Stage I trust decision](PUBLIC_STAGE_I_NCDC.md): public server certificate must chain to a trusted public CA and contain the exact approved DNS SAN. An optional, independently verified ncdc KEYP pin is a distinct trust mode; a pin-only result must never be reported as simultaneous CA + hostname verification.
- [Hub systemd example](../../config/dc24h-twilight.service.example) has Conflicts=dc24h.service and an OWNER cutover marker. Stage II tested disposable unit isolation and rollback, **not** activation of the real hub unit.
- MariaDB, internal API/Unix sockets, Node/Vite, maintenance controller, and WebAdmin gateway remain private; no implicit Internet exposure.

## 3. Read-only host and provider inventory — before any modification

Run these *manually* only on the separately identified OWNER-approved **future public hub host**, not on the isolated self-hosted test runner as a substitute. The commands are observations, not a deploy procedure. Keep unredacted network, process, host and firewall output private; publish only bounded summaries. Do not infer the target public IP from an outbound route.

~~~sh
cat /etc/os-release
ip -4 -brief address show
ip -4 route show
ip -4 route get 1.1.1.1
sudo -n systemctl is-active dc24h.service || true
sudo -n systemctl is-active dc24h-twilight.service || true
sudo -n ss -lntp
sudo -n nft list ruleset
~~~

Document whether 51.255.80.233 appears on an actual host interface. If not, obtain a **provider-side, OWNER-verified NAT/forwarding mapping** before proceeding. Capture the provider's inbound policy separately; a Debian firewall listing alone cannot establish external reachability. Confirm no unknown dc24h process/service is active before any later rehearsal.

Record a proposed least-privilege inbound policy for **one selected ADCS TCP port**, restricted SSH management from OWNER-approved sources, and deny-by-default for all other inbound ports. Specifically forbid Internet access to MariaDB (3306), internal API/admin sockets, Node/Vite development ports, remote.html, and the maintenance/service controller. Do not edit nftables/iptables/ufw, provider firewalls, router NAT, or SSH policy under this preflight authorization.

## 4. DNS, TLS certificate and renewal gate

The OWNER has nominated **server-759505093.dc24h.eu** as the hub name; before requesting a public certificate, verify the OWNER's DNS-zone control and obtain approval for its exact A record, target host and port. Resolve it from multiple independent recursive resolvers and record results, TTL, authoritative zone and change timestamp. The DNS value must resolve to the approved reachable public endpoint (direct or NAT), not merely to a machine's outbound address.

Example read-only DNS command for the OWNER-nominated name (this checks a response; it does **not** independently establish DNS ownership or authorize changes):

~~~sh
stage3_fqdn='server-759505093.dc24h.eu'
dig +short A "$stage3_fqdn"
~~~

For a future public certificate, verify exact DNS SAN, issuer, validity dates, full trusted chain and private-key correspondence **without printing private-key material**. Keep keys in a private service-readable path outside Git and artifacts. Plan renewal monitoring with an expiry alarm, validation of renewed material, safe restart, rollback to last known good cert, and independently authenticated KEYP rotation for pinned clients. A valid private test CA or a self-signed certificate is **not** sufficient for public acceptance.

After separately authorized activation, verify the live connection from a different network with an explicitly named SNI/hostname, for example:

~~~sh
# Only after the exact hostname, port and public activation are approved:
openssl s_client -connect "$stage3_fqdn:1512" -servername "$stage3_fqdn" \
  -verify_hostname "$stage3_fqdn" -verify_return_error -brief </dev/null
~~~

Port 1512 above is illustrative, not a confirmed deployment decision. A successful TLS handshake alone does not prove ADC login or hub features; verify those separately.

## 5. Candidate ADCS-only configuration (review-only; DO NOT APPLY)

Once the FQDN, route, port and CA identity are approved, prepare a **private** configuration based on [the existing INI](../../config/dc24h.ini.example) and use the binary's **--check-config** validation before an independently approved service start.

~~~ini
[network]
listen = none
# Replace the following example with the specific approved bind interface and port.
# adcs_listen = 127.0.0.1:1512

[tls]
# Supply actual private service-readable files, never commit keys/cert contents.
# certificate = /etc/dc24h/tls/fullchain.pem
# private_key = /etc/dc24h/tls/privkey.pem

[database]
enabled = off

[authentication]
enabled = off
~~~

The snippet is **deliberately non-runnable** and incomplete as a deployment configuration. Choose a concrete bind address using verified direct/NAT topology; do not casually substitute 0.0.0.0 or bind a publicly reachable interface. Do not open plaintext ADC as a fallback. Authentication/data migration need their own OWNER decisions.

## 6. External acceptance matrix — execute only after separate public test authorization

Use an authorized independent test network and pinned exact source SHA. Keep scan scope limited to OWNER-owned target IP and the approved port(s), with explicit stop criteria.

| Case | Expected outcome |
| --- | --- |
| Public FQDN A record, direct/NAT path and selected TCP ADCS port | Reachable only as approved |
| CA-trusted full chain, correct DNS SAN, validity, SNI | PASS in hostname-verifying client |
| Foreign/untrusted CA, wrong/missing SAN, expired/not-yet-valid certificate | REJECT in applicable CA+SAN trust mode |
| ncdc correct authenticated KEYP (optional) | PASS as **KEYP mode**, not CA+SAN |
| Incorrect/stale KEYP, unapproved replacement | REJECT |
| Plaintext ADC and unrelated TCP ports | NOT EXPOSED |
| ncdc ADCS login, two clients, SCH/DRES, chat, CTM/RCM, limits/negative inputs | PASS with bounded sanitized evidence |
| Certificate renewal/restart, pin rotation and failure rollback | PASS before operational approval |
| External bounded exposure scan | Only approved ports visible |
| MariaDB, local API, Node/Vite, WebAdmin and maintenance endpoints | NOT publicly reachable |

Never disable TLS validation, silently downgrade to adc://, or treat a trusted KEYP as proof of hostname verification. Record the client version and trust mode for each test.

## 7. Evidence and separate OWNER gates

Required report: exact commit SHA and binary version, independently verified mapping for server-759505093.dc24h.eu and the eventual public-hub IPv4 (not the test-runner IP), OWNER-approved port, proof of DNS control and public-IP topology, TLS certificate SAN/issuer/expiry and **public** fingerprint (never private key), provider and local firewall decisions, pre/post network observations, external source/network, test matrix with negative results, redacted logs, rollback procedure, and artifact checksums. Document any unresolved failure as **FAIL / NO-GO**.

**Stage III kickoff disposition: IN PROGRESS / ACTIVATION BLOCKED.** This PR changes documentation only. It neither dispatches GitHub Actions nor touches the VPS. No RELEASE GO, PUBLIC HUB ACTIVATION GO, WEBADMIN PUBLIC ACTIVATION GO, PRIVATE-STATE MIGRATION GO, or opening of public ports is implied. Such actions require their own exact-scope OWNER commands and post-change verification under [Issue #10](https://github.com/adrianf87/dc24h.eu-twilight/issues/10).
