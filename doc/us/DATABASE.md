# MariaDB (planned)

No SQL driver, database connection or executable migration is included in alpha-0.1.7. `database/migrations/` reserves the migration location. Core startup does not depend on MariaDB.

The planned schema covers user accounts/password hashes, roles and permissions, server configuration, bans/blocks, selected event logs and administrative records. Migration versions will be ordered and recorded; applied migrations must remain immutable. Backups and MariaDB DDL transaction behavior must be considered before applying schema changes.

The future adapter must own connections through RAII, parameterize queries, use a minimally privileged service account and keep credentials outside source/logs. Password hashing is planned through an audited Argon2id implementation; no password storage or authentication is currently provided. The WebAdmin will access the secured C++ API, never SQL directly.
