# Beta 0.8.17–0.8.20 — Stage V: evidence and release-candidate risk register

**Status: NO-GO for production, protected-main merge, tag, publication and operator activation.** Work unit 5 on the single `beta-0.8.x-stage-full` branch and draft [PR #241](https://github.com/adrianf87/dc24h.eu-high-starter/pull/241), tracked by [Issue #240](https://github.com/adrianf87/dc24h.eu-high-starter/issues/240). Code delivery and CI acceptance are separate. Stage I–IV changes, five subsequent fix passes and final Owner sign-off are not silently accepted here.

## 0.8.17 — authenticated GitHub CI evidence

`scripts/beta08_github_evidence.py` reads the authoritative GitHub REST API with a **read-only** Actions token *after* the workflows complete. It selects the latest required pull-request run at the exact source SHA on the approved branch and verifies each exact run attempt's successful, non-skipped GitHub-hosted job matrix. The C++/WebAdmin/sanitizer (3 Ubuntu images), isolated VM (3 images), CodeQL (3 images) and new Ubuntu 24.04 TSAN/fuzz workflow are all required. Each uploaded redacted result must belong to the selected run/attempt, be unexpired and carry a GitHub-provided SHA-256 artifact digest. Missing digest, skipped job, newer failed run, wrong SHA, wrong attempt or privately managed runner labels **fail closed**. Run IDs, attempts, allowlisted artifact names and hashes only are output; authentication tokens and raw logs are never exported.

This verifier is intended for a **separate final-head check**, after all workflows succeed. The script does not make a GitHub API transcript cryptographically independent of GitHub or the token holder. Synthetic unit tests verify rejection logic; they never represent real CI results. Do not supply a manually constructed green manifest as acceptance.

## 0.8.18 — pinned transport envelopes

Keep the existing predeclared three windows on each GitHub-hosted Ubuntu 22.04/24.04/26.04 image:

| Transport | Requests/window | Concurrency | Predeclared p99 ceiling | Minimum successful RPS |
| --- | ---: | ---: | ---: | ---: |
| ADC loopback | 32 | 4 | 500 ms | 2.0 |
| ADCS verified TLS | 32 | 4 | 1000 ms | 2.0 |
| Private HTTPS password+TOTP | 8 | 2 | 2000 ms | 0.2 |

`beta08_transport_evidence.py` independently recomputes p50/p95/p99 and throughput from the original numeric sample windows via the existing strict beta-0.7 gate, then requires equality with the stored summary. It binds the source SHA, Ubuntu image, Release build, compiler/Node version, lockfile SHA-256, hashed inputs and predeclared policy digest. Missing, non-finite or altered values fail. Private credentials, actor/IP identifiers and raw HTTP responses do not enter the artifact. **ADC, ADCS and HTTPS workloads are not throughput-equivalent.** GitHub-hosted results are regression envelopes, not certified production capacity or SLA.

## 0.8.19 — bounded resource, parser and sanitizer evidence

- The existing disposable half-open TLS, slow peer, blocked output, restart, SIGTERM, recovery and queue tests remain mandatory in the Ubuntu VM matrix, plus ASan and UBSan in the C++ workflow.
- `beta08-hardening.yml` adds a **required** Ubuntu 24.04 C++ ThreadSanitizer job against parser/network/security unit targets. A runtime incompatibility, unsupported suppression, missing JUnit or sanitizer finding is a **failed/blocked gate**, never a passing skipped check.
- A separate Clang coverage-guided libFuzzer target exercises ADC frame parsing and stream decoding with synthetic seed inputs. Fixed seed `424242`, **1024 runs**, max input **4096 bytes**, 5-second per-input timeout, 512 MiB RSS and 180-second wall-time ceiling. Only a bounded, allowlisted successful-run summary is uploaded; crash inputs/raw traces are not published. An unavailable toolchain or failed run remains visible as a failure.

These smoke tests are deliberately short. They cannot prove absence of all parser, concurrency or exhaustion flaws on production hardware.

## 0.8.20 — candidate identity, dependencies and final gate

The C++ package/version, Node/React WebAdmin metadata, npm package+lock tuple, active version assertions, README and changelog identify **`beta-0.8.20`**. MariaDB remains schema **v11** plus five private owner-only stores. This is a development **candidate**, not a tag or deployable production package.

Hosted WebAdmin CI retains its production-only npm audit and now performs a **second full npm audit including dev/build-chain dependencies**. High/critical unreviewed findings, invalid network output or a mismatched head fail. The redacted report contains only aggregate severities, exact lock digest and an explicit **empty dependency-exception list**; no exception is invented by a tool. Pinned Actions, public npm lock inventory, CodeQL and sanitizer checks remain independently mandatory.

`scripts/beta08_release_candidate_gate.py` checks candidate metadata plus 8 redacted evidence categories × 3 Ubuntu images and the two Ubuntu 24.04 hardening results. Even a structurally positive offline output says `authenticated_github_api_verification_still_required=true`, `merge_authorized=false`. A later Owner-approved acceptance step must run the direct GitHub API checker on the **final** SHA and compare artifact origin and release evidence. No stage is accepted from a pending/synthetic result.

## Outstanding production-readiness decisions (Owner)

| Boundary | Still unresolved after this development candidate |
| --- | --- |
| External ADC clients | DC++/AirDC++ not certified without independent native execution; ncdc results are scoped to pinned versions |
| Revocation | C++ startup account snapshots require a controlled hub restart and old-session disconnection; instant live revocation is not designed |
| WebAuthn | Chromium virtual authenticators are not physical/synchronized passkey certification |
| Package/runtime | Isolated compiled Node runtime and dpkg rehearsal do not authorize production packaging, migration or long-lived systemd operation |
| Recovery | Offline MariaDB v11 + five-store matching is not a live atomic capture or established production RPO/RTO |
| Audit retention | 8 MiB audit boundary and stopped-writer archival must preserve the chain; audit loss fails authentication closed |
| CI/security | TSAN, fuzz, CodeQL, ASan/UBSan and dependency advisories need current-head successful hosted evidence; tooling incompatibility cannot be hidden |
| Complete dependency provenance | GitHub Action SHAs/npm lock are pinned; Ubuntu apt dependency versions follow the disposable runner image, not a separately immutable OS package SBOM |
| Post-merge scans | The protected-main CodeQL/security outcome cannot be claimed before a separately authorized merge and subsequent main-head verification |
| Capacity/availability | Short Ubuntu regressions do not measure Debian/Xeon capacity, production load, SLA or total DoS resistance |

## Owner handoff

1. Push Stage V to the same draft PR. Trigger/confirm GitHub-hosted Ubuntu-only CI, then **stop immediately without waiting for results**.
2. Later, retrieve real GitHub Actions statuses for the **exact final head** and authenticate run attempts/jobs/artifact SHA-256 with a read-only `GITHUB_TOKEN`: `python3 scripts/beta08_github_evidence.py --root . --sha <full-sha> --output <private-redacted-report>` (in a temporary directory; do not commit the report or token).
3. Assemble only sanitized artifact JSON in isolated directories and run the offline beta-0.8.20 candidate gate. Confirm no missed/skipped required check, unreviewed dependency advisory or false-green resource claim.
4. Complete **Fix Pass 1–5** as separate requests, then repeat exact-head verification. Only a later explicit Owner request may make PR #241 ready and authorize a protected-main merge. No deployment by this work unit.
