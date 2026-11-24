# Beta 0.7.5–0.7.8 — Stage II: browser WebAuthn, MFA and session integrity

This is work unit 2 of [Issue #238](https://github.com/adrianf87/dc24h.eu-high-starter/issues/238), using the existing `beta-0.7.x-stage-full` branch and the single draft PR #239. Earlier commits provided preliminary Stage II code; this work unit reviews, hardens and exercises that code. It does not accept Stages III–V, publish a release, change `main`, or authorize deployment.

## Four patch contracts and executable evidence

| Patch | Behavior and evidence | Negative boundary |
| --- | --- | --- |
| `beta-0.7.5` | `webadmin/tests/beta07-webauthn-restore.spec.ts` uses actual Chromium virtual authenticators and a restored private credential catalog. The GitHub-hosted `tests/beta06_authenticated_api_process_tests.py` separately exercises live Chromium through Apache HTTPS, Node, the C++ account authority and a matched stopped-writer MariaDB v11 plus five-private-store restoration. | Wrong Origin/RP ID and challenge, replayed assertion, revoked device and pre-restore cookie must be refused; the real hosted restore report is required, not inferred from the isolated verifier test. |
| `beta-0.7.6` | `remote-passkeys.test.ts`, `remote-webauthn.test.ts`, `remote-passkey-gateway.test.ts` and the two-device Chromium scenario cover independent keys, persisted counters, backup flags, names, account ownership, single-device revocation and actor-wide invalidation. | Backup *eligibility* cannot switch after enrollment in either direction; a foreign user handle cannot cross an actor boundary; revoking one key invalidates every current session for that actor while preserving unrelated accounts and the remaining usable key. |
| `beta-0.7.7` | `remote-mfa.test.ts` exercises TOTP replacement with proof of the old and new factor, persisted consumed time steps, stopped-writer snapshot/restore and interruption/clock reversal. `remote-rate-limit-store.test.ts` checks rate budgets survive restarts. | Corrupt/partial replay, old TOTP after replacement, reused codes, lost audit or limiter data, and clock reversal must fail closed without silently resetting security history. |
| `beta-0.7.8` | `remote-gateway-auth.test.ts`, `remote-passkey-gateway.test.ts` and `remote-gateway.spec.ts` exercise simultaneous sessions/tabs, one-use CSRF, cookie rotation after device changes, late HTTP responses, permission revocation and in-flight logout. | An old cookie, stale CSRF, late grant result or delayed browser response cannot create/revive access or overwrite a newer authenticated session. |

## Security invariants

- Browser sign-in still requires **C++-verified password + TOTP** or **C++-verified password + enrolled passkey**. No passwordless access, browser-visible service token, public listener, or browser hub-control write is introduced.
- RP ID and HTTPS Origin are configured explicitly; client Host or proxy headers cannot choose them. The passkey private key remains in the authenticator, never in the MariaDB or Node store.
- Passkey revocation persists across Node restart and advances the actor's session epoch. Device rename/enrollment rotate the browser cookie and CSRF; enrollment also invalidates older sessions for that actor.
- TOTP replay, redacted audit, rate budgets, credential catalog and database migration state must be recovered as one matching stopped-writer set, never reconstructed from fresh empty files. Use synthetic isolated data in CI.

## Verification and acceptance status

Run frontend typecheck/build, all Node unit/security tests and the required Playwright browser tests. On GitHub-hosted Ubuntu **22.04, 24.04 and 26.04 only**, require same-final-head C++ Debug/Release, WebAdmin/browser, CodeQL, sanitizers and disposable VM recovery evidence. The VM `beta06-restored-login.json` must prove the real browser login and revocation after matched restoration; the integrated chaos gate rejects absent, wrong-head or false evidence. A Chromium virtual authenticator does not certify a physical passkey or a production restore.

Under the Owner's no-wait instruction, after pushing the Stage II work, record the exact branch SHA and available GitHub Actions run URLs, then **stop without polling or waiting for CI to finish**. Pending checks mean `implementation delivered / CI unverified`, not Stage II acceptance. Only a later explicitly authorized check can mark this unit accepted when all mandatory final-head results are green. Keep PR #239 draft; do not merge or deploy.

## Limitations

- Local unit tests cover deterministic security and file-state paths; a full C++/MariaDB/Apache/Chromium system test requires the hosted Ubuntu fixture.
- Physical authenticator backup/sync behavior, external customer accounts, public gateway exposure and production RPO/RTO are not asserted.
- Previous Stage I evidence is independent; this document does not retroactively mark it accepted.
