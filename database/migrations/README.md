# MariaDB migrations

`0001_schema_migrations.sql` is the immutable alpha-0.1.8 migration metadata baseline.
CMake embeds the exact SQL and SHA-256 in the database library and installs the SQL
under `share/dc24h/migrations`. Run only through the explicit
`dc24h --config FILE --migrate-database` action; normal startup never migrates.

The baseline creates the migration registry only. Application tables and future
partial-failure recovery are not implemented. Never edit an applied migration.
See [English documentation](../../doc/us/DATABASE.md) and
[Polish documentation](../../doc/pl/DATABASE.md).
