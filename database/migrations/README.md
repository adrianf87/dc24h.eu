# MariaDB migrations

Schema v6 adds separate encrypted ADC credentials; SQL 1–5 remains unchanged. See [ADC Authentication](../../doc/us/ADC_AUTHENTICATION.md) for the external-key requirement and explicit provisioning.

`manifest.json` declares the ordered SQL catalog. Migration 1 remains the
immutable alpha-0.1.8 metadata baseline; migration 2 adds the migration state
column; migration 3 adds checked account metadata storage and migration 4 adds
separate registration credential storage. CMake validates and embeds exact LF SQL/SHA-256 and installs the manifest
and listed SQL under `share/dc24h/migrations`. Each file is one prepared statement;
multiple files run in manifest order without splitting on semicolons.

Use `dc24h --config FILE --database-status` for read-only version inspection and
`--migrate-database` for explicit upgrades; normal startup never migrates. All
versions 3+ record applying before SQL and refuse replay after partial failure.
Only the two idempotent bootstrap steps are resumable automatically. Never edit
an applied migration. The account and credential tables are application schema. No down migration or automated
repair is supplied. See [English](../../doc/us/DATABASE_MIGRATIONS.md) and
[Polish](../../doc/pl/DATABASE_MIGRATIONS.md) for extension/recovery and privileges.

Schema v5 adds immutable account/nickname/CID bindings in `0005_account_identities.sql`. Existing migrations 1–4 are unchanged. Upgrade explicitly after backup; no identities are inferred from account names. See [Identity Mapping](../../doc/us/IDENTITY_MAPPING.md).

Migration v7 widens the stored account-role CHECK to six persistent roles. Guest is session-only. Migration SQL 1–6 remains immutable; see [Permissions](../../doc/us/PERMISSIONS.md).

alpha-0.8.5 adds explicit v8 `0008_bans.sql` for account/CID/exact-IP blocks. Earlier SQL files 1–7 are unchanged. See [Ban Core](../../doc/us/BAN_CORE.md) ([PL](../../doc/pl/BAN_CORE.md)).
