# Beta 0.5.1–0.5.4 — Stage I implementation and evidence

This is **work unit 1** of [Issue #234](https://github.com/adrianf87/dc24h.eu-high-starter/issues/234), using the single `beta-0.5.x-stage-full` branch and one draft PR. A development version is **not** a release, production rollout permission or a completed target-host acceptance test.

**Owner-directed CI scope change (2026-11-23):** Execute no Debian 12/13 tests. All new tests and measurements run only on GitHub-hosted Ubuntu 22.04/24.04/26.04. No test may target the Intel Xeon D-1521 or a privately managed runner. This direction overrides the current *execution scope*, not the meaning of the original issue's Debian/target-hardware acceptance criteria. Those remain explicitly unevidenced unless the owner separately revises the release gate.

## Ordered development patches

| Patch | Implemented safeguard | Evidence boundary |
| --- | --- | --- |
| 0.5.1 — CI & Test Stability | Bound the test/build parallelism; retain Ubuntu 22.04/24.04/26.04, WebAdmin, ASan/UBSan, security checks and the required aggregate check; generate per-job immutable, short-lived GitHub artifacts containing only CTest status/counts, never test stdout, tokens or private API data. Add fail-closed JUnit summarizer unit tests. | Artifacts are per CI run/attempt, not signing or independent evidence. A missing JUnit file is `not-run`, not success. |
| 0.5.2 — Compatibility | The original patch introduced Debian container smoke; the owner's later instruction removed that job and its fixture gate. C++20, MariaDB, Apache2, ADC/ADCS, Node 22/Vite/TypeScript are checked on Ubuntu 22.04/24.04/26.04 only, with timeout-controlled apt provisioning. | No new Debian 12/13 tests run. An active systemd manager and root access are mandatory for the Ubuntu sandbox check; absence fails instead of skipping. |
| 0.5.3 — Real Performance Preparation | Refuse to count an incomplete/not-running private API response as success. Add an offline validator for all four probe profiles (ADC, ADCS, WebAdmin HTTPS and authorized private API), timestamped preapproved p95/p99/error/throughput limits, CPU/RSS/MariaDB observations and the exact target characteristics. | Synthetic unit fixtures are **not target measurements**. Numerical limits and raw probe reports must be approved/collected by the operator; the tool cannot attest hardware identity. |
| 0.5.4 — Matched Recovery Preparation | Add an offline SHA-256 pairing manifest for a stopped-pilot MariaDB dump and the existing five-file gateway snapshot, with strict file ownership/mode, no-follow, no-overwrite and corruption checks. | The tool does **not** dump/import MariaDB, restart services, prove transactional consistency or measure RPO/RTO. Those remain operator acceptance gates. |

## CI evidence and isolation

- Draft PR to `main` triggers `.github/workflows/build.yml`; wait for every C++ Debug/Release job (three Ubuntu OS images), WebAdmin job (three images), both sanitizers, and `CI (C++ and WebAdmin)`. There are no Debian jobs or private runner jobs. Do not infer success from queued, skipped, cancelled or running results. The final head SHA and run/job URLs belong in Issue #234.
- `scripts/ci_junit_summary.py` emits only test counts, aggregate status, suite and source SHA; it excludes raw XML, logs, cookies, audit events, paths, passwords and HTTP responses. GitHub upload-artifact is pinned and named per run/attempt; retain summaries for 14 days. Use the GitHub job log for failure investigation in a controlled environment and avoid sharing secrets.
- `DC24H_REQUIRE_SYSTEMD_HOST=1` makes Ubuntu CTest fail if the real manager or root/sudo is unavailable. Its service sandbox checks exercise the disposable unit and real `systemd-run`, not a container PID-1 approximation. Review these jobs' complete logs and any failures.

## Ubuntu-hosted measurement scope (not target acceptance)

For each GitHub-hosted Ubuntu image, the isolated VM workflow collects bounded loopback ADC/ADCS protocol samples: p50/p95/p99, successful throughput, error counts, before/after RSS, and approximate process CPU time relative to one logical CPU. A disposable CLI MariaDB backup/restore fixture records elapsed time and verifies schema/credential bytes. The CTest and Chromium JUnit reports are converted into fixed-category p50/p95/p99 **test-duration** summaries. Browser test durations and SQL suite times are **not HTTP/database request latencies**. Raw JUnit output and private responses do not enter the artifact.

The existing standalone four-profile tool and offline validator remain available for an appropriately authorized future campaign, but are **not** run against Debian or the Xeon in this owner-directed unit:

```sh
python3 scripts/stage1_perf_gate.py --policy /private/stage1-policy.json --evidence /private/stage1-observations.json
```

This command requires preapproved real target evidence and therefore is **not executed by current CI**. No Ubuntu CI value substitutes for target Debian 13 Xeon throughput, recovery RPO/RTO, or independent capacity acceptance. All reports mark `capacity_claim: false`.

## Matched MariaDB + gateway recovery — operator action required

On an authorized **disposable** pilot, stop the gateway and pause hub writes; confirm both are stopped. Create a consistent private Unix-socket `mariadb-dump` with separately secured `0600` option files, verified success/row counts/transactional consistency, and record its timestamp. Keep all sensitive SQL files in owner-owned `0700` directories. Snapshot the five gateway files from the *same stopped consistency window* as in [the existing recovery procedure](BETA_0.4_STAGE_IV_RECOVERY.md).

Bind the two immutable filesets after snapshot verification (paths below are examples only; create an empty `0700` pair directory first):

```sh
python3 scripts/stage1_recovery_bundle.py pair --mariadb-dump /private/pilot.sql --private-snapshot /private/gateway-snapshot --pair-directory /private/pair --commit-sha FULL40CHARSHa --ack-stopped --ack-isolated
python3 scripts/stage1_recovery_bundle.py verify --mariadb-dump /private/pilot.sql --private-snapshot /private/gateway-snapshot --pair-directory /private/pair --ack-isolated
```

Replace `FULL40CHARSHa` with the real 40-character lowercase Git SHA. The pairing manifest stores only hashes, sizes, timestamp and SHA, not contents or private paths; it is not signed and cannot prove that the dump and snapshot were taken together. In a fresh separate MariaDB instance, have the operator import that reviewed dump, verify schema v11 and row counts/permission grants, restore five files into a **new** directory, start the disposable services, check password + TOTP/passkey, revoked sessions/grants and audit integrity, inject an outage, and time safe recovery and rollback. Never import a dump with database create/drop statements into live production; never overwrite live private state. A second operator must validate RPO/RTO and the completed checklist.

## Work-unit gate (not preapproved)

| Required gate | Status until actual evidence exists |
| --- | --- |
| Full latest-head public GitHub Actions on Ubuntu 22.04/24.04/26.04 | **PENDING new head** |
| Real systemd manager under three hosted Ubuntu VMs | **PENDING new head** |
| Debian 12/13 compatibility evidence | **DEFERRED BY OWNER — no tests run** |
| Actual target Xeon performance, limits and resource observations | **NOT MEASURED — owner forbids target test in this unit** |
| Matched MariaDB/private restore, restart, rollback and RPO/RTO | **NOT MEASURED** |
| Independent/operator acceptance | **PENDING — NO-GO** |

Keep Stage I unchecked in Issue #234 until these gates and the final-head CI pass. Do not proceed to Stage II in the same request, merge, close the issue or deploy.
