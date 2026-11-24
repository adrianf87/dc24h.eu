# Beta 0.7.17–0.7.20 — Stage V release-candidate qualification and risk register

**Status: NO-GO for merge, production deployment or operator activation.** This document describes the development candidate on `beta-0.7.x-stage-full`, [Issue #238](https://github.com/adrianf87/dc24h.eu-high-starter/issues/238), draft [PR #239](https://github.com/adrianf87/dc24h.eu-high-starter/pull/239). An implementation commit, passing local unit tests or `CI pending` is **not** acceptance. All five later fix passes and the final Owner decision remain separate requirements.

## 0.7.17 — exact-head, three-window Ubuntu baselines

The existing `scripts/beta07_ubuntu_perf_matrix.py` accepts **exactly three** within-profile windows per GitHub-hosted Ubuntu 22.04, 24.04 and 26.04 image. Native ADC and ADCS: **32 requests/window, concurrency 4, configured rate 8/s**. Authenticated C++→Node→Apache HTTPS: **8 password+TOTP requests/window, concurrency 2**, spaced to avoid artificially defeating login limits. Each window must report p50/p95/p99 latency, elapsed time, success/error totals, successful RPS and CPU/RSS evidence. Native CPU and RSS come from the actual C++ test process; the HTTPS profile measures C++ account-admin/Node gateway/Apache process RSS with a summarized CPU observation. All three profiles are held to *predeclared hosted CI regression ceilings*, never to a claimed cross-protocol speed ratio.

The workload sizes and authentication costs **differ**, therefore comparing the resulting ADC, ADCS and HTTPS RPS numerically as if they were interchangeable is prohibited. A GitHub-hosted VM is not the Owner's Debian/Xeon deployment. No capacity, availability or latency SLA is implied. Required file: redacted `beta07-transport-performance.json` bound to source SHA and Ubuntu image.

**Fix Pass 5 hardening:** numeric-only loopback probe reports include sorted successful-request latency samples, allowing the gate to recompute p50/p95/p99 instead of trusting aggregate claims. Authenticated HTTPS separately records C++/Node/Apache CPU observations. The final release gate verifies numeric envelopes, fault-suite durations and the actual checked-out npm lock digest. See [Fix Pass 5 review](BETA_0.7_FIX_PASS_5.md); remote GitHub artifact authenticity still requires separate verification.

## 0.7.18 — bounded exhaustion and graceful drain

The disposable CTest suite exercises real half-open TLS, slow-read TCP/ADCS peers, pending output, connection bursts, rejection budgets, healthy concurrent peers, reconnect and repeated SIGTERM/drain. `scripts/beta07_bounded_fault_gate.py` rejects skipped, failed, duplicate or missing fault suites and observations over the predeclared **240-second per-case CI terminal envelope**. This is a coarse outer CI timeout **in addition** to the stricter socket/queue/heap bounds asserted by the real process tests. It records only fixed testcase names and execution seconds; no raw peer data, logs or credentials enter `beta07-bounded-faults.json`.

ASan/UBSan remain separate hosted CI gates. This is bounded disposable fault coverage, not a long-running production soak or a proof against every denial-of-service condition.

## 0.7.19 — provenance, SBOM and advisory policy

- `scripts/beta07_supply_chain.py` binds the checkout to an exact 40-hex SHA, a permitted Ubuntu image and full-length pinned GitHub Action references. All public npm lock entries must have an exact version, `registry.npmjs.org` resolution and SHA-512 integrity; unpinned/floating/direct drift or hashless transitive entries fail closed.
- The redacted **npm lockfile SBOM subset** lists public package names, exact versions and SHA-256 of each registry integrity token. CI does not export raw registry URLs, integrity tokens, credentials or source contents. The lockfile SHA-256, action pins and package counts are recorded.
- A separate hosted `npm audit --omit=dev --audit-level=high --json` run is checked by `scripts/beta07_npm_audit_gate.py`. Network errors, malformed/inconsistent totals and high/critical production dependency advisories **fail**. Only aggregate counts are uploaded. The raw audit JSON is temporary and deleted; no advisory text is published.
- Pinned C++/TypeScript CodeQL runs and ASan/UBSan continue on the exact PR head, separately from the npm gate. A report with a missing/wrong head, missing scan or secret-bearing raw data is not acceptable evidence.

## 0.7.20 — synchronized development version and final status gate

CMake C++ server/API/service-controller version, Node/React `WEBADMIN_VERSION`, test fixtures, npm package/lock tuple, README and changelog now identify **`beta-0.7.20`**. This is release-*candidate metadata only*; it is not an installed release or GitHub tag. MariaDB stays at schema **v11**. No public remote listener, grant-changing remote write, or passwordless login was introduced.

`scripts/beta07_stage1_completion_gate.py` requires an exact-head manifest covering **all 20 patches, 17 completed successful GitHub-hosted jobs and ten evidence categories**. The new `scripts/beta07_release_candidate_gate.py` additionally requires matching de-identified evidence files for all three Ubuntu images: locked package/SBOM provenance, advisory totals, transport windows, bounded faults, real `.deb` rollback, matched SQL/five-store recovery and integrated chaos. Missing, mismatched or explicitly production-claiming records fail.

**Critical trust limitation:** the offline scripts validate declared job status and file provenance but **cannot independently authenticate a supplied GitHub job manifest**. Operator/Owner must retrieve and cross-check the actual GitHub Actions API runs, job names, head SHA, run attempts and artifact origins before accepting the gate output. Even a positive offline structure check reports `github_run_authenticity_independently_verified=false` and `merge_authorized=false`. No CI result is inferred while checks are running.

## Residual risk register (Owner decision required)

| Risk | Current disposition / acceptance boundary |
| --- | --- |
| External ADC clients | Actual `ncdc` fixtures are version-pinned; DC++/AirDC++ are **not certified** unless independently executed |
| MariaDB and private state | Offline v11 + five-store pairing/restore is disposable; writer quiescence and production RPO/RTO are not established |
| Package rollback | A test-only inert `.deb` pre-unpack failure and downgrade are exercised; no production package-manager recovery claim |
| Authentication and WebAuthn | Password+TOTP or password+passkey only; Origin/RP ID, replay, revocation and account grants must remain enforced; no passwordless path |
| Throughput, memory and drain | Hosted short windows and bounded faults cannot predict actual Debian/Xeon load, outage or total memory exhaustion |
| Audit and incident freshness | Aggregate history is ephemeral and deliberately unknown after loss; it is not a durable forensics log or health certification |
| Supply chain and external scanners | npm/CodeQL may find new advisories; full runner/test provenance must still be reviewed on the final SHA |
| Deployment authorization | **NO-GO** until all exact-head CI checks, five separate fix passes and explicit Owner approval; no automatic merge/deploy |

## Handoff

1. Confirm the final implementation SHA and three hosted Ubuntu job matrices (C++ Debug/Release, browser/API, VM, CodeQL plus ASan/UBSan). Retrieve only approved redacted reports; do not upload private data.
2. Correlate their `source_sha`, image, workflow run, job status and artifacts with the final GitHub head. Run the offline final gate only after all required results are complete. Do not manufacture a positive manifest for a pending or skipped job.
3. Complete **Fix Pass 1–5** separately, then repeat the exact-final-head qualification after fixes. All passes remain on the one branch/PR and require separate requests.
4. The Owner decides whether to accept residual risk and authorize a subsequent merge/deployment. This implementation performs neither.

Per Owner instruction, after pushing the current implementation and triggering CI, **stop without polling for CI results**. The handoff remains **NO-GO** until explicit later review.
