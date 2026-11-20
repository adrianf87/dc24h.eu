# Identity Mapping — alpha-0.7.6

**Current extension (alpha-0.7.7):** [Password Security](PASSWORD_SECURITY.md) adds credential checks. Mapping alone and a matching password alone still do not grant a session or account permissions. Historical future-work statements below describe the original alpha-0.7.6 increment.

This increment starts directly from main `d279245e3636d631c159ee8df53623c293fb36e8`.
Database Core, migrations, account metadata, repository and registration remain
intact. The branch is not merged to main automatically.

## Contract

Link `dc24h_identity_mapping` and include
`dc24h/accounts/identity_mapping.hpp`. `IdentityRepository` borrows a Database
Core connection on its owning thread. Its constructor does not contact SQL.
All operations are synchronous and belong outside the Network reactor.

This is a trusted management API. The caller must authorize binding an identity
to an account. It has no ADC command, HTTP route, WebAdmin action or CLI that
accepts untrusted binding requests. A syntactically valid CID, even one whose
PID relationship was verified, does not authenticate an account. Resolution
returns metadata; it never changes session identity, account status, role,
last-login time or permissions. Existing guest ADC/ADCS behavior is unchanged.

| Operation | Result |
| --- | --- |
| `IdentityKey::create(nickname, cid)` | Owning checked pair or `nullopt`. |
| `bind(account_id, key, created_at)` | Inserts an immutable mapping in one owned transaction; returns an owning `StoredIdentity` after commit. |
| `find_by_account(id)` | Optional mapping for the persistent account ID. |
| `find_by_nickname(nickname)` | Optional exact-byte nickname lookup; invalid input is refused. |
| `find_by_cid(TigerCid)` | Optional canonical CID lookup. |
| `resolve(key)` | Optional mapping only when **both** nickname and CID match the same row. No nickname-only or CID-only fallback. |

One account has at most one mapping. Nickname and CID are each unique across
accounts. Repeating a bind, including identical input, reports a conflict;
there is no overwrite, transfer, implicit account creation or silent takeover.
Replacement, revocation and multiple identities per account are future work
requiring an explicit conflict and authorization policy.

The nickname is distinct from the account's private registration name. It is
nonempty, already-NFC UTF-8, at most 64 bytes, without the controls, format
characters, line/paragraph separators or ASCII space excluded by the existing
ADC public-text policy. Bytes and case are preserved: `Alice` and `alice` are
different identities. No trimming, normalization or case folding occurs.
CID parsing reuses `adc::TigerCid`: 39 uppercase base32 characters representing
exactly 192 bits, including canonical unused bits. PID, SID, address, verifier
and password are neither inputs nor stored fields. Moved-from keys are refused.
Creation time is an explicit UTC Unix second in 0–253402300799; there is no
implicit clock or account-login inference.

## Storage and failures

Explicit migration **v5** creates InnoDB `dc24h_account_identities`:
`account_id BIGINT` primary key, `nickname VARBINARY(64)`, `cid VARBINARY(39)`
and `created_at BIGINT`. Full unique nickname/CID indexes and a restrictive
foreign key to accounts enforce ownership. Checks bound lengths, CID shape,
positive account IDs and timestamps. Migrations 1–4 remain byte-for-byte intact.
Existing accounts and credentials are preserved; no identities are guessed
from old account names or automatically assigned during upgrade.

Back up, stop writers and use operator credentials for the existing explicit
`--migrate-database` command. A clean v4 database applies one migration and
reports `version=5 latest=5 pending=0 dirty=no`. Fresh databases apply five.
Ordinary startup and repository calls never migrate. Durable applying/applied
journaling remains in force: an existing table or partial DDL produces a dirty
history that requires operator investigation, not automatic replay.

Operations require a current clean catalog and compatible account and mapping
storage. Mapping checks include engine, exact columns, full unique indexes and
the referenced account key. These are
bounded compatibility checks, not a full audit of SQL triggers, CHECK expressions,
defaults, referential actions, administrator changes or concurrent DDL. Read-only
inspection does not require broad privileges to expose referential-action metadata. Keep DDL and migration
credentials separate. Runtime mapping reads need SELECT on the involved tables;
binding also needs INSERT on the mapping table. No UPDATE, DELETE, schema
administration or credential-table access is needed by the mapping API itself.

Binding locks the existing account row and inserts with prepared parameters
inside a rollback-on-exit transaction. It refuses a missing account and never
publishes a partial result. The owning result is allocated before mutation and
moves without allocation after commit. Nested caller transactions and use from
another thread are refused. Database Core errors preserve numeric diagnostics
and `outcome_unknown`; reconcile through a new connection after a lost
acknowledgement rather than retrying a write blindly.

`MappingErrorKind` distinguishes invalid identity/time, missing account,
conflict, invalid stored data and incompatible storage. Diagnostics use fixed
EN/US text without nickname, CID or SQL. Existing account-schema, migration and
Database Core errors can propagate. SQL-imported rows are validated on reads.

## Verification and limits

`accounts.identity_mapping` covers nickname/CID boundaries, Unicode, canonical
padding, exact case and moved-from values. `accounts.identity_mapping.process`
uses a private MariaDB with networking disabled. It checks persistence, exact
pair matching, cross-pair rejection, duplicates, absent accounts, timestamp
limits, unchanged account metadata, restricted privileges, thread/transaction
ownership, racing binds, schema/index/foreign-key drift, corrupt stored CID and
allocation-failure rollback through a successful insert. Migration tests cover
v4-to-v5 preservation and dirty-history refusal. Existing ncdc ADC/ADCS,
MariaDB, Apache and WebAdmin tests remain in Debug/Release.
See [Testing](TESTING.md) and
[Changelog](../../CHANGELOG.md) for recorded results; launched CI is not a pass.

Password verification, authenticated session binding, identity revocation,
registered admission, role grants and account UI/API remain separate stages.
The broader Debian/Ubuntu support matrix is a target; this change's remote CI
uses Ubuntu 24.04 only. No production database is migrated or hub deployed.

References: MariaDB [binary strings](https://mariadb.com/docs/server/reference/data-types/string-data-types/varbinary)
and [foreign keys](https://mariadb.com/docs/server/ha-and-performance/optimization-and-tuning/optimization-and-indexes/foreign-keys).
