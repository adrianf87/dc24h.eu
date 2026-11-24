# Beta 0.7 Fix Pass 5 — Stage V reliability, provenance and release qualification

**2026-11-24 — implementation handoff, not acceptance.** [Issue #238](https://github.com/adrianf87/dc24h.eu-high-starter/issues/238), single draft [PR #239](https://github.com/adrianf87/dc24h.eu-high-starter/pull/239), shared `beta-0.7.x-stage-full` branch. The final pushed SHA and GitHub Actions runs belong in the issue/PR handoff; this document intentionally does not claim a future CI result. **NO-GO** for ready-for-review, merge, tag, deployment or Owner security sign-off.

## Verified findings and corrections

| Patch | Finding | Repair and regression |
| --- | --- | --- |
| `0.7.17` | The aggregate gate accepted arbitrarily altered p50/p95/p99 values because it did not see individual request latencies. A single combined HTTPS CPU value could not be traced to all three C++/Node/Apache processes. | Synthetic loopback probes now report numeric sorted successful-request latency samples; the exact-head three-window gate requires 32 ADC/ADCS or 8 HTTPS samples per window and recomputes all three percentiles. HTTPS records per-process CPU and the gate checks that the aggregate equals their sum. Reject tampered samples, percentiles, process CPU, negative/non-finite observations and altered workload sizes. Raw peer information and credentials remain excluded. |
| `0.7.18` | The existing bounded real process tests cover TLS handshakes, slow readers, burst admission, blocked output, reconnect and SIGTERM. Their JUnit gate already rejects missing/duplicate/skipped/over-240s cases; no specific new reactor defect was reproduced. | The final release gate now checks the required fixed suite identities, **each** observed duration, terminal budget and redacted evidence flag instead of trusting only an `all_required_fault_cases_passed` Boolean. Synthetic missing-case and over-budget negatives added. |
| `0.7.19` | The action-pin inventory scanned `*.yml` but silently omitted valid GitHub workflow files ending in `*.yaml`; root npm manifest/lock version disagreement was also not rejected. | Inventory scans both extensions, refuses unpinned actions in either, and checks root name/version across npm package and lock. Release qualification also matches the supply report's SHA-256 directly against the checked-out lockfile and checks all SBOM integrity-hash shapes/counts. |
| `0.7.20` | The offline release gate accepted empty `{}` performance profiles and unbounded/placeholder fault evidence as long as top-level field names existed. | Enforce exact 3-window request totals, predeclared numeric envelopes, zero errors, passed per-profile flags, required fault-suite durations, 0.7.20 npm package/lock alignment and correct lock digest. Regression fixtures confirm false-green reports fail. |

## Reproducible local checks

- `python3 -m unittest discover -s tests -p beta07_ubuntu_perf_matrix_tests.py`
- `python3 -m unittest discover -s tests -p beta07_bounded_fault_gate_tests.py`
- `python3 -m unittest discover -s tests -p beta07_supply_chain_tests.py`
- `python3 -m unittest discover -s tests -p beta07_release_candidate_gate_tests.py`
- `python3 -m unittest discover -s tests -p stage4_perf_probe_tests.py`
- `git diff --check`

All listed synthetic/offline checks passed before the handoff commit. They do **not** replace executing C++/browser/ADC/ADCS on the Ubuntu hosts, sanitizers, CodeQL, package downgrade or matched SQL+five-store recovery. Those remain exact-final-head GitHub Actions checks, with no result inferred here.

## Residual constraints

- Latency samples and synthetic evidence can detect **internal contradictions**, not prove that a writer actually performed a measurement; authenticate job/artifact origin separately against GitHub Actions and the source SHA. Different ADC, ADCS and password+TOTP HTTPS workloads must never be compared as equivalent throughput or presented as production SLAs.
- The bounded 240-second gate is an outer CI envelope; inner socket, memory and drain assertions live in real process tests. This is not a production DoS guarantee or a complete sanitizer assessment.
- The npm lock subset is not a complete C++ OS-package SBOM or a substitute for CodeQL, dependency-advisory review and Ubuntu image verification. Do not accept any pending/skipped/failed final-head check.
- The offline release manifest is **not cryptographically authenticated by the scripts**. Owner acceptance, all stage/fix evidence, final run authentication and a separate merge request remain mandatory.

**Owner rule:** push once, let GitHub-hosted Ubuntu 22.04/24.04/26.04 CI start on the draft PR, record pending status, and stop immediately. Never wait for the result in this work unit. No Debian, self-hosted Xeon or production environment is involved.
