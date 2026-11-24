# beta-0.9.x — Stage III (0.9.9–0.9.12): historical auth, dirfd, recovery, systemd

Status: **implemented for review; hosted CI not yet observed or accepted**.
Owner issue: [#242](https://github.com/adrianf87/dc24h.eu-high-starter/issues/242).
Development branch `beta-0.9.x-stage-full`; existing draft PR #243 to protected
`main`. This is **one Stage III work unit**, not Stage IV, a merge or a release.

## 0.9.9 — Installed historical maintenance authority, not a linked stub

The GitHub-hosted Ubuntu VM matrix (22.04/24.04/26.04) now independently
builds `dc24h` **and** `dc24h-account-admin` from the immutable beta-0.5.16
source. Both binaries enter the uniquely named, disposable dpkg test package.
The transition is a real old install → candidate install → rejected pre-unpack
upgrade → failed *post-unpack* configuration → **old package downgrade**.
After downgrade the fixture verifies `dpkg-query`, installed executable bytes,
historical `--version`, unit ownership and nonactivation. It restores a fresh
MariaDB v11 database and five matching owner-private Node files from an offline
HMAC-signed six-component pair with a one-use anti-replay ledger.

**Required additional end-to-end proof:** while the previous package is still
installed, a *second independent disposable matched v11/five-store restoration*
runs `tests/beta06_authenticated_api_process_tests.py` using the **installed
`/usr/lib/dc24h-high-starter-ci-fixture/dc24h-account-admin`** for the real
password proof. The fixture exercises the full Node/Apache HTTPS/real Chromium
TOTP and virtual-WebAuthn passkey flow, session grants, post-restore fresh login,
revocation and refusal after cold restart. The current C++ hub only seeds and
serves read-only status; it never substitutes for old password authority.
Historical protocol incompatibility, timeout or denied credentials must make
the job **red**; it is not a permitted silent skip. The two matched restorations
are separate fixtures—do not imply that the first dpkg SQL dump becomes the
second browser fixture's dump.

## 0.9.10 — Bounded Linux descriptor-relative private files

The Node private catalog paths (MFA seed and replay, passkeys, audit, rate
budgets and integrity) use `withPinnedPrivateParent` plus descriptor-relative
`/proc/self/fd/<dirfd>/<leaf>` opens, `O_DIRECTORY`, `O_NOFOLLOW`, repeated
directory-inode checks, owner-only mode and same-parent renames. A concurrent
ancestor rename or symlink replacement fails closed without following an
attacker's replacement pathname; the deterministic regression actually swaps
the ancestor during a pinned open. This is a Linux-specific **procfs dirfd
technique, not a claimed portable Node `openat2` binding or global protection
against hostile same-UID/process-level filesystem mutation**. Existing file
inode, ownership, nlink, size and digest checks remain part of the boundary.
If `/proc`, the directory flags or the kernel permissions cannot support it,
startup/use must refuse. Keep private directories `0700`, files `0600`.

## 0.9.11 — Matching six-component offline recovery and single-use trust

The migration v11 SQL reader now streams through the already-pinned Linux
directory FD and verifies the SQL file identity *after* parsing. Deterministic
tests cover a six-component matching manifest, fresh empty target, detached
HMAC signature, independent trusted key, one-use ledger, torn replay store,
wrong generation and a **byte-identical SQL rename while scanning**. No SQL
target is created by a preflight: only the hosted disposable MariaDB imports
the database; the private restore writes a new destination. Offline recovery
still requires stopped writers, protected key/ledger and independent
verification. Do not claim that unauthenticated private on-disk files are
tamper-proof before the authenticated recovery decision.

## 0.9.12 — Actual local-only systemd and dpkg rehearsal

The installed historic C++ `dc24h-account-admin` is launched through a
**transient systemd system service**, as the unprivileged runner, against the
fresh disposable SQL instance and a Unix-only maintenance API; its restricted
AF_UNIX address family and `PrivateNetwork=yes` prohibit public listeners. The
fixture checks effective sandbox properties, actual start, **restart**, live
API response and stop. Separately, the compiled Node gateway with **only
production npm dependencies** runs as a transient unprivileged systemd unit
with private test catalogs, a private environment file, hardening properties,
start/restart/stop and a strictly `127.0.0.1` HTTP listener check. The normal
fixture dpkg unit remains inert and never enabled. All transient units are
stopped/reset on failure; no production unit is installed, enabled or changed.

## Evidence and strict operator boundary

- GitHub VM workflow `.github/workflows/stage4-ubuntu-vm-validation.yml`
  requires all existing regressions plus new Python recovery/gate cases,
  TypeScript dirfd unit cases and real Node/C++ systemd rehearsals.
- `beta07-dpkg-rollback.json` attests installed old maintenance binary,
  historical authentication, independent recovery and real C++ systemd proof.
  `beta09-systemd-node.json` is written only after the Node systemd service
  successfully starts, restarts, proves loopback-only and stops.
- `beta09-stage3-gate.json` binds those *executed* aggregate inputs to the
  exact source SHA and supported Ubuntu image. Missing/wrong-head/partial
  evidence refuses; never upload secrets, cookies, raw credentials or dumps.
- **At handoff: local focused tests pass; hosted CI remains pending.** Even a
  successful CI run does not authorize merge, release, public exposure,
  production dpkg/systemd changes or a production rollback. Owner must review
  the final exact-head Ubuntu matrix and explicitly authorize the next step.

## Fix-pass follow-up — MariaDB preflight, private restore and service isolation

- The read-only SQL preflight now checks **all 11 migration names and exact
  SQL SHA-256 identities** reconstructed from the trusted source manifest,
  not just the number of rows and their `applied` states. This detects an
  incompatible-but-self-consistently-hashed backup before any fresh MariaDB
  import. The real C++/MariaDB post-import schema inspection is still required.
- The five-store recovery validator now applies the signed 64-bit C++ account
  ID bound to TOTP enrollment and replay history as well as passkeys. An
  unusable account ID cannot be certified as a successful recoverable backup.
- The transient installed-historical C++ systemd test now verifies the
  **effective** Unix-only address-family and additional kernel/home hardening
  properties. The compiled Node gateway systemd test additionally checks
  its effective address-family/private-device/kernel settings and rejects
  any matching **IPv6 listener**, not only public IPv4 binds.
- New negative regressions run on the GitHub-hosted Ubuntu 22.04/24.04/26.04
  matrix. These are disposable tests: no production MariaDB, schema v11,
  installed unit, listener or recovery key/checkpoint is changed. Do not
  equate a passed offline preflight with a completed production restore or
  Owner approval for the separately gated runtime authenticity migration.

