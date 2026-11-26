# Stage III — OWNER DNS attestation and CI qualification evidence

**Date recorded:** November 27, 2026 (Europe/Warsaw).  
**Tracking:** [Issue #10](https://github.com/adrianf87/dc24h.eu-twilight/issues/10) and [PR #24](https://github.com/adrianf87/dc24h.eu-twilight/pull/24).  
**Public guest service:** `adcs://server-747585786.dc24h.eu:1512` at IPv4 `62.171.156.32`.  
**Disposition:** **DNS ZONE CONTROL VERIFIED — PASS** through a fresh TXT challenge at two independent public resolvers. Domain registrant identity remains **OWNER-ATTESTED** from private registrar screenshots. A/PTR compatibility is verified separately by the public CI suite.

## OWNER statement and supplied observations

The repository OWNER (`adrianf87`) explicitly states that `dc24h.eu` is their domain. Additional privately supplied registrar and DNS-zone management screenshots show the domain under the OWNER's account and the new TXT entry. This is OWNER-provided evidence of registration and DNS administration, not an independent identity check. Personal contact data and account identifiers are omitted.

1. **Command screenshot, supplied by OWNER:** The read-only command `host 62.171.156.32` returned a PTR mapping to `server-747585786.dc24h.eu`. The command `host server-747585786.dc24h.eu` returned the A address `62.171.156.32`.
2. **Contabo Customer Panel screenshot, supplied by OWNER:** **Network → DNS Management → Reverse DNS Management** displayed the same IP address and hostname in one row: `62.171.156.32` → `server-747585786.dc24h.eu`. The screenshot shows the Contabo reverse-DNS management view, **not** the forward-zone administration or domain registrar account.
3. The terminal prompt in the command screenshot identifies `server-759505093`, a **retired** host. This is solely where the read-only DNS commands were issued. It is **not** the public deployment target and must not be used for deployment or host maintenance.

The original screenshots were supplied in the OWNER conversation; they are **not** embedded in this public repository or included as independently hashed GitHub Actions artifacts. The exact mapping, its source, and the evidentiary limits are retained here without exposing a provider account session.

## Independent TXT challenge — PASS (November 26, 2026, 23:39:32 UTC)

The OWNER created a fresh TXT record under the managed `dc24h.eu` DNS zone. Earlier shell output during provider processing returned a legacy alias/SPF answer and was not accepted. A later OWNER shell screenshot returned the correct TXT value.

Two independent public recursive resolvers were queried over HTTPS:

| Field | Verified value |
| --- | --- |
| Query | `_dc24h-stage3-verify.dc24h.eu` TXT |
| Expected token | `dc24h-stage3-owner=f9defa5ca27e018bec03296bc11f11c15324600c` |
| Google Public DNS | `Status: 0`; exact token; TTL 300 — **PASS** |
| Cloudflare DNS | `Status: 0`; exact token; TTL 300 — **PASS** |
| Later OWNER `host -t TXT` screenshot | Exact token returned — **PASS** |

The Cloudflare response displayed DNS TXT quotation marks around the same value; the semantic token matches. This is an independently observed, fresh proof that the OWNER can publish records under `dc24h.eu` at the verification time. It does **not** independently authenticate the legal identity of the registrar account holder. Do not turn this one-time challenge into a permanent CI dependency. No service restart, DNS editing by CI, or VPS mutation occurred during verification.

## Evidence boundary: DNS records versus ownership

- **Supported by OWNER evidence:** Forward lookup of the selected hostname and reverse lookup of the selected IPv4 agree. The OWNER also demonstrates visibility of the IP's PTR entry in their Contabo Customer Panel and asserts ownership of the domain.
- **Now independently verified:** The freshly published TXT challenge demonstrates technical control of a record under `dc24h.eu`. Registrar identity remains OWNER-ATTESTED, not independently identity-checked. The existing A record was not edited, and provider firewall permissions are still a separate check.
- **Remaining if required for the broader Stage III gate:** Independently compare authoritative NS/A records and verify provider ingress and approved port exceptions. The OWNER supplied registrar screenshots privately; never publish account IDs, personal contact/billing data, passwords, or session cookies.
- **Acceptance notation:** `DNS ZONE CONTROL VERIFIED — PASS` for the TXT challenge; `OWNER-ATTESTED` for subscriber identity; `DNS FORWARD-REVERSE VERIFIED` only after the separate independent A/PTR CI case passes.

## Added automated DNS regression and external qualification

The Stage III `tls-public` evidence group now includes:

| Case | Expected outcome | Execution scope |
| --- | --- | --- |
| `dns.a` | Approved FQDN resolves exclusively to `62.171.156.32` | Independent public Ubuntu runner |
| `dns.ptr.forward-confirmed` | PTR for `62.171.156.32` equals `server-747585786.dc24h.eu`, and the PTR hostname resolves only to the approved IPv4 | Independent public Ubuntu runner |
| `tls.identity` / `tls.fingerprint` | Trusted public CA, exact hostname SAN/SNI and consistent certificate fingerprint | Independent public Ubuntu runner |
| `exposure` | Only approved public listener/explicit exceptions are exposed in the bounded TCP probe | Independent public Ubuntu runner |

The `dns.ptr.forward-confirmed` case fails closed on a mismatched PTR, a wrong forward mapping, or an inconclusive lookup. Offline unit regressions cover its valid, invalid and blocked outcomes. DNS checks are read-only; they do not change the zone or the VPS. A forward-confirmed PTR still cannot establish domain ownership.

On pull requests, the public external groups are **out of scope / NOT TESTED** by design. For the candidate source, the OWNER may run the existing external-only workflow dispatch `RUN_STAGE3_UBUNTU_EXTERNAL_ONLY` with the reviewed exact SHA on GitHub-hosted Ubuntu 24.04. An automatically green PR run alone is not an external DNS/TLS acceptance test.

## CI and acceptance checklist — keep exact-SHA evidence

- [ ] **Current PR head:** Obtain the completed GitHub Actions CI for the actual updated head SHA, not the earlier `0863f1bf820e34f64e7df0076fe51f98827ca81d`; do not reuse an older SHA as the current head.
- [ ] **Isolated Ubuntu matrix:** Require full CTest and Stage III offline/two-client/TLS fixtures/stability/systemd *fixture* verification on GitHub-hosted Ubuntu 22.04, 24.04 and 26.04. The temporary Ubuntu systemd fixture is not a production ADCS restart.
- [ ] **General required CI:** C++ Debug/Release, WebAdmin, address/undefined sanitizers, baseline policy, CodeQL, parser/fuzz and relevant Stage IV checks must finish successfully, with no unreviewed required failures.
- [ ] **Public external snapshot for exact source:** Review independent public DNS A/PTR, CA/SAN/TLS, forbidden-port exposure, real ncdc and two-guest evidence for the source under review. Link run URL, SHA, attempt and checksummed artifacts.
- [ ] **Real DC++ client:** Record version, independent network, trusted ADCS connection, two-client visibility, bidirectional chat, search and transfer signaling, reconnect and sanitized observer evidence.
- [x] **DNS record control:** The fresh OWNER TXT challenge was independently observed by Google DNS and Cloudflare DNS and confirmed in a later OWNER terminal screenshot.
- [ ] **Provider and remaining zone evidence:** Review authoritative NS/A records as required; verify Contabo ingress/approved SSH and HTTPS exceptions and investigate public reachability of host TCP/UDP 5355 if present.
- [ ] **Stability provenance:** The earlier OWNER-approved 30-minute public stability run was PASS at `7c961238066e3de2bef5ab9303fd3945a4b85157`. Keep that exact SHA and deployed binary identity in the evidence; do not claim it ran on a later head. Determine explicitly whether the historical evidence suffices for the unchanged production binary.
- [ ] **Decision:** Mark each case PASS, FAIL, BLOCKED, NOT TESTED or OWNER-ATTESTED only as supported. Separate `OWNER STAGE III ACCEPTANCE GO` and `OWNER MERGE GO` are required; PR #24 and Issue #10 remain open until their real gates are satisfied.

### Historical CI evidence, not evidence for future commits

At source `0863f1bf820e34f64e7df0076fe51f98827ca81d`, [Stage III qualification](https://github.com/adrianf87/dc24h.eu-twilight/actions/runs/45909397299), [C++/WebAdmin including UBSan](https://github.com/adrianf87/dc24h.eu-twilight/actions/runs/45909397330), [CodeQL](https://github.com/adrianf87/dc24h.eu-twilight/actions/runs/45909397313), [bounded fuzz/TSAN](https://github.com/adrianf87/dc24h.eu-twilight/actions/runs/45909397306), and [Stage IV offline audit](https://github.com/adrianf87/dc24h.eu-twilight/actions/runs/45909397323) completed successfully. [Stage IV isolated VM](https://github.com/adrianf87/dc24h.eu-twilight/actions/runs/45909397303) was still in progress at the recording check. Public external PR jobs were skipped by design. New documentation/test commits must be qualified on their own exact head.

## Explicit OWNER exclusion: no production restart qualification

Per the OWNER's November 26 decision, **do not reboot the Contabo VPS or deliberately restart the live `dc24h-stage3-adcs.service` for Stage III testing**. These withdrawn tests are not required, must not block Stage III, and cannot be reported as PASS. Future certificate renewal or incident response requiring a service restart is a separately authorized operational change. No live service, VPS, DNS, firewall or TLS material was changed by this evidence update.
