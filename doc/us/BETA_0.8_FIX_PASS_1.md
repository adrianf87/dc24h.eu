# Beta 0.8.x — Fix Pass 1 / Stage I (`0.8.1`–`0.8.4`)

Issue [#240](https://github.com/adrianf87/dc24h.eu-high-starter/issues/240); shared draft [PR #241](https://github.com/adrianf87/dc24h.eu-high-starter/pull/241) on `beta-0.8.x-stage-full`. This review is limited to Stage I interop, account/revocation boundary, real-client provenance, search/transfer tokens and ADCS TLS. It does not authorize another stage, merge, release or production activation.

## Findings and disposition

| Patch | Evidence and finding | Fix or disposition |
| --- | --- | --- |
| **0.8.1** | Rechecked mandatory ncdc package-version and apt-index/executable SHA-256 inventory, JUnit exact-case matching, source SHA, three Ubuntu image pins and explicit `DC++`/`AirDC++` **untested** states. The unit gate already rejects spoofed clients, wrong head, missing/duplicate/failed JUnit and nonfinite durations. | No new executable client is claimed. Added direct negative tests proving an individual `<failure>`, `<error>` or `<skipped>` node cannot be overridden by a `status="run"` attribute. Real native DC++/AirDC++ remain untested. |
| **0.8.2** | **Confirmed Stage I defect:** the [completed Stage I C++ run](https://github.com/adrianf87/dc24h.eu-high-starter/actions/runs/45465183203) failed `accounts.adc_authentication.process` on all six Debug/Release hosted Ubuntu jobs. The new revocation test ran `UPDATE ... WHERE name='RegisteredADC'`, but `RegisteredADC` is an **ADC nickname** stored in `dc24h_account_identities`. The matching account row is named `AuthAccountOne` in the disposable fixture. The UPDATE silently affected **zero rows**, and the next login was correctly allowed for the still-active account. | Resolve account through the actual unique `dc24h_account_identities.nickname → account_id` relationship, update only that mapped row and verify the affected-row count and resulting status. Both disable/re-enable must change **exactly one** row; a zero-row no-op now fails explicitly. A no-op is tolerated only in best-effort test cleanup. Preserve existing requirement for a controlled C++ restart and old-session disconnect—this is **not** hot revocation. |
| **0.8.3** | Rechecked real ADC/ADCS directed TTH search, DRES token, denied passive CTM, resumed TCP mode and unknown-SID refusal. Existing ncdc search, peer-token, port-scope, transfer-limit and error-recovery cases remain required in Stage I JUnit gate. | No separate source defect confirmed. Preserve the negative path and ensure CI executes the exact CTest cases. Python fixtures do not count as additional independent shipped clients. |
| **0.8.4** | Reviewed both TLS 1.2/1.3 hostname/certificate cutover, stale trust, key mismatch, legacy cipher and downgrade checks. The Stage I plaintext-to-ADCS assertion only checked that two specific ADC command strings were absent: arbitrary application bytes could still have passed this test. | Tighten the assertion: a plain socket reaching the ADCS port may receive **only EOF/reset or a bounded fatal TLS alert**, never arbitrary response data. Retain old/new CA trust checks and no public/plaintext fallback. |

## Negative regressions and exact-head gate

- `tests/adc_authentication_process_tests.py` — actual MariaDB **join to nickname mapping**, checked affected-row count (exactly one in the main test), checked resulting status, old-session shutdown, disabled ADC/ADCS login refusal and approved re-enable. The parent database is always disposable; no production SQL is executed.
- `tests/tls_security_process_tests.py` — strict `assert_fatal_tls_alert_or_eof` for the plaintext ADCS cutover path, with bounded reads and EOF/reset acceptance.
- `tests/beta08_stage1_gate_tests.py` — required-Ctest `failure`, `error` and `skipped` child nodes reject a false PASS despite the surrounding `status="run"`.
- Run local Python static/gate, documentation and frontend checks before publishing; **the real MariaDB/C++/OpenSSL process regressions cannot be reproduced in this workspace without the full hosted build**. They must run on GitHub-hosted Ubuntu 22.04/24.04/26.04 on the new exact SHA.
- No broader claims: ncdc is the only executed independent client; DC++/AirDC++ remain untested, native TLS/certificate failures are synthetic, and the account directory still requires a controlled restart after revocation.

## Handoff

Fix Pass 1 changes should be committed to the **existing** `beta-0.8.x-stage-full` branch and [PR #241](https://github.com/adrianf87/dc24h.eu-high-starter/pull/241), preserving Stages II–V already present on that branch. The entire C++/WebAdmin/CodeQL/sanitizer/Ubuntu-VM matrix must run on the **new exact source SHA**. Per Owner rule, **stop after push/CI triggering; do not wait or poll the result**. A pending or failed check is not Owner acceptance. Later Fix Passes 2–5 still require separate instructions.

**Status: Fix Pass 1 fixes implemented; exact-head hosted CI pending/unverified, not accepted, PR still draft.**
