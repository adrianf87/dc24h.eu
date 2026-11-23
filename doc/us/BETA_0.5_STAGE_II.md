# Beta 0.5.5–0.5.8 — Stage II security architecture and validation

Work unit 2 of [Issue #234](https://github.com/adrianf87/dc24h.eu-high-starter/issues/234). Development remains on the single `beta-0.5.x-stage-full` branch and draft PR #235. No public deployment, remote hub-control write route, database migration, self-hosted runner, Debian test or production data is authorized by this work unit.

## 0.5.5 — Trust boundaries and threat-model review

| Boundary | Authority and assets | Threat and mandatory control |
| --- | --- | --- |
| Browser → Apache TLS → local Node gateway | Secure, HttpOnly/SameSite cookie; CSRF challenge; WebAuthn RP ID/Origin | Reject spoofed Origin/Host/SNI, proxy headers, session fixation, unknown routes and unauthorized writes. The HTTP gateway is bound to loopback only. |
| Node gateway → private C++ Unix socket | C++ owns password checks, account grants and the session authority | A password alone never creates a remote browser session. Every session read or credential mutation rechecks current C++ permissions and fails closed on denial/outage. |
| Node WebAuthn/TOTP code → private catalog | Passkey public keys, user handles, counters, backup flags, TOTP seeds and replay ledger | Enforce credential-to-account binding, cryptographic RP/Origin/UP/UV, one-use challenges and counters. Never send seeds or backend grant tokens to a browser. |
| Gateway → private state filesystem | Five files: `mfa.json`, `replay.jsonl`, `audit.jsonl`, `passkeys.json`, `rate-limits.json` | A missing file after recovery must not silently reset replay protection, audit history, revoked keys or abuse budgets. Require explicit offline initialization before first start. |
| Gateway → journal/alerts | Immutable event vocabulary, bounded nonidentifying alert reasons | Never include actor, cookie, credential, IP or private response. Detect audit corruption, bound alert frequency and handle clock reversal without indefinite alert suppression. |
| Offline recovery → MariaDB/private snapshot | Schema v11, credentials, grants, five private files and matched integrity manifest | Restore as a single isolated unit, compare schema and hashes, preserve replay/rollback state; no live import or production-capacity inference. |

The C++ service remains the **only** authority for accounts, passwords and grants. Browser login requires **password + TOTP** or **password + passkey**. Passkey attestation is `none`; this does not provide hardware provenance. Authentication is not passwordless.

## Review findings and patches

| Finding | Risk | Disposition |
| --- | --- | --- |
| S2-01: noncanonical credential IDs/keys or unexpected catalog fields can be loaded into private passkey state | Ambiguous identifier interpretation at a cryptographic trust boundary | 0.5.6: tighten catalog validation and negative tests; no migration of actual credentials. |
| S2-02: a missing TOTP replay file can be silently recreated on verifier startup | A previously consumed code may be accepted again after loss of the ledger | 0.5.7: fail closed on missing ledger; explicit offline initialization and loss/restart tests. |
| S2-03: a missing audit, passkey or rate-limit file can be silently bootstrapped by the local gateway | Loss of history, revocations or throttling after incomplete recovery | 0.5.8: require deliberate offline initialization and refuse missing runtime files; tests cover restart/recovery. |
| S2-04: the security-alert cooldown depends on wall-clock order | A backward clock jump can suppress subsequent alerts for an unintended period | 0.5.8: reset cooldown safely on time reversal and test bounded privacy-only output. |

Repository checks include npm production audit, C++ ASan/UBSan, WebAdmin unit/browser/integration tests, private Unix-socket API contracts and actual Ubuntu systemd checks. Any new confirmed weakness discovered during this stage must be fixed and tested before the stage is marked complete.

## 0.5.6 — Passkey hardening contract

- RP ID and Origin are explicit fixed configuration, never inferred from proxy identity headers.
- Registration and assertion require user presence and verification, a one-time expiring challenge, exact account ownership and a valid C++ password-based grant.
- Stored credential IDs, user handles and public-key bytes must use canonical unpadded base64url encoding. Reject duplicate identifiers, invalid shape, counter rollback, unexpected backup flags and malformed storage; do not try to repair or reinterpret them automatically.
- Enrollment, device rename and revocation recheck the current browser session, CSRF and C++ permissions. Changes invalidate or rotate affected sessions; no browser receives a TOTP secret or private passkey material.

## 0.5.7–0.5.8 — Session, recovery, audit and alert operation

- A valid replay ledger is mandatory before loading TOTP. Recovery preserves its high-water consumed steps; replacement and compaction are offline-only. A missing file never means an empty new history.
- An existing passkey, audit or rate-limit catalog is mandatory for normal gateway startup. After a legitimate **first-time** installation, provision empty catalogs using an explicit offline tool only while the service is stopped. Do not use an initialization option to recover a missing, previously active catalog: restore the matched backup instead.
- An audit-chain validation or write failure invalidates sessions. The SHA-256 chain detects accidental damage and nonprivileged edits; it is not a keyed attestation against a privileged attacker. Retention is bounded (8 MiB); filling the store denies authenticated service until offline maintenance and preservation of evidence.
- Alerts are fixed-code, throttled journal records without request data or identities. Test repeated denials, audit loss, corruption, storage loss and backward time jumps.

## Validation and release boundary

Required test hosts are **GitHub-hosted Ubuntu 22.04, 24.04 and 26.04 only**. After the final Stage II commit, wait for all head-SHA C++ Debug/Release, WebAdmin, ASan/UBSan, security, browser, systemd and recovery jobs to finish successfully. Record exact source SHA, workflow run URLs, failure disposition and outstanding risks on Issue #234. No separate external security assessment or Owner approval is a gate. This stage is not a public deployment decision.

**Status:** pending implementation of 0.5.6–0.5.8 and latest-head CI.
