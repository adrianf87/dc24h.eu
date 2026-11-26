# Public deployment Stage I — Debian 13 / ncdc / ADCS evidence

**Issue:** [#10](https://github.com/adrianf87/dc24h.eu-twilight/issues/10), Stage I (P0).
**Scope:** process-test coverage and client TLS trust diagnosis; no public listener, production migration or release permission.
**Status:** engineering follow-up; **production/public activation NO-GO** until separate OWNER acceptance and real-host retest.

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
- A correct keyprint learned from an untrusted channel is **not** independent server authentication. An attacker-controlled initial URI could carry an attacker-controlled keyprint. Distribute fingerprints/URLs by a trusted channel and record the OWNER-approved policy.

For public operation, prefer a DNS name, a current CA-signed certificate and correct DNS SAN; optionally publish a separately authenticated keyprint for compatible ADC clients. Do not disable certificate checks or silently downgrade to plaintext ADC to make a client connect. Certificate renewal changes a leaf keyprint and requires a planned ncdc pin update when using an exact leaf pin.

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
| ncdc with independently verified, matching SHA-256 keyprint | Connect regardless of SAN if this client implements KEYP pin-only trust | Incoming result only; policy requires OWNER decision |
| ncdc with **wrong/stale** keyprint or changed leaf | Refuse | Incoming result only; rerun pending |
| Invalid/expired/future certificate, bad key, weak TLS | Fail closed | Existing server tests; rerun pending |
| Incorrect password, reconnect, two users, chat, SCH/CTM/RCM, stability | Expected allowed/refused results | Incoming local evidence; real-host rerun pending |

**Closure criteria:** record final source SHA, exact Debian/kernel/OpenSSL/ncdc versions, full prompt and redacted raw logs/CTest XML, SHA-256 digests, accepted trust policy, complete negative outcomes, and real-host repetition. CI success on Ubuntu does not certify Debian 13. Any public hub/WebAdmin activation requires separate OWNER GO.
