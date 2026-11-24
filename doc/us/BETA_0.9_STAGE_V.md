# Beta 0.9.17–0.9.20 — Stage V: independently inspected release evidence

**Status: NO-GO. Source delivery is not CI acceptance, release approval, protected-main merge authorization or deployment.** This is work unit 5 under [Issue #242](https://github.com/adrianf87/dc24h.eu-high-starter/issues/242) on the sole `beta-0.9.x-stage-full` branch, shared [draft PR #243](https://github.com/adrianf87/dc24h.eu-high-starter/pull/243). This stage adds a candidate and reproducible *CI* proof machinery, not a fully production-ready installation. Fix Passes 1–5 and a later explicit Owner decision are separate work units.

The subsequent isolated `beta-0.9.x-stage-full-fix-pass` changes, including exact-head CI, performance, dependency and RC hardening, are documented in [CI/RC Fix Pass](BETA_0.9_CI_RC_FIX_PASS.md).

## 0.9.17 — independently checked GitHub artifact bytes

After all workflows finish, `scripts/beta09_artifact_evidence.py` checks the live PR #243 head or, **only after a separately approved merge**, the resulting protected `main` head. It requires the exact SHA, latest successful attempts, complete non-skipped GitHub-hosted job matrix, matching Ubuntu 22.04/24.04/26.04 labels, distinct artifact IDs, job time windows and the correct branch/event.

For every expected redacted artifact it follows the GitHub REST artifact ZIP redirect *without forwarding the bearer token* to the signed object-storage URL. It caps download bytes, compares the **SHA-256 of the downloaded ZIP bytes** with the independent GitHub artifact digest, and reads the ZIP in memory without extracting files. A fixed JSON filename inventory, single-entry and total decompression ceilings, filename/traversal/symlink/duplicate/encryption/ratio checks, UTF-8/JSON schema and source-head checks reject wrong-attempt, ZIP-slip, bombs, missing reports and deliberately unreviewed secret fields. Select legacy numeric report names lack embedded SHA; the tool counts them as `legacy_reports_without_embedded_sha`, linked only by the authenticated enclosing job. It never claims those legacy files are individually signed.

The API, its token holder, CI job environment and GitHub's digest are still trust roots. Passing this tool **does not** authenticate private production state, a physical device or a human Owner's release decision. A report's internally consistent values are not independent measurement of physical reality. If archive bytes differ from GitHub digest, verification blocks, even if workflow metadata reads `success`. The redacted verifier result contains counts/IDs/attempts, not raw report contents.

Post-merge operation is a **future manual check only**: the C++/WebAdmin, isolated Ubuntu VM, CodeQL and TSAN/fuzz workflows now define a `push: main` trigger. The final merged commit must finish those jobs independently before any protected-main result can be claimed; no merger or rerun is performed here.

## 0.9.18 — public Ubuntu APT/build/npm supply chain

`scripts/beta09_os_provenance.py` captures only public installed package names and versions from a GitHub-hosted Ubuntu job, canonicalizes the entire bounded `dpkg-query` inventory and records its SHA-256. It lists the eight critical C++/TLS/ncdc APT package versions, pinned GitHub Action reference digest/count and npm-lock SHA-256, plus exact GCC, CMake, Node and npm versions. The existing production **plus dev/build-chain** npm audit, high/critical advisory refusal and GitHub-pinned action inventory remain mandatory, separately required checks. Candidate release gates compare the matching source SHA, npm lock, compiler/Node and Ubuntu APT inventory digest across the relevant same-job measurements.

**Residual:** a digest of the observed disposable Ubuntu image is **not an immutable APT mirror snapshot, archived `.deb` hash inventory, package signature attestation, reproducible compiler binary, or complete OS SBOM**. Public Ubuntu security updates and GitHub-hosted image rollovers may legitimately change package versions. The gate rejects missing/ambiguous values and mismatches; it never mislabels a drifting host as fully reproducible.

## 0.9.19 — expanded bounded, separately interpreted security evidence

- The GitHub-hosted Ubuntu 24.04 ThreadSanitizer target list broadens from 3 to **8** C++ parser/network/ADC transfer/RCM/CTM/token/protocol unit suites. A TSAN runtime incompatibility or suppressed/missing check remains a failed gate.
- Clang/ASan libFuzzer runs **two independent targets**: existing ADC frame/stream parser plus new finely fragmented input decoding. Each uses a fixed seed, at least 1024 cases, max 4096 bytes/input, a 512 MiB fuzzer RSS policy, 5-second per-input timeout, 180-second wall cap, 1 MiB diagnostic cap and disposable seed/crash directories. The combined fail-closed report requires **2048 total runs** with neither raw inputs nor crash traces uploaded.
- The C++ Ubuntu 24.04 ASan/UBSan subset now covers parser, syntax, peer routing, tokens, negotiation and network in addition to previous memory/security/API checks. Existing CodeQL and all hosted C++ Debug/Release + WebAdmin/browser tests remain mandatory.
- Every hosted Ubuntu 22.04/24.04/26.04 isolated VM recomputes three fixed numeric-only windows each for ADC, ADCS and password+TOTP private HTTPS. Per-workload p50/p95/p99, successful RPS, CPU/RSS and the five bounded slow-reader/TLS/drain/restart cases are tied to exact compiler/Node/lock/APT identity. No fabricated percentile/nonfinite value, failed request, missing fault or budget exceedance may pass. **Different protocols are not comparable workloads; there is no Debian/Xeon production SLA, capacity or exhaustive DoS certification.**

## 0.9.20 — aligned candidate and NO-GO Owner gate

The C++ binary/API/controller, React/Node UI, npm package+lock, current active tests, README/changelog and configuration examples use **`beta-0.9.20`**. Historical `beta08_release_candidate_gate.py`/legacy test-fixture baselines intentionally preserve their own old-version expectations and cannot authorize the 0.9 release. The new `scripts/beta09_release_candidate_gate.py` validates candidate metadata, 4 categories of redacted report per Ubuntu image and the two Ubuntu 24.04 TSAN/fuzz records. Even with separately verified GitHub ZIP bytes it returns `status: NO-GO`, `merge_authorized: false` and `production_deployment_authorized: false`; an explicit Owner decision is separate.

### Residual risk register requiring Owner decision

| Area | Explicit limit or decision not covered by Stage V |
| --- | --- |
| ADC account/role authority | C++ remains the sole grant authority; controlled restart is required for account snapshot revocation. Live generation reload remains a separate Owner architectural decision. |
| Private state/backup | MariaDB schema v11 plus five owner-only Node stores; v1→v2 authenticated cold-start migration/key/checkpoint design requires separate approval. Offline HMAC experiments do not replace a live atomic six-component snapshot. |
| Native clients and devices | DC++/AirDC++/physical or synchronized WebAuthn keys remain unexecuted unless a real supported host/device is explicitly approved. |
| Historical binary/systemd | Disposable downgrade, restored state and compiled runtime are not complete browser password+TOTP/passkey reauthentication against an actually installed historical C++ binary and systemd service. |
| Audit/data loss | 8 MiB hash-chain retention, stopped-writer archival and ENOSPC fail-closed authorization need Owner retention/checkpoint policy; do not truncate or reset stores. |
| Security/toolchain | CI TSAN/fuzz/ASan/UBSan/CodeQL and supply-chain checks can find defects but not prove their absence. Runner updates and the APT mirror remain mutable. |
| Production performance | Three short Ubuntu windows and resource-drain fixtures are not production Debian 13/Xeon throughput, SLA, RPO/RTO or blanket availability guarantees. |
| Release/governance | All 5 later Fix Passes, exact-final-head GitHub-hosted evidence, explicit Owner approval and a separately authorized post-merge main scan are mandatory. No public listener, hub-control write, migration, tag, publish or deploy. |

## Review / rollback handoff

1. Commit/push on the single branch and trigger GitHub-hosted Ubuntu CI, then **stop immediately without polling/waiting**. Record the exact SHA in PR #243. `pending` and required-skipped cases are NO-GO.
2. Later, after workflows finish, with a **read-only** GitHub token and a temporary output directory, run `python3 scripts/beta09_artifact_evidence.py --root . --sha <full-branch-sha> --mode pr --output <private-report>`. Do not commit the report or token. A later post-merge main scan uses `--mode main` only after explicit Owner merge permission.
3. Collect **sanitized** reports from the verified ZIPs by Ubuntu image; run `python3 scripts/beta09_release_candidate_gate.py --root . --reports <isolated-ubuntu-reports> --hardening <isolated-hardening-reports> --github <private-report> --output <private-NO-GO-report>`. The offline gate is never a merge command.
4. For any future Owner-approved rollback, quiesce **all** writers. Preserve an exact matching SQL v11 snapshot and the five owner-only files (`mfa.json`, `replay.jsonl`, `audit.jsonl`, `passkeys.json`, `rate-limits.json`) with pinned manifest/key/ledger where approved. Validate into a fresh empty isolated directory, never overwrite original state, verify grants, factor replay, audit continuity and version compatibility before restart. Existing stopped-writer procedures do not prove a live atomic recovery or RPO/RTO.
5. Finish the five separately authorized Fix Passes, independently inspect final-head hosted CI and unresolved risks. Only a **new, explicit Owner request** can authorize making the PR ready, merging protected main or publishing a release. **No deployment is authorized by this stage.**
