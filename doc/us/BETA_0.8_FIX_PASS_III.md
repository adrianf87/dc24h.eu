# Beta 0.8.x Fix Pass 3 — Stage III disposition (0.8.9–0.8.12)

**Scope:** work unit 8 of [Issue #240](https://github.com/adrianf87/dc24h.eu-high-starter/issues/240). Reuse `beta-0.8.x-stage-full`, draft PR #241 and protected `main`. This pass reviews *only* the previous Stage III work while preserving Stages IV/V. The actual commit SHA and hosted Ubuntu checks are recorded at the PR handoff; a pending/aborted run is never evidence of acceptance.

## Findings and fixed regressions

| Review area | Evidence-based disposition |
| --- | --- |
| Partial SQL plus five private stores | Confirmed offline read path-swap gap: inode/mtime checked on the opened source but a different file could replace the pathname before hashing completed. `stage1_recovery_bundle.private_hash` now opens through a pinned owner-only directory descriptor, hashes the same FD twice in bounded chunks, and checks the current name's inode against the opened FD. Negative regression replaces the source with identical bytes and size while hashing. |
| Intermediate ancestor and inode/path aliases | Shared Python `private_directory_fd` verifies **all** ancestors twice, pins and rechecks the owner-only directory inode, and uses `dir_fd` for private reads/writes and audit/replay snapshot I/O. Symlinked parents or a path swap at `open()` fail closed. Traversal aliases (`..`) and final symlinks are rejected. Node also rejects noncanonical dot, repeated-slash and trailing-slash directory aliases across the five-store validator. |
| Authenticated recovery manifest | Detached HMAC with independently pinned key ID remains opt-in; no trust key is stored in the bundle. The anti-reuse ledger is now opened relative to a validated directory FD; before accepting a pair, its *current* name must still resolve to the same inode under the exclusive lock. A deterministic replacement before consumption is refused; ledger is not rewritten/reset. Schema/unknown-key, pair-digest tamper and replay regressions remain mandatory. |
| Compiled runtime-only gateway | The CI fixture still installs only `npm ci --omit=dev`, excludes `tsx`/TypeScript/esbuild and starts/restarts the compiled private loopback gateway on Ubuntu. Fixed a fixture race that could miss a fast exit after SIGTERM and wait for a stale timeout. This is not evidence of a successfully installed **systemd service**. |
| Failed package transition | CI-only `postinst`/`preinst` failures, failed archive, explicit previous-version `dpkg` reinstall, inert service guard and previous executable hash/version checks remain. The package source is beta-0.8.20 after Stage V alignment; failed `postinst` fixture version is `0.8.20+ci3`. No production `.deb` is installed. |
| Reauthentication after downgrade | The historical C++ executable and MariaDB v11/disabled-account state are reopened and Node verifies synthetic TOTP/replay, revoked passkey, audit and budgets. **An actual password+TOTP / password+passkey HTTP/browser login against that downgraded historical C++ executable is not executed.** This specific Stage III acceptance item remains open rather than being reported as passed. |

## Regression commands and protected gates

```sh
python3 -m unittest discover -s tests -p beta08_fix_pass3_tests.py
python3 -m unittest discover -s tests -p stage4_private_recovery_tests.py
python3 -m unittest discover -s tests -p stage1_recovery_bundle_tests.py
python3 -m unittest discover -s tests -p beta08_recovery_auth_tests.py
python3 -m unittest discover -s tests -p beta07_ci_deb_tests.py
cd webadmin
node --import tsx --test tests/remote-private-init.test.ts
npm run typecheck
npm run build:remote:ci
```

GitHub-hosted Ubuntu **22.04, 24.04, 26.04** must additionally execute the full C++/Node/Apache/MariaDB v11 package rollback and compiled gateway smoke in `.github/workflows/stage4-ubuntu-vm-validation.yml`, as well as C++/WebAdmin/browser, sanitizer and CodeQL checks pinned to the same source SHA. Neither the current sandbox's `AF_UNIX listen EPERM` nor a cancelled earlier run counts as a successful test. The fix-pass negatives are mandatory in that Ubuntu VM workflow, before the package transition.

## Residual boundaries / explicit Owner decisions

1. These descriptor-relative opens harden the offline Python tooling; Node's pathname-based check does not implement a kernel-atomic `openat2(RESOLVE_NO_SYMLINKS)` walk. Do **not** run recovery while hostile directory writers can rename the tree.
2. SQL read locking does not quiesce the five Node stores. Stop C++/gateway writers, capture matching schema v11 SQL and owner-private files together, verify digest and HMAC with independently retained key ID and anti-reuse ledger, then restore to **fresh** paths only. No live atomic snapshot or production RPO/RTO is established.
3. Full reauthentication through the old packaged executable, installed-systemd startup/restart (not just the compiled process), and real browser/passkey downgrade remain **unverified**. Do not authorize a production rollback or claim Stage III final acceptance on the existing synthetic checks alone.
4. The detached HMAC proves shared-key knowledge, not nonrepudiable signer identity; a lost or corrupt external anti-reuse ledger must stop acceptance, never be silently reinitialized.

Stop after publishing the exact Fix Pass 3 head and observing CI start. No merge to `main`, release, deployment, tag, production key use, Debian/self-hosted/Xeon testing or Owner acceptance is implied.
