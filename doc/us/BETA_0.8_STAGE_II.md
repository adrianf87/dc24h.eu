# Beta-0.8.5–0.8.8 — Stage II authentication and credential-device lifecycle

Scope: work unit **2 of 10** in [Issue #240](https://github.com/adrianf87/dc24h.eu-high-starter/issues/240), on the existing `beta-0.8.x-stage-full` branch and single draft [PR #241](https://github.com/adrianf87/dc24h.eu-high-starter/pull/241). Stage I is preserved. Stages III–V and five subsequent fix passes need separate Owner authorization. **Implementation is not acceptance, merge approval or permission to deploy.**

## Patch ledger and negative regressions

| Patch | Changes and coverage | Remaining qualification |
| --- | --- | --- |
| **0.8.5 — Interrupted browser passkey** | `beta07-webauthn-restore.spec.ts` now obtains a real Chromium assertion before stopping the HTTPS writer, clears the in-memory challenge to represent a cold gateway, restores all five private files and refuses the old assertion. It checks a truncated catalog is rejected and requires a new signed ceremony. The existing hosted `beta06_authenticated_api_process_tests.py` separately reauthenticates with real C++/Node/Apache, schema-v11 MariaDB and matched private files. | Actual Playwright and integrated restore evidence are CI-dependent. No private authenticator key export, physical device, synced-key or production recovery certification. |
| **0.8.6 — Passkey races and content integrity** | `private-catalog-integrity.ts` SHA-256 fingerprints the file behind a verified `O_NOFOLLOW` descriptor, detecting valid-shaped same-length in-place counter rollbacks even when mtime is restored. A gateway test proves that concurrent revoke defeats a delayed rename; existing account, epoch, grant, backup and counter tests remain. | In-process trusted digest, not a cryptographic offline source-of-truth or defense against a compromised runtime. |
| **0.8.7 — MFA and limiter integrity** | The same guard protects the open rate-limit catalog. A test edits a budget from 4 to 1 while preserving inode, size and mtime; the live store fails closed. Existing TOTP rotation, consumed-step replay, clock reversal, audit outages and persisted rate-budget tests remain required. | **Known limitation:** a fully offline valid-looking rewrite cannot be authenticated across a cold restart without a separate durable keyed MAC/anchor. Adding one would require an Owner-approved on-disk migration and rollback; no format change is made implicitly. |
| **0.8.8 — Cross-tab session and accessibility** | `RemoteApp.tsx` sends only the fixed `session-revoked` BroadcastChannel signal after successful TOTP or passkey sign-in, in addition to existing logout. Playwright checks a sibling's stale private metrics disappear, the sign-in input gains focus and EN/PL guidance stays clear and secret-free. Existing CSRF/expiry/late-response tests remain. | Broadcast is a UI cache invalidation, not server-side authority or cross-device logout. C++ still verifies access for protected requests. |

## Exact-head Ubuntu gate

The Stage4 disposable VM workflow now runs `beta08_stage2_gate.py` after genuine Playwright and C++/Node/Apache/MariaDB restoration. It fails closed on missing/duplicate/skipped/failed browser cases, wrong SHA/Ubuntu image, missing restored browser-login/replay/revocation evidence, or fake C++ grant success; then writes only redacted `beta08-stage2-authentication.json`. Synthetic positive/negative tests of the evaluator in `beta08_stage2_gate_tests.py` are **not** real hosted evidence.

Final acceptance still requires the full exact-final-head GitHub-hosted **Ubuntu 22.04, 24.04 and 26.04** C++ Debug/Release, WebAdmin/Chromium, CodeQL, sanitizer, ADC/ADCS/TLS, systemd, recovery/rollback and benchmark matrix. Any skipped-required, pending, failed or wrong-head job blocks acceptance. No claim of physical passkey certification, real production RPO/RTO, SLA, public WebAdmin or passwordless authentication.

## Local checks and Owner handoff

- 59 targeted Node security regressions passed; four Stage II gate tests and four earlier Stage I gate tests passed; WebAdmin TypeScript/Vite build and diff checks passed.
- Local Playwright was attempted but Chromium failed at startup with **`socket() EPERM`** in this execution container. It is **not** a passing browser test and must be executed in hosted CI.
- MariaDB schema v11, all five owner-only stores and their file formats, C++20 password/grant authority, private HTTPS boundaries and product identifier `beta-0.7.20` remain unchanged. Candidate metadata belongs to Stage V.

Record the actual pushed SHA and new CI links in PR #241, then **stop without polling or waiting for GitHub Actions**. Keep the PR draft and `main` untouched; no merge, tag, release, production package or deployment.
