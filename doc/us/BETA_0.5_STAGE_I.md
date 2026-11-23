# Beta 0.5.1–0.5.4 — Stage I implementation and evidence

This is **work unit 1** of [Issue #234](https://github.com/adrianf87/dc24h.eu-high-starter/issues/234), using the single `beta-0.5.x-stage-full` branch and one draft PR. A development version is **not** a release, production rollout permission or a completed target-host acceptance test.

## Ordered development patches

| Patch | Implemented safeguard | Evidence boundary |
| --- | --- | --- |
| 0.5.1 — CI & Test Stability | Bound the test/build parallelism; retain Ubuntu 22.04/24.04/26.04, WebAdmin, ASan/UBSan, security checks and the required aggregate check; generate per-job immutable, short-lived GitHub artifacts containing only CTest status/counts, never test stdout, tokens or private API data. Add fail-closed JUnit summarizer unit tests. | Artifacts are per CI run/attempt, not signing or independent evidence. A missing JUnit file is `not-run`, not success. |
| 0.5.2 — Debian & Ubuntu | Add hosted, disposable Debian 12 (bookworm) and 13 (trixie) container C++20, MariaDB, Apache2, ADC/ADCS, Node 22/Vite/TypeScript smoke checks; require registration of integration fixtures. Root and sudo apt provisioning both work with timeouts. Debian jobs must pass for aggregate CI to pass. | These are **container** checks, not real systemd-manager tests or target hardware. The three Ubuntu hosted images remain required. |
| 0.5.3 — Real Performance Preparation | Refuse to count an incomplete/not-running private API response as success. Add an offline validator for all four probe profiles (ADC, ADCS, WebAdmin HTTPS and authorized private API), timestamped preapproved p95/p99/error/throughput limits, CPU/RSS/MariaDB observations and the exact target characteristics. | Synthetic unit fixtures are **not target measurements**. Numerical limits and raw probe reports must be approved/collected by the operator; the tool cannot attest hardware identity. |
| 0.5.4 — Matched Recovery Preparation | Add an offline SHA-256 pairing manifest for a stopped-pilot MariaDB dump and the existing five-file gateway snapshot, with strict file ownership/mode, no-follow, no-overwrite and corruption checks. | The tool does **not** dump/import MariaDB, restart services, prove transactional consistency or measure RPO/RTO. Those remain operator acceptance gates. |

## CI evidence and isolation

- Draft PR to `main` triggers `.github/workflows/build.yml`; wait for every C++ Debug/Release job (three Ubuntu OS images), WebAdmin job (three images), both sanitizers, both Debian smoke jobs, and `CI (C++ and WebAdmin)`. Do not infer success from queued, skipped, cancelled or running results. The final head SHA and run/job URLs belong in Issue #234.
- `scripts/ci_junit_summary.py` emits only test counts, aggregate status, suite and source SHA; it excludes raw XML, logs, cookies, audit events, paths, passwords and HTTP responses. GitHub upload-artifact is pinned and named per run/attempt; retain summaries for 14 days. Use the GitHub job log for failure investigation in a controlled environment and avoid sharing secrets.
- Debian smoke runs on GitHub-hosted Ubuntu with Debian containers. Even if `systemd-analyze verify` is available, a Docker container with no active systemd manager cannot pass `systemd-run` lifecycle/security checks. Exclude `systemd.sandbox` from container results; test the actual systemd unit on an authorized host or VM with an active manager, under a non-production identity, and record `systemctl`, `journalctl`, service restrictions, restart and failure outcomes.

## Actual target performance — operator action required

Target: **Debian 13 Trixie, Intel Xeon D-1521, eight logical threads, 31,997 MiB RAM**. An AMD EPYC or Debian container is not equivalent. Obtain authorization for an isolated replica with disposable accounts, local-only listeners, verified TLS and stable MariaDB/Apache versions. Before any load run, agree and timestamp numeric limits for **each** profile: max p95, max p99, max error fraction, minimum successful requests/second; additionally max CPU %, peak RSS KiB, MariaDB peak threads and errors. Keep the threshold JSON outside Git and ensure `approved_before_utc` predates `measurement_started_utc`.

Run four bounded loopback-only profiles from [the existing performance procedure](BETA_0.4_STAGE_IV_PERFORMANCE.md), with explicit `--ack-isolated` and the approved CA/session file for HTTPS/private API. Save the four JSON reports privately under `profiles` keyed `adc`, `adcs`, `webadmin`, `api`; add independently measured peak CPU/RSS/MariaDB observations under `resources`, and target facts plus exact source SHA under `target`. Run:

```sh
python3 scripts/stage1_perf_gate.py --policy /private/stage1-policy.json --evidence /private/stage1-observations.json
```

The command emits only a non-sensitive pass/fail numerical summary; `capacity_claim` and `release_approved` remain false even when thresholds pass. Keep report/approval provenance off-repository. Repeat controlled workloads and review tails, saturation, errors and database health with an independent operator before making any capacity claim.

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
| Full latest-head public GitHub Actions including Debian smoke | **PENDING** |
| Debian 12/13 and Ubuntu compatibility with authorized systemd-host behavior | **PENDING — host-manager evidence** |
| Actual target Xeon performance, limits and resource observations | **NOT MEASURED** |
| Matched MariaDB/private restore, restart, rollback and RPO/RTO | **NOT MEASURED** |
| Independent/operator acceptance | **PENDING — NO-GO** |

Keep Stage I unchecked in Issue #234 until these gates and the final-head CI pass. Do not proceed to Stage II in the same request, merge, close the issue or deploy.
