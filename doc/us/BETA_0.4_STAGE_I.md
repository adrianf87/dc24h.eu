# Beta 0.4.1–0.4.4 — Stage I: security foundations

Branch: `beta-0.4.1-0.4.4-stage1`, based on the merged beta-0.3.12 `main`.
The final Stage I runtime identifier is `beta-0.4.4`. This patch is **not**
a production deployment, a WebAuthn implementation, or permission to expose
the remote portal to the Internet. The public Apache vhost and MariaDB schema
v11 remain unchanged. Merge only after successful CI and operator review.

## beta-0.4.1 — CI & Stability

- The preceding Alpha 0.1 operational fixes were merged as PR #184; the
  complete C++/WebAdmin CI workflow #573 passed before starting this stage.
- Retain the three hosted Ubuntu matrices, Debug/Release, sanitizer, browser,
  Python and Node checks. Add this branch to push triggers; PRs to `main`
  continue to run the required `CI (C++ and WebAdmin)` gate.
- Keep the baseline and release-gate source checks, plus add deterministic MFA
  rotation, replay, audit-corruption, file-permission and fail-closed tests.
  **The new branch must pass its own CI**; historical success is not enough.

## beta-0.4.2 — Security Architecture

Trust boundaries: operator/browser → explicitly configured local HTTPS/SSH
pilot → loopback Apache → loopback Node remote gateway → private C++ Unix API.
Five fixed `/remote/v1/` routes are read-only. Password and TOTP cannot
authorize C++ mutations or service management. No public binding, wildcard
proxy, authentication downgrade, new SQL migration, or automatic exposure.

For later **passkeys/WebAuthn**: choose a stable RP ID and HTTPS origin;
validate the origin, RP ID hash, challenge, authenticator signature, user
presence and policy-required user verification on the server. Bind each
credential to an authorized account and retain counter/backup flags, device
revocation and recovery records. Do not equate a successful browser ceremony
with authorization; revalidate active role and grants at every private read.
Passkey attestation and multi-device sync policy require an explicit operator
decision. No WebAuthn registration, login route, or JavaScript ceremony is
implemented in Stage I. TOTP alone is not phishing-resistant.

Threat cases to test before any Internet pilot: credential phishing, account
enumeration, TOTP replay, concurrent rotation, stale in-memory sessions, CSRF,
proxy-header spoofing, untrusted origins, stolen SSH key, compromised operator
host, storage rollback, exhausted disk, lost journal records, backups restoring
old OTP secrets and compromised server root. A privileged host attacker can
rewrite a hash chain; an independent off-host checkpoint is still required.

## beta-0.4.3 — MFA Hardening

The remote gateway continues to use account-bound, six-digit TOTP in owner-only
files and an append/fsync replay ledger. The offline maintenance helper
`webadmin/server/remote-mfa-tool.ts` replaces exactly one enrolled seed using
an atomic same-directory rename plus file/directory fsync. It **never clears
or rewinds the replay ledger**. Existing verifiers detect a changed enrollment
file and refuse logins until restart. A used old step cannot be reused with
the replacement seed; wait for the next 30-second step if required.

Operator procedure (all paths absolute and private, directory 0700/files 0600):

1. Verify the person and active administrator/Owner account out of band. Stop
   the dedicated remote gateway; close its SSH pilot and revoke browser
   sessions by restarting. Back up the enrollment and replay ledger together
   into protected storage; never restore an older ledger with the same seed.
2. Generate a **fresh, unique** 160-bit Base32 seed in a protected operator
   environment, enroll it in the authenticator, and prove a fresh six-digit
   code. Do not put seeds, passwords or codes in command arguments, shell
   history, Git, journal output or world-readable files.
3. Prepare a private JSON request file with these fixed keys (placeholder
   values below; use a protected editor, never a pasted production secret):

   ```json
   {"schema":1,"actor":"7","new_seed":"BASE32_SEED","new_code":"000000","old_code":"000000"}
   ```

   For ordinary rotation `old_code` must be a fresh code from the **current**
   authenticator. Run the helper as the service account while stopped:

   ```sh
   node --import tsx server/remote-mfa-tool.ts \
     --enrollment /var/lib/dc24h-remote/mfa.json \
     --replay /var/lib/dc24h-remote/replay.jsonl \
     --request /private/operator/rotation.json --offline-ack
   ```

