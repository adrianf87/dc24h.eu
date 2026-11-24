# Beta-0.8.1–0.8.4 — Stage I ADC/ADCS interoperability ledger

Issue [#240](https://github.com/adrianf87/dc24h.eu-high-starter/issues/240); one branch `beta-0.8.x-stage-full` and one draft PR to protected `main` for all five stages and five later fix passes. **This document records Stage I implementation only. It is not hosted CI acceptance, authorization to merge, a production release or a promise of native DC++/AirDC++ certification.**

## Immutable baseline and scope

- Branch base: protected `main` at `20eae07ada1645e7caa11e0b1c3cd0d7c15b8a1f` (completed beta-0.7 PR #239). Record the pushed head separately from the merge base.
- Stage I is exactly `0.8.1`–`0.8.4`; no browser, six-component recovery, packaging or Stage II–V implementation is authorized here. Existing beta-0.7 behavior is preserved.
- C++20 remains the ADC/ADCS password and role authority; MariaDB schema v11, five owner-only Node stores, loopback-only gateway and MFA boundaries do not change.
- Hosted evidence must come from GitHub-hosted **Ubuntu 22.04, 24.04, 26.04** on the checked-out PR head. Debian, privately managed runners, Xeon, physical clients and production services are out of scope.

## Patch implementation and negative evidence

| Patch | Implemented on the development branch | Acceptance still required |
| --- | --- | --- |
| **0.8.1 — External client identity** | `scripts/beta08_stage1_gate.py` composes the existing SHA-pinned ncdc .deb and executable verification with a new four-patch CTest matrix and strict extra-client `not_executed` inventory. `tests/beta08_stage1_gate_tests.py` refuses wrong head, bad digests, missing/duplicate/skipped/nonfinite tests and forged DC++/AirDC++ runs. | Actual CTest execution, verified exact-version ncdc package and SHA-256, source SHA matching the checked-out head, all mandatory gates. **DC++/AirDC++ remain untested**, because no version-pinned native executable is actually exercised in the permitted runners. No other independent client is advertised as certified. |
| **0.8.2 — Negotiation and account revocation** | Preserve real ncdc GPA/PAS, SUP/SID/INF/Unicode and malformed pre-auth tests. New `AuthenticationTests.test_beta08_revocation_is_enforced_after_controlled_hub_restart` disables a seeded account in an isolated MariaDB test, shuts down the C++ hub (closing its session), verifies fresh ADC and ADCS login refusals and confirms recovery after an explicit re-enable/restart. | Hosted MariaDB/real-hub CTest and no leaked credentials. **Not live revocation:** SQL account changes are startup snapshots; Owner must coordinate stop/change/restart and disconnect in their approved environment. |
| **0.8.3 — Search and transfer failure recovery** | Existing ncdc and real-protocol IPv4/IPv6/TTH/SCH/CTM/RCM/active-passive, token and limits tests remain required. New `SchProcessTests.test_beta08_search_reply_to_peer_connection_survives_bad_active_mode` pairs directed TTH search/response with a denied passive CTM, a valid TCP capability transition, directed CTM routing and unknown-SID refusal across ADC and ADCS. | Exact-head CTest execution, no bypass of peer identity/transport mode, redacted status/logs and successful recovery after denied requests. Unsupported NAT/platform paths stay explicitly unsupported. |
| **0.8.4 — Certificate cutover and downgrade** | Existing TLS 1.2/1.3 wrong-host, expired-leaf, legacy-cipher and bad-key tests stay required. New `TlsSecurityProcessTests.test_beta08_certificate_cutover_blocks_stale_trust_and_plaintext_on_both_versions` swaps disposable certificate generations with a controlled restart; rejects stale CA trust for both TLS versions, accepts new trust and rejects plaintext on the ADCS-only listener. | Exact-head hosted TLS CTest on all three Ubuntu images; never permit a plaintext fallback, fabricated client trust or public listener. |

## Strict hosted evidence and release boundaries

1. `build.yml` runs all CTest process cases on each Ubuntu Debug/Release matrix head, then retains the prior `beta07-client-capabilities.json` ncdc evidence with its apt-index/package/executable SHA-256 and actual results.
2. The new `beta08_stage1_gate.py` independently checks **every** mandatory case against CTest JUnit (with duplicate, skipped, nonfinite, failed and absent refusal), ties the ncdc report to the exact source SHA/image and creates a redacted `beta08-stage1-interoperability.json` artifact.
3. If any required test is unavailable, failed, skipped or run on a wrong head, Stage I **cannot be accepted**; a local synthetic PASS is not real CI evidence.
4. The existing CodeQL, ASan/UBSan, WebAdmin and Ubuntu VM checks remain required. **After commit/push and CI trigger, stop without waiting or polling.** Acceptance and a later fix pass require another explicit Owner instruction.
5. The product's pre-existing `beta-0.7.20` version identifiers are intentionally unchanged in Stage I. Candidate metadata alignment belongs to `beta-0.8.20`, not an early release claim.

## Residual limitations and operator reminder

- Native DC++/AirDC++ compatibility remains **untested**; a Python protocol fixture is not a separately shipped ADC client. Do not infer installed binary versions or platform support.
- The revocation regression tests a stop/change/restart boundary on disposable MariaDB. Existing sessions are not promised to lose access immediately upon SQL mutation without a hub restart. Only approved Owner/operator procedures may touch real account state.
- The scripted TLS rotation is private and disposable; it does not automate production ACME, unattended renewals or multi-node certificate rotation.
- IPv6, NAT and peer transfer behavior remains subject to explicitly exercised mode and host constraints. No home-network or production peer-to-peer result is claimed.

**Stage I status: code and local evidence tests added; hosted exact-head CI pending, not accepted.** No Stage II changes, production action, GitHub release, merge or issue closure.
