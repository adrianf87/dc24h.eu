# Public deployment Stage I — Debian 13 / ncdc / ADCS evidence

**Issue:** [#10](https://github.com/adrianf87/dc24h.eu-twilight/issues/10), Stage I (P0).
**Scope:** process-test coverage and client TLS trust diagnosis; no public listener, production migration or release permission.
**Status:** engineering follow-up; **production/public activation NO-GO** until separate OWNER acceptance and real-host retest.

## OWNER TLS trust policy — approved 2026-11-26

**Decision (applies to the future public ADCS hub, not to production activation):**

1. **Mandatory server-side deployment baseline:** use an OWNER-approved public DNS hostname, a current certificate issued by a publicly trusted CA, a complete verifiable chain and a `subjectAltName` containing the **exact DNS name used by clients**. The product identifier `server.name = dc24h.eu-twilight` is not the DNS hostname. The public IPv4 alone does not substitute for DNS identity; a literal-IP URL would require a matching **IP SAN** in clients enforcing IP-name validation. Never treat a mismatch as an accepted public-server configuration.
2. **Optional ncdc KEYP trust mode:** an operator/user may explicitly choose `adcs://<approved-host>:<approved-port>/?kp=SHA256/<base32-fingerprint>` **only after independently obtaining and confirming the exact certificate SHA-256 keyprint through an authenticated, secure out-of-band channel** (e.g. a separately verified OWNER-signed announcement). Verify the fingerprint character-for-character **before first use**. A URL, QR code or keyprint provided together by an unverified intermediary is **not** independent confirmation. Never retrieve the keyprint from the same unverified TLS connection and call it trusted.
3. **Distinct verification semantics:** observed ncdc 1.23.1 KEYP pin trust can authenticate the presented certificate even if its SAN differs. This is **not evidence of simultaneous CA + SAN validation**; acceptance of pin-only client behavior does **not waive** the public hub's mandatory CA + correct DNS SAN baseline.
4. **Fail closed and rotate safely:** reject missing/incorrect/stale pins, substituted or expired certificates under the applicable trust policy, and untrusted chains/name mismatches in CA+SAN clients. No automatic trust-on-first-use, bypass of TLS verification or silent downgrade to plain `adc://`. Track certificate renewal and distribute/independently authenticate the **new leaf-certificate fingerprint before switching**; test pinned clients again after rotation. Never put private keys in Git/CI logs.
5. **Owner's authorization is limited to this trust policy.** Choosing the hostname, proving DNS ownership, obtaining/deploying CA certificates, opening ports and running a public listener still require later Stage III work and separate OWNER activation GO.

**Acceptance evidence to collect:** exact DNS name, SAN and issuer/chain; trusted CA+matching SAN PASS; trusted CA+wrong SAN FAIL; untrusted CA FAIL; expiry/not-yet-valid FAIL; ncdc with an independently validated matching KEYP PASS; wrong/stale/replaced pin FAIL; safe rotation and no plaintext fallback; client version, test host and exact source SHA. Record whether a passing client used CA+SAN **or** KEYP. Negative tests are mandatory.

## Incoming evidence, not independently reproduced here

The redacted [Debian 13 + ncdc report](../../logs/tests/2026-11-26-debian13-ncdc-1f2ce42.redacted.log) was supplied by OWNER for the earlier `main` source `1f2ce425e192a71c6ca2499e9c613e5d8d14ba78`.

| Area | Reported outcome | Qualification meaning |
| --- | --- | --- |
| Release build, ADC, two ncdc profiles, stable reconnects | PASS | Local IPv4 evidence only; not an Internet deployment test. |
| Selected CTest batch | 40 PASS, 2 FAIL; 218 unselected | `adc.ctm.process` and `adc.rcm.process` attempted `bind(::1)` in an IPv4-only network namespace. Root cause not yet rerun on actual Debian 13. |
| ADCS | Marked FAIL by the source report | A reference TLS client rejected a DNS-only SAN for an IP URL, while ncdc accepted an **explicitly independently verified** SHA-256 keyprint. These are different trust models. |

The original Codex prompt, unredacted report, CTest XML and independent original artifact hashes beyond the report's supplied digest are **not yet available in this repository**. Do not mark them verified.

## Why a matching keyprint does not necessarily require a SAN match

ADC's [KEYP extension](https://dc-protocols.github.io/ADC-EXT.html#_keyp_certificate_substitution_protection_in_conjunction_with_adcs) authenticates the peer certificate by comparing its SHA-256 keyprint with an **already trusted** keyprint in the ADCS URL. The [ncdc manual](https://dev.yorhel.nl/ncdc/man) supports `?kp=SHA256/<base32-keyprint>` and warns when the supplied/stored keyprint changes. This is an explicit trust mode distinct from the usual CA + DNS/IP-name validation.

- A **CA/hostname reference TLS client** must reject a certificate containing only `DNS:localhost` when connecting using the literal `127.0.0.1` identity; it may accept that certificate using `localhost` if the trust chain is valid. A new process regression asserts both cases.
- An **ncdc client given a securely verified exact keyprint** may accept that same leaf by keyprint even when its SAN does not match the IP URL. This is not by itself proof of a hub TLS flaw, or proof that CA/hostname verification was performed.
- A **different leaf with the old pin** must be rejected. The incoming report says ncdc rejected a replacement certificate; this needs an independent repeat with exact client and server versions.
- A correct keyprint learned from an untrusted channel is **not** independent server authentication. An attacker-controlled initial URI could carry an attacker-controlled keyprint. Distribute fingerprints/URLs by a separate, independently authenticated channel under the OWNER policy recorded above.

For public operation, **require** an OWNER-approved DNS name, a current publicly trusted CA certificate and matching DNS SAN; KEYP is an optional, independently authenticated trust mechanism for compatible ADC clients. Do not disable certificate checks or silently downgrade to plaintext ADC to make a client connect. Certificate renewal changes a leaf keyprint and requires a planned ncdc pin update when using an exact leaf pin.

## IPv4-only versus full dual-stack process coverage

The CTM/RCM process tests now **always** exercise IPv4. Before the IPv6 scenarios, they probe a real `::1` socket bind:

- If IPv6 loopback exists, they execute **both** IPv4 and IPv6, with ADC and ADCS variants.
- If it is unavailable (`EADDRNOTAVAIL`, `EAFNOSUPPORT`, `EPROTONOSUPPORT`), they print `SKIP IPv6 ... IPv4 coverage only` and run IPv4. A passing CTest in this condition is **not** full dual-stack coverage.
- For a host expected to support IPv6, set `DC24H_REQUIRE_IPV6_LOOPBACK=1` to fail closed if `::1` is unavailable. Unexpected socket errors always fail; they are not converted into skips.
- `stage1.ipv6_loopback_policy` tests the probe and all the above decision paths.

Examples (on an **isolated** test host, with a locally built target):

```sh
ctest --test-dir build --output-on-failure -R '^(adc\.ctm\.process|adc\.rcm\.process|stage1\.ipv6_loopback_policy)$'
DC24H_REQUIRE_IPV6_LOOPBACK=1 ctest --test-dir build --output-on-failure -R '^(adc\.ctm\.process|adc\.rcm\.process)$'
ctest --test-dir build --output-on-failure -R '^adcs\.process$'
```

## Required independent acceptance matrix (all outcomes logged)

| Check | Expected | Evidence status |
| --- | --- | --- |
| IPv4 CTM/RCM (ADC + ADCS) in IPv4-only namespace | PASS; explicit IPv6 partial-coverage notice | Needs Debian 13 rerun |
| IPv6 CTM/RCM on an IPv6-capable host with strict flag | PASS; zero skipped IPv6 cases | Needs rerun |
| Reference client, valid CA/trust + matching DNS SAN | Connect | Automated regression added, CI pending |
| Reference client, valid CA/trust + **wrong IP SAN** | Refuse | Automated regression added, CI pending |
| ncdc with independently verified, matching SHA-256 keyprint | Connect even if this client uses pin-only trust instead of SAN checking | OWNER trust policy approved; Debian 13.8/ncdc 1.23.1 reported PASS; final-SHA/real-host retest pending |
| ncdc with **wrong/stale** keyprint or changed leaf | Refuse | Incoming result only; rerun pending |
| Invalid/expired/future certificate, bad key, weak TLS | Fail closed | Existing server tests; rerun pending |
| Incorrect password, reconnect, two users, chat, SCH/CTM/RCM, stability | Expected allowed/refused results | Incoming local evidence; real-host rerun pending |

**Additional Stage I report, 2026-11-26:** the [sanitized Debian 13.8 / ncdc 1.23.1 retest summary](../../logs/tests/2026-11-26-debian13-ncdc-stage-i-1bbcd54.summary.md) and [redacted log](../../logs/tests/2026-11-26-debian13-ncdc-stage-i-1bbcd54.redacted.log) report PASS for CA+SAN, strict IPv6 and correctly/wrongly pinned ncdc. A broad CTest attempt yielded **256 PASS / 4 FAIL** at umask 077; all four failed cases later passed individually at umask 022. Full identical CTest rerun and isolated `systemd.sandbox` remain outstanding. **STAGE I BLOCKED / PUBLIC NO-GO**.

**Closure criteria:** record final source SHA, exact Debian/kernel/OpenSSL/ncdc versions, full prompt and redacted raw logs/CTest XML, SHA-256 digests, accepted trust policy, complete negative outcomes, and real-host repetition. CI success on Ubuntu does not certify Debian 13. Any public hub/WebAdmin activation requires separate OWNER GO.
