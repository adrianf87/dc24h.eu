# Beta-0.8.5–0.8.8 — Stage II authentication and credential-device lifecycle

Scope: work unit **2 of 10** in [Issue #240](https://github.com/adrianf87/dc24h.eu-high-starter/issues/240), on the existing `beta-0.8.x-stage-full` branch and single draft [PR #241](https://github.com/adrianf87/dc24h.eu-high-starter/pull/241). Stage I is preserved. Stages III–V and five subsequent fix passes need separate Owner authorization. **Implementation is not acceptance, merge approval or permission to deploy.**

## Fix Pass 2 — independent review and repairs (work unit 7/10)

Two confirmed defects received code changes and negative regressions:

1. **TOTP enrollment rewritten in place:** A live verifier previously pinned inode/size/mtime but did not authenticate content. An outside same-length replacement of a private seed with restored mtime could leave the old in-memory secret usable after the intended rotation. The verifier now checks a bounded SHA-256 digest and ctime of its pinned enrollment before every attempt, including an opening-time reread. A test changes one valid seed to another without altering file length or mtime, then demands fail-closed behavior and no replay-ledger append.
2. **Passkey persisted, completion refused:** After a valid WebAuthn enrollment was durably added, a failing strict completion audit could return HTTP 403 while leaving the enrollment caller and its sibling sessions valid. Now any failure *after confirmed persistence* removes all existing sessions of that actor. A two-browser gateway regression injects a failing `passkey_enrolled` audit event and asserts both cookies are refused, without representing the persisted credential as rolled back.

**Reviewed, existing negative evidence:** Real ES256 RP/Origin/UV/UP/challenge validation; signed counter monotonicity and immutable backup eligibility; revoke/epoch races and C++ grants; TOTP consumed-step persistence, independent verifiers, rotation and backward clock; same-length/same-mtime passkey counter and rate-limit budget modifications; one-use challenges, CSRF, and stale browser sessions. Software/Chromium virtual authentication is not physical hardware or synchronized-passkey certification.

**Explicit residual risk:** Existing schema-1 passkey and limiter catalogs have an in-memory digest only. A fully offline valid-shaped rewrite before restart cannot be authenticated by SHA-256 on a fresh process. Neither schema, extra MAC/anchor nor rollback semantics is changed without Owner approval. A hostile actor able to rewrite owner-only state is outside this pass's cold-start authenticity claim.

Locally, the 26 targeted MFA/passkey/limiter/WebAuthn/gateway tests and WebAdmin TypeScript typecheck passed. Hosted Chromium, C++/Node/Apache/MariaDB restore, full Node and required Ubuntu matrix are separate CI evidence, not claimed passing by this local review. The exact pushed SHA belongs in the PR handoff; stop after push/CI trigger without waiting for outcomes.

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
