# Beta 0.7.x — Fix Pass 2: WebAuthn, MFA and session integrity (0.7.5–0.7.8)

**Disposition: a confirmed concurrent-grant defect was repaired and targeted local tests passed; exact-final-head hosted CI is not yet verified. NO-GO for merge or deployment.** This is Fix Pass 2 of 5 under [Issue #238](https://github.com/adrianf87/dc24h.eu-high-starter/issues/238), on the existing `beta-0.7.x-stage-full` branch and single draft [PR #239](https://github.com/adrianf87/dc24h.eu-high-starter/pull/239). No other pass is started by this change.

## Confirmed defect and regression

The private gateway checked C++ management grants *before* awaiting the streamed JSON body of `passkey/device/rename` or `passkey/device/revoke`. If the C++ authority revoked grants during that read and no other browser request invalidated the gateway's session map, the gateway could still rename or revoke a credential using its earlier authorization decision. This violated Stage II's concurrent grant-change contract.

`webadmin/server/remote-gateway.ts` now performs a second authoritative C++ management check **after the request body is fully received and before either credential mutation**. The existing session-map, pending-logout, expiry and passkey-epoch checks still run after that asynchronous authorization. A refusal or unavailable upstream fails closed. `webadmin/tests/remote-passkey-gateway.test.ts` pauses a real async body iterator, withdraws the C++ grant, then completes the body for both rename and revoke. Both are refused without changing the surviving credential, and the stale browser session is invalidated.

## Stage II review ledger

| Patch | Inspected source and regressions | Disposition / remaining boundary |
| --- | --- | --- |
| **0.7.5** | `remote-webauthn.ts`, `beta07-webauthn-restore.spec.ts`, `remote-webauthn.test.ts`, hosted `beta06_authenticated_api_process_tests.py` integration entry | Existing real ES256 Origin/RP/challenge and one-use verification unit checks retained. Browser restore requires the hosted Chromium + matched SQL v11/five-private-store evidence; not certified by this local run |
| **0.7.6** | `remote-passkeys.ts`, `remote-passkeys.test.ts`, `remote-passkey-gateway.test.ts` | Existing persisted counters, fixed backup eligibility, account isolation, device revocation, epoch/session invalidation and surviving-device tests passed locally; no separate defect confirmed |
| **0.7.7** | `remote-mfa.ts`, `remote-mfa.test.ts`, `remote-passkey-recovery.test.ts`, replay and limiter checks | Existing consumed-step ledger, interrupted append, rollback, stopped-writer rotation, restored history, strict private files and audit tests passed locally; no separate defect confirmed |
| **0.7.8** | `remote-gateway.ts`, `remote-gateway-auth.test.ts`, `remote-passkey-gateway.test.ts` | **Fixed grant-revocation race for streamed passkey rename/revoke** with negative regressions. Existing multi-session, rotated CSRF, one-time challenges, delayed responses and logout tests passed locally |

## Executed checks and remaining gate

- Targeted Stage II Node security tests: **54 passed, 0 failed**.
- WebAdmin TypeScript check: **passed**; `git diff --check`: **passed**.
- Full local Node suite: **147 passed, 3 could not run** in this execution environment because creating local Unix-domain test sockets failed with `listen EPERM`. These are environment-bound test failures, not counted as successful checks.
- Local Playwright Chromium restore: **not executed successfully**; the Chromium binary was absent and the download endpoint returned HTTP 403. The real browser/system-level gate must run on hosted Ubuntu.

The required acceptance gate remains **all applicable targeted tests plus the complete exact-final-head GitHub-hosted Ubuntu 22.04/24.04/26.04 C++ Debug/Release, WebAdmin/Chromium, security, CodeQL, ASan/UBSan, ADC/ADCS/TLS, systemd, MariaDB/five-store recovery, package rollback and benchmark checks**. Correlate all required job statuses, redacted artifacts and source SHAs against the new commit. Neither a queued nor a passing older-head run establishes Fix Pass 2 acceptance. The final pushed SHA and new run URLs belong in the draft PR handoff; under the Owner's instruction, stop immediately after push/CI trigger without polling for completion.

No MariaDB schema, privileged C++ grant model, browser-exposed credential, production service or public listener was changed. Keep the PR draft, `main` protected, and release/deployment blocked pending separate Owner authorization.
