# Beta 0.8.9–0.8.12 — Stage III: quiesced recovery, authenticity and disposable rollback

Scope: **only work unit 3** in [Issue #240](https://github.com/adrianf87/dc24h.eu-high-starter/issues/240), on `beta-0.8.x-stage-full` and draft PR #241. This is an isolated CI/recovery prototype, not a production package, online backup, RPO/RTO claim, or merge authorization. MariaDB remains schema v11; the five private stores and C++ grant/password authority remain unchanged.

Later review: [Fix Pass 3 EN/US evidence and residuals](BETA_0.8_FIX_PASS_III.md). The original stage description below is retained for traceability.

## 0.8.9 — six-component consistency and full private-path traversal

- Maintain the already-required stopped-writer coordinator: stop C++/Node writers, take the MariaDB read lock, export schema-v11 SQL while holding the lock over a separately verified snapshot of `mfa.json`, `replay.jsonl`, `audit.jsonl`, `passkeys.json`, and `rate-limits.json`, and create the SHA-pinned pair manifest before re-enabling writers. A read lock alone does **not** stop Node file writers. No online atomicity is claimed.
- `remote-private-path.ts` validates **every directory component** through `lstat`, checks a second stable inode traversal, and requires the final owner-only directory. The same check is used by Node MFA/replay, audit, passkey, rate-limit and first-installation paths, including revalidation before updates. File leaves retain `O_NOFOLLOW`, owner/mode and inode checks. Intermediate symlink aliases, non-directory ancestors and post-open ancestor swaps fail closed. This improves path checks but is not a kernel-level `openat2(RESOLVE_NO_SYMLINKS)` atomic guarantee in the presence of hostile concurrent directory renames; isolate and stop writers during recovery.
- Synthetic Node negative cases prove the old intermediate-ancestor alias is rejected across all five stores and the bootstrap. Existing Python private backup, paired source and SQL preflight regressions retain partial/torn/cross-generation and non-overwrite refusals.

## 0.8.10 — optional, independently pinned manifest authenticity

The historical `recovery-pair.json` hashes establish **integrity only**, not authorship. `scripts/beta08_recovery_auth.py` adds an **opt-in detached HMAC-SHA256** envelope (`recovery-auth.json`) and a separate, owner-only anti-reuse ledger. The 32-byte random HMAC key, its expected SHA-256-based ID and the consumed-pair ledger must be distributed through an independently controlled offline trust channel, kept **outside the backup, repository, CI artifacts and restored directory**, and retained across rollbacks. Never reuse a key intended for browser, TOTP, service token or C++ credentials.

Isolated example after the standard snapshot and `stage1_recovery_bundle.py pair`:

```sh
# Each of /secure/trust and /secure/auth must already exist, owner-only (0700).
# /secure/trust/key is an independently provisioned 32-byte file (0600).
python3 scripts/beta08_recovery_auth.py init-ledger --ledger /secure/trust/accepted-pairs --ack-isolated --ack-stopped
python3 scripts/beta08_recovery_auth.py sign --pair-directory /secure/offline/pair --auth-directory /secure/auth --trusted-key /secure/trust/key --ack-isolated --ack-stopped
python3 scripts/beta08_recovery_auth.py verify --mariadb-dump /secure/offline/state.sql --private-snapshot /secure/offline/snapshot --pair-directory /secure/offline/pair --auth-directory /secure/auth --trusted-key /secure/trust/key --expected-key-id INDEPENDENTLY_PINNED_32_HEX_KEY_ID --commit-sha EXACT_40_HEX_SHA --ledger /secure/trust/accepted-pairs --ack-isolated
# Only after independent Owner approval, use 'accept' with --ack-stopped
# immediately before a fresh isolated restore; this consumes the pair once.
```

The `verify` operation never imports SQL or mutates the backup or ledger. `accept` appends the digest with an exclusive file lock and fsync, refusing replay or missing/corrupt ledger. Unknown key IDs, wrong format, changed SQL/private pair, tampered MAC and reuse fail closed. This symmetric HMAC demonstrates knowledge of the shared key, **not non-repudiable signer identity**. An approved detached Ed25519 public-key option could support independent verifiers later, but is not silently implemented or mandated here. A privileged operator controlling both the trust key and ledger can forge/erase evidence; a separately retained trust anchor is mandatory.

## 0.8.11 — package failures and production-dependency-only runtime dry-run

- `beta07_ci_deb.py` remains an inert **test-only** dpkg fixture. A new `0.8.20+ci3` variant deliberately fails during `postinst`, after unpack, in addition to the existing corrupt archive and `preinst` failure. The hosted Ubuntu test records a non-configured dpkg state and explicitly reinstalls the original `0.5.16+ci1` package, checking the exact executable/version and unchanged private data. No service is enabled or a deployable package published.
- The example gateway unit no longer depends on the development-only `tsx` loader. A separate build (`npm run build:remote:ci`) bundles TypeScript to `dist/remote-main.mjs` with a pinned direct `esbuild` development dependency. The GitHub-hosted Ubuntu job copies only this JS artifact and the npm lock to an isolated directory, runs `npm ci --omit=dev`, verifies absence of `tsx`, TypeScript and esbuild, then attempts local-only synthetic gateway start/restart with private files and a Unix C++ API socket fixture. The example is not installed/enabled by CI, and production installation/ownership/systemd deployment must be separately reviewed.

## 0.8.12 — restored credential/grant state after real downgrade

The disposable dpkg run revalidates matched SQL+five stores using the optional independent HMAC gate, restores to **new** MariaDB/private destinations, boots the real previous C++ binary, checks schema-v11/owner grants, preserves a disabled synthetic account across a restart, and separately reopens the restored Node TOTP/replay, revoked passkey, audit and rate-limit stores. A consumed TOTP cannot be reused, a previously revoked passkey stays revoked, and the same signed recovery pair cannot be accepted twice. Existing broader WebAuthn Chromium and expired-session CI tests remain required.

This **does not** independently establish a full user-operated browser password+passkey login against the downgraded C++ binary or instant active-session revocation. Those are explicitly residual acceptance/production-rehearsal limits; do not claim them from separate synthetic regressions.

## CI and Owner gate

Locally run Python recovery/auth/package unit tests and Node TypeScript/private-store tests; the runtime-only process smoke depends on Unix sockets and must complete on GitHub-hosted Ubuntu 22.04/24.04/26.04, with the actual MariaDB/dpkg/C++ and Chromium/CodeQL/sanitizer matrix. Record the exact final source SHA and workflow links in PR #241. **Stop immediately after push/CI trigger. CI pending is not accepted.** No private-runner/Debian/Xeon tests, release, main merge or production deployment.
