# beta-0.6.1–beta-0.6.16 — Stage I implementation, evidence and security risks

Development branch: `beta-0.6.x-stage-full` · [Issue #236](https://github.com/adrianf87/dc24h.eu-high-starter/issues/236) · [draft PR #237](https://github.com/adrianf87/dc24h.eu-high-starter/pull/237). **Not a production release or an approval to merge.** Stage I must remain unchecked until the latest PR head passes every required hosted Ubuntu job; Stage II is a single *later* full-series review/fix pass.

## Fixed trust boundaries

- C++20 is the sole account/password/grant authority; remote WebAdmin is an authenticated **read-only** projection over local Node and Apache HTTPS. It requires password + TOTP or password + enrolled passkey, never passwordless or a raw browser API token.
- Node listens only on loopback; the private Apache edge is loopback-only and TLS. No new public listener, arbitrary proxy, hub start/stop, SQL migration or schema change. The synthetic MariaDB v11 state and exactly five private gateway stores remain tied together for stopped-writer recovery.
- Browser incident acknowledgment is local React state and sends no API request. Read age is a monotonic **browser-receipt age**, not server audit freshness; recovery/maintenance status is intentionally unknown where the approved projection has no field. Old metrics disappear on 401/403/outage and after their age budget.
- Rotate the local API token only with all C++/Node writers stopped, on private owner-only files, with no active 60-second limiter bucket. Never print token material. A lost key or private-store failure is an offline refusal, not an invitation to bootstrap or reset live limits.

## Patch and evidence inventory

| Range | What changed | Acceptance source | Known evidence limit |
| --- | --- | --- | --- |
| 0.6.1–0.6.4 | Provenance, real authenticated HTTPS, diagnostic IDs and numeric workload | `scripts/beta06_ci_provenance.py`, `tests/beta06_authenticated_api_process_tests.py`, `scripts/beta06_auth_gate.py`, Ubuntu CI artifacts | 24 TOTP-authenticated synthetic requests, not an SLA or browser passkey measurement |
| 0.6.5–0.6.8 | Capture cleanup, fresh-state authentication, older binary rollback and offline token cutover | `tests/stage4_private_recovery_tests.py`, `tests/beta06_previous_version_rollback.py`, `tests/beta06_gateway_token_rotation_tests.py` | No online MariaDB/file atomicity, actual browser WebAuthn restore or distro-package downgrade |
| 0.6.9–0.6.12 | Wire corpus, slow-peer fairness, cert cutover and bounded shutdown | `tests/beta06_adc_compatibility_corpus.json`, `tests/adc_login_process_tests.py`, `tests/tls_security_process_tests.py`, `tests/adc_close_process_tests.py` | Loopback deterministic clients only; no third-party DC++/AirDC++ certification |
| 0.6.13–0.6.16 | Read-only incident UI, local keyboard acknowledgment, fault/endurance gate and version/docs | `webadmin/tests/remote-gateway.spec.ts`, `scripts/beta06_integrated_chaos_gate.py`, GitHub-hosted workflow | Synthetic independent fault windows, not production RPO/RTO or simultaneous fleet chaos |

## Numerical conditions and reporting

- The private HTTPS fixture requires exactly 24 successful password+TOTP authenticated read-only requests, zero errors and nonzero measured RPS with observed p50/p95/p99 and bounded p99 (see the executable `beta06_auth_gate.py` ceiling), plus **account-maintenance C++**/Node/Apache process CPU and RSS. Requests use a fixed **1,100 ms start interval**: every summary rechecks a C++ grant twice and the unchanged C++ authority allows 24 checks per 10-second window. The resulting RPS includes pacing and is **not peak throughput**. The serving `dc24h` hub exposes a separate read-only API without `/api/v1/admin/access`; the fixture first proves the hub is running and serves status, then **stops it** before starting `dc24h-account-admin` as the real password/grant authority. Authenticated readings intentionally report `server.running=false` during maintenance. Mixing the serving hub's socket with administrator POSTs would falsely turn every real TOTP login into an HTTP 403. Node errors and refused requests are **not** successful throughput. This measures an isolated maintenance-mode API workload, **not** concurrent live-hub capacity. The Ubuntu image, source SHA and fixture identity must match the final head.
- The existing real ADC and verified ADCS smoke workloads are **12 requests each**, followed by separate three-window 32-request/profile endurance against the pinned historical hosted Ubuntu baseline. A separate six-case fault suite checks ADC negotiation, slow-client fairness, real TLS cutover, three restarts, private recovery corruption and actual systemd. The integrated gate requires all upstream reports, exact source SHA and complete non-skipped results.
- Latest-head final gate: GitHub-hosted Ubuntu **22.04, 24.04 and 26.04** C++ Debug/Release, WebAdmin/Playwright and npm dependency audit, ASan/UBSan, CodeQL, actual systemd, ADC/ADCS, private Apache HTTPS, MariaDB v11/five-store recovery, real restored authentication and 0.5.16 previous-binary rollback. A pending, skipped, stale, failed, cancelled or timed-out job is a blocker, not a pass.
- Record exact **GitHub PR head SHA**, OS release, workflow run links, build/dependency identity, result counts, p50/p95/p99, RPS, errors, CPU/RSS and limitations in the PR/issue after those runs complete. All reports must be synthetic and redacted. Do not copy private state or credential values to artifacts.

## Security disposition / residual-risk register

| Risk / disposition | Mandatory operator constraint | Evidence gap that cannot be called complete |
| --- | --- | --- |
| Browser passkey post-restore | Enroll and verify password + passkey against the approved origin; never reset revoked catalogs to an empty file | The automated restore currently verifies revoked passkey options but **not** a full real browser WebAuthn ceremony |
| Historical package rollback | Stop writers; restore SQL and all five private stores into a fresh isolated directory; validate SHA and prior source before any login | This rehearses an actual separately built 0.5.16 binary, not OS package-manager downgrade or target host recovery |
| Token/certificate rotation | Rotate offline after limiter expiry, preserve old token in private backup, restart loopback services and reauthenticate; rollback offline if failure | No online atomic cross-service key rollover; live TLS files require restart and revalidation |
| Stale operator information | Never use cached counters after 401/403/outage or freshness expiry; local acknowledgment does not resolve an incident | Approved gateway has no trustworthy maintenance/recovery timeline; UI marks it unknown |
| Slow clients / resource use | Keep per-peer and global queue and timeout controls; enforce existing loopback admission; investigate failed CPU/RSS regression | Hosted VM budgets are neither production capacity nor a target Xeon/Debian benchmark |
| Data loss / SQL-file consistency | Snapshot only while all writers are stopped; reject symlinks, mismatched manifests, partial restore or known-bad audit/replay state | MariaDB and five independent files have no online shared transaction; RPO/RTO figures are synthetic |
| ADC interoperability | Exercise pinned wire corpus, strict TLS and reconnect; retain protocol grammar and CID/PID checks | No external DC++/AirDC++ client matrix validated by this CI fixture |

## Rollback and operator handoff

Use the [Polish operator/recovery runbook](../pl/BETA_0.6_STAGE_I.md) for isolated preflight, offline capture, token rollback, certificate restart and stop/go checks. The `beta06_previous_version_rollback.py` fixture is intentionally source-pinned to immutable beta-0.5.16; update neither its old source SHA nor its expected old runtime version when updating current release metadata.

**Go/no-go:** NO-GO for production. The draft PR stays draft until Stage I *and* the later consolidated Stage II pass their own exact-final-head CI. A normal protected `main` merge and issue close can follow only after both work units and final checks; no deployment follows automatically.
