# MariaDB migrations

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
