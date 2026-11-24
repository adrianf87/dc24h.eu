# Beta 0.5.13–0.5.16 — Stage IV security, endurance and release readiness

Work unit 4 in [Issue #234](https://github.com/adrianf87/dc24h.eu-high-starter/issues/234). All code stays on `beta-0.5.x-stage-full` and in existing draft PR #235. The release candidate is **development-only**: not merged, deployed, exposed to the Internet or authorized for production. Its automated gate requires successful final-head GitHub-hosted Ubuntu 22.04/24.04/26.04 runs; fix passes 1–4 remain separate future work units.

## 0.5.13 — Bounded adversarial/security regressions

- Real C++ ADC process: deterministic malformed/fragmented frame corpus, overlong payloads, embedded NUL/invalid UTF-8, early peer teardown, concurrent clients and a clean valid connection afterward. Existing ADCS invalid TLS, WebAuthn signature/Origin/RP ID/UP/UV/replay/ownership and account-grant negatives remain mandatory.
- Real Apache loopback HTTPS: encoded path, traversal, suffix and query attacks must not leak or reach the upstream; Node gateway's exact raw-path and method allowlist receives the same boundary-fuzz corpus. React Playwright tests still assert second-step passkey revocation, revoked sessions, no hub-control writes and fail-closed outage views.
- C++ AddressSanitizer and UndefinedBehaviorSanitizer run on hosted Ubuntu 24.04; `npm audit --omit=dev --audit-level=high`, CodeQL and the complete browser security suite provide additional coverage. The fuzz input is deterministic and bounded; this is **not** coverage-guided exhaustive fuzzing or an independent penetration test.
- Previous CodeQL repair `f2c7c4c` eliminated a hand-written SHA-1 credential hash in an isolated MariaDB test and created its temporary password file with owner-only `0600` permissions from the start. The final stage must be rechecked for new scan findings.

## 0.5.14 — Hosted Ubuntu endurance and Stage I comparison

The pinned, non-sensitive [Stage I loopback reference](../../tests/fixtures/stage1_ubuntu_loopback_reference.json) was recorded on commit `96c3f873c2c94ea5e252b7db7e9b007b470330c2`, [VM run #45283779224](https://github.com/adrianf87/dc24h.eu-high-starter/actions/runs/45283779224). Each Ubuntu image used 12 real ADC and 12 verified ADCS connections at 4 requests/s, concurrency 2. The Stage IV CI test runs **three windows of 32 connections per protocol** on the same image, with 8 requests/s, concurrency 4, isolated loopback TLS, verified certificate, actual hub PID and process RSS/CPU observations. Together with the two initial 12-request smoke probes this stays below the existing 256 admissions/minute per-IP defense (216 total); the test **does not relax** the gateway quota. It records p50/p95/p99, successful requests/s, failures and RSS changes in `stage4-endurance.json`, with conservative numeric CI bounds (p99 ≤ max(200 ms ADC / 500 ms ADCS, 10× Stage I p99), throughput ≥ 40% of Stage I reference, RSS growth ≤ 32 MiB, zero errors). These are regression guardrails, **not** equal-workload capacity guarantees. Different request counts/rates and cloud-image scheduling prevent a statistically controlled apples-to-apples comparison.

The real local Apache TLS proxy and WebAdmin static HTML get a separate, bounded 48-request-per-route concurrency test (`stage4-https-endurance.json`); the private API upstream in that test is synthetic. Stage I reported browser/CTest **test durations**, not per-request HTTP throughput, tail latency or RSS. Their reference durations are recorded with `stage1_request_level_baseline_available=false` and deliberately **not divided into** the new HTTP results. A disposable MariaDB Unix-socket SQL read test measures 40 concurrent client invocations, including CLI startup time, and records p99/rps/errors in the rollback artifact. Neither represents authenticated production WebAdmin/API behavior or an exact Stage I MariaDB SQL baseline.

All measurements must stay in GitHub Actions artifacts produced on GitHub-hosted Ubuntu 22.04/24.04/26.04. Do not run the endurance loops on a production hub or publish cookies, private SQL, credentials, audit entries or raw request contents.

## 0.5.15 — Disposable failure, restore and rollback

The new `beta.05.stage4.rollback` test uses its own private temporary MariaDB datadirs/sockets, freshly generated test-only credentials, synthetic v11 account/grant state and all five owner-only gateway stores. It creates a stopped matched SQL/private manifest, verifies hashes, then injects a database/socket outage plus damage to **isolated copies** of the built package artifact and configuration. The C++ DB check and config check must refuse the failure. The test restores exact package/config bytes, imports the SQL into a **fresh** MariaDB instance, restores the five private files to a new directory, rechecks grants, revoked passkeys, audit/replay integrity and C++ schema status, then restarts the recovered database and revalidates. It records synthetic zero acknowledged-write loss (nothing committed after the checkpoint), measured RTO and CLI query rates. Existing matched Stage I recovery additionally performs a separate fresh rollback following a deliberately changed account role.

This rehearsal does not operate the OS package manager, change a real installed service, prove a previous-version downgrade, authenticate a real browser after restoration or establish production RPO/RTO. No snapshot is restored on a running database; failure artifacts stay redacted.

## 0.5.16 — Release candidate and security re-review

Security disposition and residual risks:

| ID | Finding or exposure | Disposition / evidence |
| --- | --- | --- |
| S4-01 | Test used direct SHA-1 and a delayed `chmod` for a disposable MariaDB credential | Fixed in `f2c7c4c`; latest-head CodeQL Python and integration revalidation required. |
| S4-02 | Parser/proxy alias smuggling and malformed handshake cleanup | Added fail-closed C++/Node/Apache corpora; existing WebAuthn and session negative tests remain enforced. |
| S4-03 | Cloud CI endurance baseline is short, workload mismatched and HTTP lacks Stage I request-level values | Explicitly distinguished and retained as residual measurement uncertainty; no capacity SLA, stress certification or real API p99 comparison. |
| S4-04 | Recovery is same-version/synthetic with disposable database and private state | Verified by CI; historical rollback, package manager rollback and real user session reauthentication remain untested. |
| S4-05 | One gateway, file-backed passkeys, MariaDB v11, local-only Apache and optional gateway | Intentional scoped architecture; no multi-writer, public ingress, independent security attestation or production readiness claim. |

Release-candidate evidence should include exact source SHA, all Ubuntu C++ Debug/Release, WebAdmin, ASan/UBSan, real systemd, CTest, browser/Node, CodeQL, repository security audit, endurance and matched recovery/rollback results, with artifact links and failed-test disposition. **A failing/pending CI job means Stage IV is not complete**; do not check Work unit 4 until every required final-head job passes. Work units 5–8 are mandatory later fix passes even when Stage IV is green. PR #235 stays a draft, `main` and Issue #234 remain open, and no external approval is an extra gate in this issue.

## Safe offline operator rehearsal (synthetic disposable inputs only)

1. Stop the **isolated** gateway and MariaDB writers and confirm the intended source commit. Never take a partial live-state snapshot. Keep SQL dump and all five private-state files protected and together.
2. Use `scripts/stage4_private_recovery_drill.py snapshot --source PATH --destination EMPTY_PATH --ack-isolated --ack-stopped` to capture a stopped private-state copy. Use `verify --source SNAPSHOT_PATH --ack-isolated` before any import.
3. Pair the dump and snapshot with `scripts/stage1_recovery_bundle.py pair --mariadb-dump DUMP --private-snapshot SNAPSHOT_PATH --pair-directory EMPTY_PRIVATE_DIR --commit-sha EXACT_SHA --ack-isolated --ack-stopped`. Run `verify` with the **same** paths plus `--commit-sha EXACT_SHA --ack-isolated` before restoring; refuse a missing/modified element.
4. Restore into **fresh** isolated MariaDB and gateway directories. Revalidate v11 migrations, app credentials/grants, passkey revocation/epoch, TOTP replay, audit hash chain and limiter state before reconnecting clients. The paired manifest provides a consistency check, not a signature or atomic transaction guarantee.
5. For a rollback, stop writers again, use a distinct fresh instance with the known-good package/config plus the **original matching SQL/private snapshot**, verify hashes, measure RPO/RTO against the chosen checkpoint and confirm the service/grants. Do not overwrite existing active state or assume a real session is still valid.

For detailed Stage I/II/III context see [Stage I](BETA_0.5_STAGE_I.md), [Stage II](BETA_0.5_STAGE_II.md), [Stage III](BETA_0.5_STAGE_III.md) and [PL recovery runbook](../pl/BETA_0.5_STAGE_IV.md).
