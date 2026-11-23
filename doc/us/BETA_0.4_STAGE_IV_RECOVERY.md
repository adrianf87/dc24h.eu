# Beta 0.4.15 — private disaster-recovery rehearsal (Stage IV)

This procedure is a **controlled rehearsal plan**, not evidence that a
production recovery, Debian 13 failover or RPO/RTO target has been met.
No target server or production database is accessed by this release branch.

## Preconditions and consistent backup boundary

1. Obtain maintenance authorization, a second independent operator, approved
   downtime and a disposable Debian 13 pilot with test accounts and a separate
   MariaDB instance. Record the initial commit, config schema, C++ binary and
   WebAdmin version, MariaDB and Apache2 versions, and the preflight time.
2. Revoke active client connections, stop the **local gateway** and pause hub
   database writes. Confirm both services have actually stopped; setting
   `--ack-stopped` is an operator declaration, not a process detector. Never
   snapshot a hot replay ledger, passkey store or HMAC rate-limit catalog.
3. Use `mariadb-dump` with operator-provided `0600` credentials and a private
   **Unix socket**, transaction consistency and reviewed options. Validate the
   exit status, resulting file, table counts and dump timestamp. Do **not**
   restore a dump that contains `CREATE DATABASE`/`DROP DATABASE` into the
   running production server. Use a dedicated, disposable restore instance.
4. Snapshot the five gateway files together: TOTP enrollment, TOTP replay,
   audit, passkeys, and rate limits. Encrypted, access-restricted off-host
   archival and retention are operator responsibilities; the rehearsal tool
   never uploads or exposes credentials.

Example on a stopped **isolated** pilot (private parent directories must be
owner-owned `0700`; the script refuses symlinks and non-private files):

```sh
python3 scripts/stage4_private_recovery_drill.py snapshot --source /var/lib/dc24h-remote --destination /var/lib/dc24h-rehearsals/snapshot-001 --ack-stopped --ack-isolated
python3 scripts/stage4_private_recovery_drill.py verify --source /var/lib/dc24h-rehearsals/snapshot-001 --ack-isolated
python3 scripts/stage4_private_recovery_drill.py restore --source /var/lib/dc24h-rehearsals/snapshot-001 --destination /var/lib/dc24h-rehearsals/restored-001 --ack-stopped --ack-isolated
```

The target directories must not exist beforehand. `restore` writes only into
a **new** private directory and never overwrites live files or starts a
service. The manifest contains only file sizes, hashes and an archive time;
the tool checks every source hash again after preflight, rejects changes since
manifest verification, and checks all five newly written files against the
original manifest before reporting success. It also validates the private
gateway's MFA, replay, passkey, rate-limit and audit formats. A replaced,
truncated or corrupt ledger is rejected. **A failed operation may leave an
incomplete destination; never install it.** The restored directory contains
only the five service files, not a manifest. The backup and restored data
remain confidential and should not enter CI artifacts or Git.

## Failures to rehearse without losing live data

1. **MariaDB stopped/unavailable:** deny affected hub/admin operations;
   capture the outage timestamp, negative checks, restart and read-only
   validation. The gateway must not use cached authority if its C++ API loses
   its database-backed grant. Do not use a production DB for fault injection.
2. **C++/Apache2/gateway restarts:** check the correct listener binding,
   read-only fixed routes, valid TLS hostname/chain and expected refusal of
   old session cookies and CSRF after a gateway restart. Confirm the session
   is created only after fresh password + TOTP/passkey verification.
3. **Missing/corrupt audit, replay or passkey file:** the gateway must fail
   closed. Copy the corrupt original for forensic review; do not truncate,
   synthesize a new chain or delete an audit record to make startup pass.
   Restore the **matched** set of five files from one verified snapshot only
   after approval, and separately verify all C++ account/role grants.
4. **Restore rehearsal:** import the MariaDB dump into a fresh disposable
   instance under a different database/service identity, validate migration
   schema **v11**, row counts and credentials without printing secrets, then
   verify restored private files with their native C++/Node parsers before
   serving requests. Run authorization, denied replay and session-invalidation
   tests, then destroy only the disposable rehearsal instance.
5. **Rollback:** stop the pilot, restore the prior pinned executable/config
   **and its matching** database/private-state snapshot. No Stage IV schema
   migration is introduced. Re-run startup, MariaDB migration, WebAuthn and
   audit-chain checks; reopen traffic only by a separate manual decision.

## Evidence template (complete on target, not in GitHub CI)

| Item | Required observation | Current status |
|---|---|---|
| Last consistent backup / recovery point | Timestamp and checksum of both MariaDB + private bundle | **NOT MEASURED** |
| MariaDB outage → read-only restored | First error, recovery time, grant recheck | **NOT MEASURED** |
| C++/Apache2/gateway restart | Exit/startup durations, listener and cookie invalidation | **NOT MEASURED** |
| Corrupt audit and replay denial | Failure logs and rejected requests without secrets | Offline fixtures passed; **target pending** |
| Fresh pilot database + private bundle restore | Full data integrity and read-only security checks | **NOT MEASURED** |
| RPO | Difference between incident time and newest verified, consistent recovery point | **NOT MEASURED** |
| RTO | Time from incident/service loss to independently validated safe recovery | **NOT MEASURED** |

Every deviation must enter the final risk register. A Python unit test using
temporary private directories proves the **tool's** negative and positive
paths; it does not prove MariaDB backup consistency, service recovery or
target-host RPO/RTO. Those require actual operator-run evidence.
