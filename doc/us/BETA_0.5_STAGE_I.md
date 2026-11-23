# Beta 0.5.1–0.5.4 — Stage I implementation and evidence

This is **work unit 1** of [Issue #234](https://github.com/adrianf87/dc24h.eu-high-starter/issues/234), using the single `beta-0.5.x-stage-full` branch and one draft PR. A development version is **not** a release, production rollout permission or a completed target-host acceptance test.

**Current Issue #234 CI scope (2026-11-23):** Execute no Debian 12/13 tests. All tests and measurements run only on GitHub-hosted Ubuntu 22.04/24.04/26.04. Do not target the Intel Xeon D-1521 or a privately managed runner. Target hardware and external acceptance are not gates for this issue.

## Ordered development patches

| Patch | Implemented safeguard | Evidence boundary |
| --- | --- | --- |
| 0.5.1 — CI & Test Stability | Bound the test/build parallelism; retain Ubuntu 22.04/24.04/26.04, WebAdmin, ASan/UBSan, security checks and the required aggregate check; generate per-job immutable, short-lived GitHub artifacts containing only CTest status/counts, never test stdout, tokens or private API data. Add fail-closed JUnit summarizer unit tests. | Artifacts are per CI run/attempt, not signing or independent evidence. A missing JUnit file is `not-run`, not success. |
| 0.5.2 — Compatibility | The original patch introduced Debian container smoke; the owner's later instruction removed that job and its fixture gate. C++20, MariaDB, Apache2, ADC/ADCS, Node 22/Vite/TypeScript are checked on Ubuntu 22.04/24.04/26.04 only, with timeout-controlled apt provisioning. | No new Debian 12/13 tests run. An active systemd manager and root access are mandatory for the Ubuntu sandbox check; absence fails instead of skipping. |
| 0.5.3 — Real Performance Preparation | Refuse to count an incomplete/not-running private API response as success. Add an offline validator for all four probe profiles (ADC, ADCS, WebAdmin HTTPS and authorized private API), timestamped preapproved p95/p99/error/throughput limits, CPU/RSS/MariaDB observations and the exact target characteristics. | Synthetic unit fixtures are **not target measurements**. Numerical limits and raw probe reports must be approved/collected by the operator; the tool cannot attest hardware identity. |
| 0.5.4 — Matched Recovery | Add an offline SHA-256 pairing manifest for a stopped-pilot MariaDB dump and the existing five-file gateway snapshot, with strict file ownership/mode, no-follow, no-overwrite and corruption checks. Add `beta.05.stage1.matched.recovery`: disposable MariaDB v11 SQL export/import, matched five-file restore, source outage, integrity/grant/revocation checks and new-instance rollback with bounded RPO/RTO evidence. | The offline tool alone does not run services. Hosted CI exercises a **synthetic** consistency window and DB process restarts; a live Node gateway login and production recovery are not claimed. |

## CI evidence and isolation

- Draft PR to `main` triggers `.github/workflows/build.yml`; wait for every C++ Debug/Release job (three Ubuntu OS images), WebAdmin job (three images), both sanitizers, and `CI (C++ and WebAdmin)`. There are no Debian jobs or private runner jobs. Do not infer success from queued, skipped, cancelled or running results. The final head SHA and run/job URLs belong in Issue #234.
- `scripts/ci_junit_summary.py` emits only test counts, aggregate status, suite and source SHA; it excludes raw XML, logs, cookies, audit events, paths, passwords and HTTP responses. GitHub upload-artifact is pinned and named per run/attempt; retain summaries for 14 days. Use the GitHub job log for failure investigation in a controlled environment and avoid sharing secrets.
- `DC24H_REQUIRE_SYSTEMD_HOST=1` makes Ubuntu CTest fail if the real manager or root/sudo is unavailable. Its service sandbox checks exercise the disposable unit and real `systemd-run`, not a container PID-1 approximation. Review these jobs' complete logs and any failures.
- The C++ Debug/Release test suite also runs `beta.05.stage1.matched.recovery` on each hosted Ubuntu image. Its unique `stage1-matched-recovery.json` artifact records source SHA, Ubuntu version, schema v11, five paired files, synthetic zero-loss RPO, outage-to-verification RTO, rollback duration and limitations. SQL dumps and private state are kept in OS temporary directories and never uploaded. Missing `mariadb-dump` fails the required integration configuration.

## Ubuntu-hosted measurement scope (not target acceptance)

For each GitHub-hosted Ubuntu image, the isolated VM workflow collects bounded loopback ADC/ADCS protocol samples: p50/p95/p99, successful throughput, error counts, before/after RSS, and approximate process CPU time relative to one logical CPU. A disposable CLI MariaDB backup/restore fixture records elapsed time and verifies schema/credential bytes. The CTest and Chromium JUnit reports are converted into fixed-category p50/p95/p99 **test-duration** summaries. Browser test durations and SQL suite times are **not HTTP/database request latencies**. Raw JUnit output and private responses do not enter the artifact.

The existing standalone four-profile tool and offline validator remain available for a future authorized campaign, but are **not** run against Debian or the Xeon in this unit:

```sh
python3 scripts/stage1_perf_gate.py --policy /private/stage1-policy.json --evidence /private/stage1-observations.json
```

This command requires separately prepared evidence and therefore is **not executed by current CI**. No Ubuntu CI value establishes target Debian 13 Xeon throughput or production RPO/RTO. All reports mark `capacity_claim: false`.

## Isolated CI recovery boundary

The CI-only test creates three separate, Unix-socket-only MariaDB datadirs under an owner-private temporary directory. It migrates the first database to schema v11 using the real C++ CLI, commits synthetic role/credential rows, creates five schema-valid test gateway stores, snapshots the stores, writes an `mariadb-dump` and checks the pair manifest. It then stops the source DB, imports the SQL into a **new** database, restores the five files into a **new** state directory, revalidates schema, credential bytes, account grants, revoked passkey epoch, replay and audit integrity, injects a role change and second outage, and restores the original paired state into a **third** database as rollback. No network listener, host database, production data or service is used.

The synthetic checkpoint has no acknowledged writes after the backup window: the observed fixture data loss is zero and the recorded RPO is `0 s` **only under that test assumption**. RTO measures wall time from intentional source stop until restored-state validation; rollback duration is measured separately. The predeclared CI smoke bound is 120 seconds for each operation; this is **not** a production SLA or capacity threshold. The test does not boot the Node gateway or verify a real WebAuthn/TOTP login, so `gateway_runtime_login_retested` remains `false` in the artifact. Failed jobs must be inspected and repaired before closing work unit 1.

## Matched MariaDB + gateway recovery — operator action required

For any separate operator drill on an authorized **disposable** pilot, stop the gateway and pause hub writes; confirm both are stopped. Create a consistent private Unix-socket `mariadb-dump` with separately secured `0600` option files, verified success/row counts/transactional consistency, and record its timestamp. Keep all sensitive SQL files in owner-owned `0700` directories. Snapshot the five gateway files from the *same stopped consistency window* as in [the existing recovery procedure](BETA_0.4_STAGE_IV_RECOVERY.md).

Bind the two immutable filesets after snapshot verification (paths below are examples only; create an empty `0700` pair directory first):

```sh
python3 scripts/stage1_recovery_bundle.py pair --mariadb-dump /private/pilot.sql --private-snapshot /private/gateway-snapshot --pair-directory /private/pair --commit-sha FULL40CHARSHa --ack-stopped --ack-isolated
python3 scripts/stage1_recovery_bundle.py verify --mariadb-dump /private/pilot.sql --private-snapshot /private/gateway-snapshot --pair-directory /private/pair --ack-isolated
```

Replace `FULL40CHARSHa` with the real 40-character lowercase Git SHA. The pairing manifest stores only hashes, sizes, timestamp and SHA, not contents or private paths; it is not signed and cannot prove that the dump and snapshot were taken together. In a fresh separate MariaDB instance, have the operator import that reviewed dump, verify schema v11 and row counts/permission grants, restore five files into a **new** directory, start the disposable services, check password + TOTP/passkey, revoked sessions/grants and audit integrity, inject an outage, and time safe recovery and rollback. Never import a dump with database create/drop statements into live production; never overwrite live private state. A second operator must validate RPO/RTO and the completed checklist.

## Work-unit gate (not yet completed on the new commit)

| Required gate | Status until actual evidence exists |
| --- | --- |
| Full latest-head public GitHub Actions on Ubuntu 22.04/24.04/26.04 | **PENDING new head** |
| Real systemd manager under three hosted Ubuntu VMs | **PENDING new head** |
| Ubuntu ADC/ADCS, API/WebAdmin performance aggregates with limitations | **PENDING new head** |
| Disposable matched MariaDB/private restore, DB restart, rollback and synthetic RPO/RTO | **PENDING new head** |
| Debian 12/13, Xeon and external assessment | **NOT REQUIRED by Issue #234; not tested** |

Keep Stage I unchecked in Issue #234 until all mandatory latest-head CI jobs pass. Do not proceed to Stage II in the same request, merge, close the issue or deploy.
