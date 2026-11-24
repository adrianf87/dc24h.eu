# Beta-0.9.20 — CI, performance, dependencies and RC Fix Pass

**Status: NO-GO.** `beta-0.9.x-stage-full-fix-pass` is an isolated development branch, not an RC release, deployment or Owner authorization.

## Implemented evidence checks

- **Exact-head GitHub CI:** `beta09_artifact_evidence.py --mode fix-pass` requires a successful *push* run on the current fix-pass branch head, independently verifies GitHub artifact ZIP SHA-256 bytes and redacted contents, and refuses skipped/pending jobs. It requires the C++/WebAdmin, isolated Ubuntu VM, CodeQL, TSAN/fuzz and Stage IV private-storage workflows. Storage-fault checks must pass on GitHub-hosted Ubuntu 22.04, 24.04 and 26.04. Future `main` acceptance needs a separate post-merge `push main` run, only after a separately authorized merge.
- **Performance:** archived envelope and offline candidate validation enforce all three windows and measured samples for ADC, ADCS and authenticated HTTPS, plus the *predeclared* p99, minimum successful RPS, maximum CPU and RSS-growth thresholds. Nonfinite/missing data fails closed. These short hosted windows do **not** qualify production capacity or SLA.
- **Dependencies:** full production **and dev** npm advisory check remains mandatory. Build-chain evidence now validates the actual manifest against lockfile v3, exact direct dependency versions and SHA-512-integrity HTTPS npm registry transitive tarballs. It detects tampering; it does not auto-upgrade packages or grant advisory waivers.
- **RC:** the candidate gate accepts only the matching PR or fix-pass independent artifact-verifier metadata, including the exact five workflow run IDs/attempts. Its result is always `NO-GO`, even if locally consistent and independently hashed.

## Manual review after final-head workflows finish

```bash
python3 scripts/beta09_artifact_evidence.py \
  --root . --sha <exact_40_character_SHA> --mode fix-pass \
  --output <private_GitHub_report.json>

python3 scripts/beta09_release_candidate_gate.py \
  --root . --reports <verified_Ubuntu_report_directories> \
  --hardening <verified_TSAN_fuzz_directories> \
  --github <private_GitHub_report.json> \
  --output <private_NO_GO_report.json>
```

Use a separate read-only GitHub token. Only consume reports from independently verified archives; never commit token, raw npm audit output, private state or crash logs. CI disk errors are injected into disposable tmpfs only.

**Unresolved Owner-gated risks:** final SHA hosted CI results, v1→v2 runtime authenticated migration and atomic six-component backup approval, archive retention/recovery policy, physical keys/native clients, production-host capacity/RPO/RTO, explicit Owner RC/merge authorization and later protected-main scan. Do not tag, publish, merge or deploy from this fix pass.
