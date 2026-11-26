# Public deployment Stage III — ADCS exposure and identity preflight (P0)

**Tracking:** [Issue #10](https://github.com/adrianf87/dc24h.eu-twilight/issues/10), Stage III.  
**Status:** IN PROGRESS — OWNER has requested public ADCS activation on the named Debian 13 VPS; readiness and Internet reachability remain unverified, **not live yet**.  
**Starting source:** protected main commit 7e46787c8a0a901fa08b5af6bb3cac7c8eb77a2d, after the [Stage II OWNER acceptance](https://github.com/adrianf87/dc24h.eu-twilight/issues/10#issuecomment-7195123564).  
**OWNER-selected public hub FQDN:** server-747585786.dc24h.eu (**DNS A record, zone control and live certificate still require independent verification**).  
**OWNER-selected public IPv4:** 62.171.156.32 (the Debian 13 VPS's reported address; direct assignment/NAT and external reachability must still be verified).  
**OWNER topology override:** The public hub is now requested **on the same Debian 13 VPS as the GitHub self-hosted runner**. The previous assumption of a dedicated, secret-free test-only VPS no longer covers this configuration. The self-hosted account has passwordless root sudo; the OWNER **explicitly accepted this co-location risk** on November 26, 2026. This does not make real private credentials safe: Stage III first uses guest access with database/authentication disabled and must keep private state outside the rollout.  
**Retired historical deployment target — never use:** 51.255.80.233 / server-759505093.dc24h.eu.

The OWNER has requested actual public ADCS activation at the specified new target. This runbook is only the pre-deployment safety gate; it does not itself change a VPS firewall, provider ACL, DNS record, TLS private key, service unit, process, or listener. The existing systemd example requires a separate OWNER migration marker and approval; do not create the marker, enable a unit, or stop a legacy service as a side effect of Stage III planning.

## 1. OWNER decisions and blocking dependencies

The OWNER has named the host, approved ADCS/TLS TCP 1512 with guest access and accepted the existing privileged-runner co-location risk. Record and verify the following before carrying out changes; a request is not proof that TLS, credentials, firewall or rollback prerequisites are met:

| Decision / evidence | Required value | Current disposition |
| --- | --- | --- |
| Public hub IPv4 | Verify the OWNER-selected 62.171.156.32 on server-747585786, including any NAT/provider forwarding | OWNER selected **62.171.156.32**; actual host bind and external route **not yet verified** |
| Hostname | Exact OWNER-controlled FQDN to place in DNS A record and certificate SAN | OWNER selected **server-747585786.dc24h.eu**; zone ownership, current A record and SAN still unverified |
| DNS authority | Proven control of the chosen DNS zone and intended TTL | **BLOCKED** |
| Network path | Direct assignment vs. provider NAT/port forwarding; external reachability | **BLOCKED** |
| ADCS port | TCP 1512 | **OWNER approved** for public ADCS-only access; live firewall/listener still unverified |
| Plaintext ADC | No public TCP 1511 | **OWNER approved TLS-only ADCS**; guest users may connect over the verified TLS transport |
| Certificates | Publicly trusted CA, correct DNS SAN, valid full chain, matching key, renewal/restart owner | **BLOCKED** |
| External testing | OWNER-controlled independent network and authorized bounded scan scope | **BLOCKED** |
| Public activation | Exact named service, host, port, source SHA, certificate, guest-login policy, fallback and rollback plan | OWNER requested activation and **accepted runner/sudo co-location risk**; execution still blocked by independent DNS/TLS/firewall/rollback readiness checks |

The internal WebAdmin/C++ value **server.name = dc24h.eu-twilight** is not a DNS hostname and must not be used as a certificate identity, Origin, or WebAuthn RP ID merely because its text resembles a domain. WebAdmin Internet exposure is a **different** OWNER decision and is not part of this gate.

## 2. Current implementation capabilities and constraints

- [config/dc24h.ini.example](../../config/dc24h.ini.example) defaults to loopback plaintext ADC (127.0.0.1:1511); ADCS is commented out. A future TLS-only profile uses **listen = none** plus an explicit **adcs_listen**. Neither is automatically installed or enabled.
- [ADCS/TLS behavior](ADCS_TLS.md): the C++ service accepts explicit certificate and matching private-key files, uses TLS 1.2/1.3 and checks key correspondence before binding. It does **not** obtain certificates, perform ACME challenges, hot-reload TLS material, or implement renewal. Plan a controlled validation and restart for renewal.
- [Stage I trust decision](PUBLIC_STAGE_I_NCDC.md): public server certificate must chain to a trusted public CA and contain the exact approved DNS SAN. An optional, independently verified ncdc KEYP pin is a distinct trust mode; a pin-only result must never be reported as simultaneous CA + hostname verification.
- [Hub systemd example](../../config/dc24h-twilight.service.example) has Conflicts=dc24h.service and an OWNER cutover marker. Stage II tested disposable unit isolation and rollback, **not** activation of the real hub unit.
- MariaDB, internal API/Unix sockets, Node/Vite, maintenance controller, and WebAdmin gateway remain private; no implicit Internet exposure. A browser URL beginning with **https://** is Web HTTPS, not an ADC/ADCS hub connection: ncdc and other DC clients require an **adcs://server-747585786.dc24h.eu:<approved-port>** endpoint.

## 3. Read-only host and provider inventory — before any modification

Run these *manually* only on the OWNER-selected Debian 13 VPS **server-747585786**, which is also the self-hosted GitHub runner. The previous isolation assumption no longer holds for public service deployment. The commands are observations, not a deploy procedure. Keep unredacted network, process, host and firewall output private; publish only bounded summaries. Do not infer the target public IP from an outbound route.

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

Document whether 62.171.156.32 appears on an actual host interface. If not, obtain a **provider-side, OWNER-verified NAT/forwarding mapping** before proceeding. Capture the provider's inbound policy separately; a Debian firewall listing alone cannot establish external reachability. Confirm no unknown dc24h process/service is active before any later rehearsal.

Record a proposed least-privilege inbound policy for **one selected ADCS TCP port**, restricted SSH management from OWNER-approved sources, and deny-by-default for all other inbound ports. Specifically forbid Internet access to MariaDB (3306), internal API/admin sockets, Node/Vite development ports, remote.html, and the maintenance/service controller. Do not edit nftables/iptables/ufw, provider firewalls, router NAT, or SSH policy until an OWNER-reviewed change plan preserves SSH/runner access and the exact ADCS port is confirmed.

## 4. DNS, TLS certificate and renewal gate

The OWNER has selected **server-747585786.dc24h.eu** as the hub name; before requesting a public certificate, verify the OWNER's DNS-zone control and obtain approval for its exact A record, target host and port. Resolve it from multiple independent recursive resolvers and record results, TTL, authoritative zone and change timestamp. The DNS value must resolve to the approved reachable public endpoint (direct or NAT), not merely to a machine's outbound address.

Example read-only DNS command for the OWNER-nominated name (this checks a response; it does **not** independently establish DNS ownership or authorize changes):

~~~sh
stage3_fqdn='server-747585786.dc24h.eu'
dig +short A "$stage3_fqdn"
~~~

For a future public certificate, verify exact DNS SAN, issuer, validity dates, full trusted chain and private-key correspondence **without printing private-key material**. Keep keys in a private service-readable path outside Git and artifacts. Plan renewal monitoring with an expiry alarm, validation of renewed material, safe restart, rollback to last known good cert, and independently authenticated KEYP rotation for pinned clients. A valid private test CA or a self-signed certificate is **not** sufficient for public acceptance.

After separately authorized activation, verify the live connection from a different network with an explicitly named SNI/hostname, for example:

~~~sh
# Only after the exact hostname, port and public activation are approved:
openssl s_client -connect "$stage3_fqdn:1512" -servername "$stage3_fqdn" \
  -verify_hostname "$stage3_fqdn" -verify_return_error -brief </dev/null
~~~

TCP 1512 is the OWNER-approved candidate; the listener remains disabled until DNS, TLS, routing and firewall gates pass. A successful TLS handshake alone does not prove ADC login or hub features; verify those separately.

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

Required report: exact commit SHA and binary version, independently verified mapping for server-747585786.dc24h.eu and 62.171.156.32 on the actual shared hub/runner VPS, OWNER-approved port, proof of DNS control and public-IP topology, TLS certificate SAN/issuer/expiry and **public** fingerprint (never private key), provider and local firewall decisions, pre/post network observations, external source/network, test matrix with negative results, redacted logs, rollback procedure, and artifact checksums. Document any unresolved failure as **FAIL / NO-GO**.

**Stage III disposition: LOCAL PREFLIGHT PASS / PUBLIC ACTIVATION PENDING.** OWNER-only read-only [preflight #1](https://github.com/adrianf87/dc24h.eu-twilight/actions/runs/45852400469) passed on merged commit `ad4bec0` with `LOCAL_PREREQUISITES_CLEAR`. On November 26, 2026 OWNER reported that provider TCP 1512 is allowed and no local host firewall is configured, leaving all listening ports potentially Internet-accessible. That is an OWNER report, **not** an independent scan or proof that other internal services are inaccessible. A separately reviewed and merged, manually dispatched activation workflow and independent external verification are required; no WebAdmin or private-state migration is approved.


## 8. Separate guarded Stage III activation proposal (review before merge)

The `.github/workflows/debian13-stage3-adcs-activation.yml` proposal is **not a deployment triggered by a PR or a push**. It can be dispatched only by `adrianf87` on protected `main` after a separate OWNER merge decision. Its activation mode requires the exact `RUN_STAGE3_PUBLIC_ADCS_ACTIVATION` token and two explicit attestations: provider TCP 1512/management access, and a named certificate renewal/restart operator. The current read-only preflight remains unchanged.

The host phase builds the exact checked-out C++ source without root, rechecks the ten read-only prerequisites, and uses passwordless `sudo` only for the separately guarded deployment script. It installs **only new owned Stage III paths**:

- `/opt/dc24h-stage3/<12-character source SHA>/bin/dc24h-twilight` — exact-source hub binary (never replaces an existing `/usr/local/bin` target).
- `/etc/dc24h-stage3/dc24h.ini` and a private copy of the already validated CA full chain and matching key under `/etc/dc24h-stage3/tls/`. No private data is uploaded to Actions.
- `/etc/systemd/system/dc24h-stage3-adcs.service` — unique, unprivileged `dc24h-stage3` service with systemd sandboxing. It does not stop `dc24h.service` or `dc24h-twilight.service` and does not change SSH, provider ACL, nftables, iptables or ufw.

The generated policy uses `listen = none`, binds ADCS to **62.171.156.32:1512** (never `0.0.0.0`), and disables MariaDB, registered-account authentication and internal HTTP API. There is no public WebAdmin activation. Guest connections are rate-limited. The code refuses any pre-existing Stage III installation target rather than overwriting or migrating it.

After startup, a **GitHub-hosted Ubuntu 24.04** job independently verifies the exact DNS A mapping, CA/SAN/TLS hostname trust, **two bounded ADC guest logins over TLS**, and that TCP 1511 refuses incoming connections. A failed external gate triggers a separate self-hosted rollback job. Rollback stops/disables the unique unit and removes only manifest-owned files; it does not recursively delete other host data or revert unrelated services. If the self-hosted runner is offline or the workflow is canceled, the operator must use the manual rollback mode and verify the listener is gone; automation alone is not a physical recovery guarantee.

**Manual rollback:** dispatch the same workflow on protected `main` with mode `rollback`, confirmation `RUN_STAGE3_PUBLIC_ADCS_ROLLBACK`, and the **full deployed source SHA** from the deployment record. The rollback job verifies the manifest's exact identity before deleting anything. Never re-run activation blindly when a deployment already exists.

**Remaining acceptance limits:** This initial bounded check is not a full port inventory, provider ACL audit, actual ncdc/DC++ compatibility matrix, 24-hour stability test, certificate renewal rehearsal, or proof that all other listeners are private. With no firewall, inspect and restrict any unrelated public sockets before calling the VPS production-hardened. Configure and test certificate renewal/restart as a separate operational prerequisite; copied key material does not renew itself. Only after those tests, complete rollback evidence, and a distinct **OWNER PUBLIC HUB ACTIVATION GO** may Stage III be formally accepted.