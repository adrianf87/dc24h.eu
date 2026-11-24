# Beta 0.8.x — final-head stage inventory and hosted CI repair

Issue [#240](https://github.com/adrianf87/dc24h.eu-high-starter/issues/240); shared draft [PR #241](https://github.com/adrianf87/dc24h.eu-high-starter/pull/241), branch `beta-0.8.x-stage-full` from protected main `20eae07`.

## Implementation inventory

All ten Owner-authorized work units are represented by distinct branch commits: Stage I (`884d9ab` through `fb4edc1`), Stage II (`7ae4b9f`), Stage III (`e00016a`), Stage IV (`e131674`), Stage V (`5767c5f`), Fix Pass 1 (`5d5465e`), Fix Pass 2 (`33efe78`), Fix Pass 3 (`d1c854c`), Fix Pass 4 (`8906944`) and Fix Pass 5 (`c3c939d`). Presence of source commits is **not** an accepted final-head matrix or Owner approval. The PR remains draft and the issue must stay open pending exact-head acceptance.

## Confirmed failure and correction

The completed [VM run 45471721340](https://github.com/adrianf87/dc24h.eu-high-starter/actions/runs/45471721340), source `c3c939d`, passed on Ubuntu 22.04 and 24.04 but failed on Ubuntu 26.04 at `Measure authenticated real C++ to Node to Apache HTTPS`. The actual Chromium failure was `client_error:restored_options_429`. All 24 authenticated HTTPS measurement requests passed and CTest, WebAdmin browser and C++ API integration steps had passed before this failure. Separate exact-head C++/WebAdmin, CodeQL and TSAN/fuzz workflows were green for that older head.

The real synthetic restore fixture performs an optional consumed-TOTP replay denial, a fresh password+TOTP login and a **second** password+passkey login. The gateway intentionally persists its per-actor four-per-60-second login quota across the MariaDB/five-store checkpoint; on a faster runner, a fifth valid login attempt can hit HTTP 429 while the original budget is still active. That is appropriate gateway behavior, but prevents the CI fixture from actually checking the recovered WebAuthn credential.

The fixture now reads **only the local disposable restored limiter catalog**, derives the already persisted HMAC bucket keys from the original synthetic service token, and permits Chromium to attempt its fresh, separately authorized passkey login **only after the existing quota expires**. The one-second C++ password-authority budget is still respected. A missing/malformed actor/global bucket aborts rather than wiping state or silently resetting an authentication quota. Production limiter thresholds, replay behavior, C++ authorization, private data format and restore semantics are unchanged.

The hosted VM workflow runs five negative/positive unit cases for this scheduling helper before the full real Chromium fixture. Local Python syntax, 5/5 budget tests, project/documentation and beta-0.8.20 metadata checks passed. The real C++/Node/Apache/MariaDB v11/five-store/Chromium integration **requires the next exact-head GitHub-hosted Ubuntu 22.04/24.04/26.04 workflow**; this local environment lacks the complete toolchain and is not acceptance evidence.

**Handoff:** after pushing the repair, stop without waiting for or polling new CI. Do not mark issue/PR complete, release, tag, publish, merge to main, or activate a production listener without later exact-head evidence and explicit Owner approval. No production SLA, physical WebAuthn device, independent DC++/AirDC++ certification or target-host recovery guarantee is implied.
