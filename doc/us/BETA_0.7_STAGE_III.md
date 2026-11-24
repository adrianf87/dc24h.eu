# Beta 0.7.9–0.7.12 — Stage III: paired backups, recovery preflight, disposable .deb and rollback

Scope: Stage III of [#238](https://github.com/adrianf87/dc24h.eu-high-starter/issues/238), on `beta-0.7.x-stage-full` in the same draft PR #239. This is **disposable-fixture evidence**, not approval for production deployment. Never install the CI package on an operator host or run this restore against an active service/database.

## 0.7.9 — one stopped-writer consistency point

1. Stop **all** MariaDB and WebAdmin/private-state writers. A MariaDB read lock alone cannot stop file writers.
2. Hold the disposable MariaDB read lock over both the SQL dump and the snapshot of all five private stores: `mfa.json`, `replay.jsonl`, `audit.jsonl`, `passkeys.json`, `rate-limits.json`.
3. Use private owner-only files and directories, refuse symlinks and incomplete copies. Create the private snapshot through `scripts/stage4_private_recovery_drill.py snapshot` in a fresh destination.
4. Use `scripts/stage1_recovery_bundle.py pair` with the **exact source commit SHA**. The `recovery-pair.json` file binds the SQL export hash and private manifest hash; the private manifest binds the five stores. Keep all six sources immutable after pairing.

Operators must independently establish stopped writers: hashes alone cannot prove that the SQL and private files were captured from one generation. Never upload the SQL or private data as CI artifacts.

## 0.7.10 — read-only refusal-first preflight

On isolated, new, absent destinations, run:

```sh
python3 scripts/beta07_recovery_preflight.py \
  --mariadb-dump /secure/offline/state.sql \
  --private-snapshot /secure/offline/snapshot \
  --pair-directory /secure/offline/pair \
  --commit-sha EXACT_40_HEX_SOURCE_SHA \
  --fresh-sql-destination /secure/rehearsal/new-mariadb-data \
  --fresh-private-destination /secure/rehearsal/new-webadmin-state \
  --ack-isolated
```

Preflight **does not import SQL or restore files**. It rejects the wrong SHA, mismatched hashes, missing/extra files, malformed MFA/passkey/replay/rate-limit formats, broken audit chain, missing applied schema-v11 migration rows, unsafe ownership/modes/links, a changed SQL input, and existing or overlapping destinations. Diagnostics exclude SQL contents, secrets, accounts and absolute paths. `schema_v11_verified_after_import=false` is intentional: only a subsequent fresh MariaDB import plus the C++ `--database-status` check can establish the restored schema.

Restore only into *new disposable* SQL and private-state destinations, using `stage4_private_recovery_drill.py restore` for the private stores. Before any service is made accessible, check C++ account authority/grants, schema v11, passkey revocation and epochs, replay, audit integrity and rate-limit state. On any refusal, leave existing destinations untouched; never synthesize empty replacement files.

## 0.7.11–0.7.12 — package transitions and recovery

- `scripts/beta07_ci_deb.py` builds a private **test-only** `dc24h-high-starter-ci-fixture` package for `amd64`. It owns the C++ executable, source/binary provenance, and an inert systemd unit guarded by a path that must not exist on the runner. Normal fixtures have no maintainer scripts or service enable/start steps, and contain no production configuration.
- As of Stage V, package versions `0.5.16+ci1` and `0.7.20+ci1` identify the *actual historical/current test binaries*, not a deployable production package. The special `0.7.20+ci2` is a valid but intentionally failing package whose `preinst` exits 41 **before unpacking**; this does not claim coverage of every possible post-install or migration failure. When Stage III was initially implemented, the candidate binary still reported beta-0.6.16.
- An ephemeral GitHub-hosted Ubuntu 22.04/24.04/26.04 runner installs the previous package, upgrades to current, rejects corrupt archive input, injects a genuine pre-unpack `dpkg` failure, verifies the installed binary and dpkg status survived, then executes an actual downgrade. After the transition, the run preflights and restores the matched SQL v11/five-file bundle to new destinations, validates C++ credential/grant state, revocation, audit and replay, and rechecks after a database restart. The fixture package is purged.
- This does **not** certify live production packaging, availability during an upgrade, arbitrary post-install failures or a production RPO/RTO guarantee. Those require a separate approved operational rehearsal.

## Acceptance boundary

Stage III requires green CI on the **exact final commit** for all three GitHub-hosted Ubuntu profiles. Per the OWNER instruction, **stop after pushing and triggering CI; do not wait for results**. Pending CI is not acceptance. Keep the PR in draft; do not merge, deploy or touch production services.