4. For a genuinely lost factor, require a documented, independent identity
   check and two-person operator approval. Include `schema`, `actor`,
   `new_seed`, `new_code` and `"approvers":["operator-one","operator-two"]`
   in the private request (omit `old_code`) and add `--recovery`. Distinct
   operator names are acknowledgments, **not cryptographic proof of approval**.
   There is no browser recovery or OTP bypass. Record the event in the
   operator's separate incident log, review account passwords/grants, and
   restart the gateway.
5. Verify that the old code fails, the new code works after any consumed step,
   local-only origin restrictions still apply, and no sessions survived the
   restart. Remove the request file under the operator's data-removal policy.

The replay ledger has a 1 MiB hard limit. During the same stopped-service
maintenance window, after protecting its original copy, an operator may
compact it to the maximum consumed step for every actor, **without resetting
replay protection**:

```sh
node --import tsx server/remote-mfa-tool.ts \
  --replay /var/lib/dc24h-remote/replay.jsonl --compact-replay --offline-ack
```

Compaction replaces the ledger inode and deliberately invalidates every
cached verifier until restart. Do not use it on a running service.

Never rotate enrollment during service operation or share a recovery seed
through untrusted chat. Neither this helper nor the local trust boundary can
prove an operator actually followed the independent verification process.
Request paths must be canonical and absolute; no intermediate path component
may be a symlink. The helper refuses unsafe ownership/permissions, hard links
and request-file replacement during reading.

## beta-0.4.4 — Audit Foundation

- `DC24H_REMOTE_AUDIT_FILE` is now required for the optional remote gateway.
  It must reside in a trusted owner-only directory. Files are 0600, one link,
  no symlink; the gateway refuses startup on malformed, truncated, tampered,
  oversized (8 MiB) or unsafe history.
- Every fixed non-personal event is appended synchronously and `fsync`ed as
  a numbered JSONL record with the previous SHA-256 digest. A restart checks
  the entire bounded chain. Event payloads exclude account IDs, IP addresses,
  passwords, codes, sessions, raw requests and secrets.
- A sink failure increments an in-memory dropped-event counter, prints the
  fixed `AUDIT_LOSS` warning to stderr and makes the local gateway refuse
  new sessions. Existing browser sessions are invalidated on subsequent
  requests. No success response can be issued when the required auth event
  could not be stored; journald is a secondary sanitized mirror.
- This is a **local integrity and loss-detection foundation**, not a signed
  remote audit system or compliance guarantee. A power failure, privileged
  host compromise, tail truncation plus replacement, journal deletion or an
  unobserved failure before alert delivery still need external detection.

**Retention and recovery policy:** operator must archive the entire chain
to protected off-host storage and verify digest/sequence continuity before
replacing a file (the running gateway refuses inode changes). Suggested
operational starting target: 90 days available and 12 months restricted
archives, subject to local privacy/legal review; no retention purge is
automated. Monitor file size before 8 MiB, `AUDIT_LOSS` signals and service
status. On failure stop the pilot, preserve evidence, investigate disk and
permissions, restore a verified complete archive or establish a documented
new audit epoch, then restart under approval. Never silently truncate.

## Release gate and rollback

Run C++ CTest/ASan/UBSan, Python project/baseline/release checks, TypeScript,
Node gateway tests, Chromium e2e, browser integration and dependency audit on
the exact final commit. Verify systemd and HTTPS on a disposable test server.
Do not auto-merge or deploy. If checks fail, keep `main` unchanged; do not
discard the MFA replay ledger or audit evidence when reverting binaries.

References: NIST SP 800-63B (authenticator binding, recovery and one-time
OTP replay), W3C WebAuthn Level 3 (RP-origin verification and signatures).
