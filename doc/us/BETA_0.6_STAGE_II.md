# beta-0.6.1–beta-0.6.16 — Stage II consolidated review and fix pass

Single development branch: `beta-0.6.x-stage-full` · [Issue #236](https://github.com/adrianf87/dc24h.eu-high-starter/issues/236) · [draft PR #237](https://github.com/adrianf87/dc24h.eu-high-starter/pull/237). Stage I passed at `ea3cd114`. **Stage II remains pending new exact-head hosted CI.** Neither review constitutes release approval, a production deployment or a request to merge.

## Full-series disposition

| Patch range | Reviewed boundary | Finding and disposition |
| --- | --- | --- |
| 0.6.1–0.6.4 | Source/OS provenance, hosted CTest integrity, real password+TOTP HTTPS measurement, diagnostics, numeric p99/RPS/CPU/RSS gates | No additional defect reproduced in the validation rules and negative gate tests. Retain the fixed 24-request, two-concurrent maintenance-only workload and prohibit production-capacity claims. Recheck the real process reports on all three hosted Ubuntu images. |
| 0.6.5–0.6.8 | Stopped-writer MariaDB v11 plus five-file snapshot, post-restore authentication, previous-version executable, owner-only token rotation | **Fixed:** cleanup of a partially written backup/restore that lost its private `0700` directory permissions could raise `ValueError`, hiding the actual copy `OSError`. The uncertain destination is now preserved, and the original failure propagates. Separate negative tests cover interrupted snapshot and interrupted restore. All prior non-overwrite and integrity tests remain. |
| 0.6.9–0.6.12 | ADC/ADCS valid and negative wire corpus, backpressure/slow peer, TLS policy/certificate cutover, bounded close/reconnect | No additional defect reproduced in the loopback process-test paths. The Python TLS client floor stays TLS 1.2; the tests still reject legacy ciphers, wrong trust, bad hostname and mismatched key. Hosted C++/sanitizer/TLS-process checks must rerun; this is not third-party client certification. |
| 0.6.13–0.6.16 | Read-only incident display, local-only acknowledgment, refresh/expiry races, synthetic chaos gate, release metadata | **Fixed:** the incident/metric receipt-age validity is 45 s, but React evaluated it every 30 s, allowing a hung refresh to display stale counters up to ~30 s beyond the limit. The monotonic freshness check now updates every second. A controlled-clock Playwright test freezes subsequent summary responses, advances 46 s, and verifies the incident and counters disappear. No privileged mutation was added. |

## Verification and gates

- Locally exercise private recovery snapshot/restore failures, CI provenance/authenticated-gate/token-rotation/chaos/endurance guard tests, React status/audit parsing, TypeScript type checking and the production WebAdmin build. Browser/network-process execution inside restricted local workspaces is **not** proof of passing; use hosted CI instead.
- Required **same-PR-head** GitHub Actions results: Ubuntu 22.04/24.04/26.04 C++ Debug+Release and WebAdmin/Playwright; ASan+UBSan; CodeQL and production npm advisory audit; isolated VM systemd, TLS/ADC/ADCS/fault tests, private C++→Node→Apache password+TOTP HTTPS, MariaDB v11 and five private stores, fresh login and genuine pinned older-binary rollback. No failed, skipped, stale, incomplete or cancelled required checks.
- Inspect each run's exact source SHA and attach redacted numeric evidence in the issue/PR: authenticated 24/24 requests and p50/p95/p99/RPS/CPU/RSS, ADC/ADCS 12/12 smoke per profile and bounded three-window endurance, six independent fault cases and nonproduction rollback/restore outcomes.
- Stage II checkbox may be checked only after every required hosted result is green at the **new** final head. Keep the PR draft, do not merge to `main`, close the issue or deploy until the separately requested final gate.

## Residual risks

The private gateway is loopback-only and exposes no hub-control writes. Password+TOTP or password+passkey remains mandatory; C++ alone owns account grants. Tests do not establish browser WebAuthn post-restore ceremony, external DC++/AirDC++ interoperability, online cross-service transaction atomicity, production throughput/RPO/RTO, target Debian/Xeon performance or real operator cutover. A preserved uncertain partial backup/restore is **not** a usable recovery bundle: it needs isolated investigation and must fail verification. Refer to the [Stage I trust boundaries](BETA_0.6_STAGE_I.md) and [Polish operator runbook](../pl/BETA_0.6_STAGE_I.md).
