# Beta 0.5.9–0.5.12 — Stage III reliability and WebAdmin operations

Work unit 3 of [Issue #234](https://github.com/adrianf87/dc24h.eu-high-starter/issues/234) on **the existing** `beta-0.5.x-stage-full` branch and draft PR #235. This is development-only: do not merge, enable a public listener, restore production data, change MariaDB schema or expose hub-control commands. Only GitHub-hosted Ubuntu 22.04, 24.04 and 26.04 CI are in scope.

## 0.5.9 — Consistent private state and MariaDB v11

The recovery unit remains one stopped, disposable MariaDB v11 SQL dump plus the five owner-only gateway files: `mfa.json`, `replay.jsonl`, `audit.jsonl`, `passkeys.json`, `rate-limits.json`. `scripts/stage4_private_recovery_drill.py` checks every private file's format, size, owner, permissions, no-symlink rule, audit chain and manifest hash before copying, and rechecks on restore. **Stage III closes a validator divergence:** stored passkey handles, IDs and public-key encodings must be canonical unpadded base64url in both the running Node verifier and the offline snapshot tool; duplicate passkey transports are rejected. Formerly, a backup might have been accepted although the restored gateway would refuse to start. The same tests cover a valid revoked key and malformed catalog rejection.

The paired MariaDB/private manifest includes the source commit and both SHA-256 records; an operator can now request `--commit-sha` in **verify** mode to fail on a different source revision. That field identifies the recorded source code, not a cryptographic signature or proof that the SQL dump and private files were simultaneously committed. Perform the snapshot while services are stopped and verify the same pair before isolated import. A restore never rewinds an active replay ledger or rate budget.

**Credential-catalog storage decision:** retain the small fsynced, private passkey file for beta-0.5.12. Moving it into MariaDB v11 would introduce a schema migration and new transaction/authentication failure paths without demonstrating a consistency or capacity benefit for the existing one-gateway pilot. The current five-file recovery process makes the coupling explicit. Reassess only if multi-writer deployment or measured consistency failures justify a documented migration and executable rollback. No SQL schema was changed in this stage.

## 0.5.10 — Apache2 and systemd failure boundaries

- The separate TLS vhost still listens only on `127.0.0.1:8443`, accepts fixed API paths, rejects unexpected Host/Origin/Fetch Metadata, strips browser-supplied backend credentials and forwards no hub-control writes.
- Bound request line/header sizes, keep-alive count and timeout. Negative real-Apache tests add encoded, traversal and suffix variations of sensitive URLs; even a mock upstream must not receive them.
- The optional gateway unit retains loopback-only network and private state directory, caps restarts to five per 60 seconds, bounds stop time, file descriptors, tasks and memory, and uses mixed kill semantics. Node/V8 executable memory remains permitted. This is an example for deliberate review, not an installed or enabled service.
- Actual GitHub-hosted systemd manager and privileged sandbox checks remain mandatory in CI. A missing manager or required integration fixture is a failure, not a skip. Resource bounds are initial conservative development values, not measured target capacity.

## 0.5.11 — ADC/ADCS failure and restart regressions

Real loopback process tests now exercise two successive hub lifetimes after concurrent, fragmented malformed ADC frames and after bursts of invalid TLS handshakes. Following failure/EOF, a fresh valid client must connect, parse and shut down cleanly. Earlier connection quotas, TLS version/AEAD, graceful SIGINT/SIGTERM, output-drain deadlines and concurrent client tests remain active. These bounded regressions test failure cleanup; they are not production endurance or Xeon performance evidence.

## 0.5.12 — Remote WebAdmin operator feedback

- Show the approximate remaining expiry of the current authenticated session without showing its cookie/CSRF. Following a passkey mutation, recheck the rotated session on the server before displaying a new lifetime; an old reply cannot rewrite a newer session.
- Display per-device creation and last-use information. Device revocation uses a localized, accessible two-step confirm/cancel flow; an operator cannot revoke a key by one accidental click. Disable repeat mutation while a request is pending.
- A failing device-list fetch clears cached device actions and shows an explicit EN/PL outage message. A 401/403 invalidates the browser session instead of retaining privileged controls. Existing tests cover stale responses, session rotation and refused enrollment.
- All capabilities remain **read-only** for hub state. Authentication is still password + TOTP or password + passkey. Neither TOTP seed, private API token nor backend grant is stored in the browser.

## Operator and validation checklist

1. Stop the optional gateway and MariaDB before taking a matched snapshot; never initialize missing state on a previously active installation. Preserve all five private files and the SQL dump together with their manifest.
2. Run the isolated offline snapshot/verify commands documented in [Stage I](BETA_0.5_STAGE_I.md) and [Stage II](BETA_0.5_STAGE_II.md). Supply the expected source SHA to the pairing verifier when available; verify audit and passkey format before importing disposable SQL. Restore only into an empty isolated directory.
3. Run all required final-head GitHub-hosted Ubuntu 22.04/24.04/26.04 C++ Debug/Release, WebAdmin, browser, ASan/UBSan, real systemd, Apache security, performance and disposable recovery jobs; inspect any failure and rerun on a repaired final SHA.
4. Record exact final SHA, workflow links and limitations in Issue #234 and PR #235. Keep the PR draft, issue open and subsequent units unchecked; no production deployment or additional assessment is part of this work unit.

**Validation state:** Follow the latest-head CI evidence and Stage III checklist in Issue #234; no independent security or target-hardware acceptance claim is made here.

## Fix Pass 3 — Stage III re-review (2026-11-24)

The review revisited the matched recovery pair, Apache/systemd restrictions,
ADC/ADCS fault regressions and authenticated device/session feedback. Two
confirmed defects were repaired without changing the recovery schema, remote
listener, privilege model or C++ protocol budgets:

- **S3-F1 — Recovery pair placement:** `stage1_recovery_bundle.py` could create
  `recovery-pair.json` **inside** an otherwise valid private snapshot. Its
  post-write verification then failed because the snapshot had an unexpected
  seventh file, leaving a previously usable backup invalid. Pair creation now
  refuses that directory or any resolving alias before writing; a regression
  revalidates the unmodified snapshot after rejection.
- **S3-F2 — Session countdown:** the React clock was captured before the
  asynchronous session inspection. Rounding up a fresh 600-second lifetime
  could therefore display **11 min** instead of 10 min, and wall-clock reversal
  could make the countdown grow. The display now uses a monotonic elapsed-time
  clock and updates the deadline and display clock together. Browser coverage
  exercises a delayed session response.

The Apache rule set, service sandbox and ADC/ADCS restart regressions were
reviewed for Stage III scope. No further confirmed code defect was identified
in those components during this pass. The final SHA and full GitHub-hosted
Ubuntu 22.04/24.04/26.04 CI outcome must be recorded in Issue #234 before
marking Fix Pass 3 complete. Local checks are not substitutes for that gate.
