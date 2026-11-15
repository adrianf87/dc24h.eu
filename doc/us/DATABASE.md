# MariaDB — alpha-0.7.2

**Current increment — alpha-0.7.4:** [Account Repository](ACCOUNT_REPOSITORY.md) persists checked account metadata in MariaDB with generated IDs, exact unique names, revisions and transactional conflict detection. Migration v3 adds the account table; credentials, authenticated session binding and WebAdmin account writes remain planned. CI reuses the pinned Proxmox runner workflow from github-runner-v0.1 for disposable Ubuntu 24.04/22.04 clones; provisioning uses its restricted helper and tests run without sudo.

**Schema versioning — alpha-0.7.2:** [Database Migrations](DATABASE_MIGRATIONS.md) documents the ordered catalog, v1 upgrade, read-only status and partial-failure contract.

**Core delivered in alpha-0.7.1:** [Database Core](DATABASE_CORE.md) adds typed prepared parameters, bounded owning query results, execution metadata, owner-thread enforcement, redacted errors and RAII transactions. The immutable migration-1 baseline and operator setup below remain; dependent native components require rebuilding.

Implemented: optional Connector/C connection, RAII cleanup, bounded timeouts, UTF-8 (`utf8mb4`), prepared statements and an explicit migration-metadata baseline. Account metadata storage and role/status keys are implemented in [Account Repository](ACCOUNT_REPOSITORY.md). Authentication/password hashing, live role/permission mapping, configuration persistence, bans, administrative records and event persistence remain planned. No hub user passwords are stored yet; a future authentication module must use an audited Argon2id implementation. WebAdmin will use the C++ API rather than direct SQL.

## Build and configuration

Install `libmariadb-dev`; the Connector/C runtime is linked even when database support is disabled. A running MariaDB server is optional. All supported distributions provide Connector/C; only the locally tested environment is recorded in [TESTING.md](TESTING.md).

Database settings are INI-only, under `[database]`. The shipped example defaults to `enabled=off`; disabled mode never reads the password file, connects or migrates. When enabled, startup connects and pings before opening hub listeners; failure exits with code 1. The connection lives until graceful shutdown. There is no automatic reconnect, pooling, runtime health monitoring or database work in the Network reactor. Connect/read/write timeouts are individual Connector/C limits, not one total wall-clock deadline; DNS may take longer. Signals arriving during synchronous startup are handled after connection setup completes.

| Key | Default / policy |
| --- | --- |
| `enabled` | `off`; `on` requires `password_file` |
| `socket` | `/run/mysqld/mysqld.sock`; relative paths use the INI directory |
| `name` | `dc24h`; 1..64 ASCII letters/digits/underscores |
| `user` | `dc24h`; 1..128 bytes without control characters |
| `password_file` | no default; path relative to the INI directory |
| `timeout` | 5 seconds; 1..60 for each connect/read/write operation |

Unknown/duplicate keys and invalid combinations are rejected even in disabled mode. There is no password option in INI, command-line arguments, environment or source. `--check-config` validates syntax/settings and ADCS credentials, but does not read the database password, contact SQL, bind ports or open logs. Log destination aliases of the password are rejected alongside existing INI/ADCS path protections.

## Private credentials and transport

Provision an existing database and dedicated account with your database administration tools. Do not use SQL root as the service user. Use `SELECT` on the selected schema for the runtime account (database selection requires schema access); explicit migrations additionally require `CREATE`, `ALTER` and `INSERT` on the selected schema. Keep separate operator credentials when adding future migrations. No database/users/grants are created automatically.

Put the database password in a file owned by the process effective UID, with no group/other permission bits, one hard link and at most 4096 bytes (normally mode 0600 or 0400). Final-component symlinks, directories, FIFOs, empty files, NUL/control characters and oversized files are rejected. One terminal LF or CRLF is removed; other password bytes, including spaces, are preserved. Protect parent directories too. The read buffer and temporary application password are wiped after use; the connector may retain its own credential copies until the connection is destroyed. This is a database service password, not a hub user password.

Only local Unix sockets are supported in this increment; protocol selection is explicit, with no TCP fallback. Protect the socket and parent directories with OS permissions. TCP/TLS database support is deferred: Connector/C 3.4 changes hostname verification for local/private destinations, and strict portable verification needs a separately tested adapter. ADCS transport is independent. Diagnostics omit credentials, SQL, file paths and raw server error strings.

```ini
[database]
enabled = on
socket = /run/mysqld/mysqld.sock
name = dc24h
user = dc24h
password_file = /etc/dc24h/database.password
timeout = 5
```

An empty socket and TCP-related keys are rejected. Never expose a local database socket through an unauthenticated forwarding service.

## Explicit schema versioning

`--migrate-database` now applies the ordered compiled catalog through v3, preserving v1's exact checksum and adding the `state` metadata column and account table. `--database-status` reads the current/latest/pending/dirty state without DDL; ordinary startup never migrates. Current bootstrap/upgrade credentials additionally need ALTER. Future non-bootstrap migrations persist `applying` before SQL and refuse automatic replay after partial failure. The full contract, source manifest, checksum format, privilege and operator recovery guidance are in [Database Migrations](DATABASE_MIGRATIONS.md).

## Adapter contract

`Connection` is noncopyable, synchronous and owned by one thread. Connector/C handles stay private. Queries use prepared statements for bound values; never concatenate untrusted identifiers or SQL. `execute()` accepts non-result statements; `scalar()` requires exactly one row/column, with a maximum 4096-byte value and `nullopt` for SQL NULL. SQL and individual bound values are capped at 64 KiB. No multi-statement execution or LOCAL INFILE is enabled. Closing each statement drains its resources even on result/prepare errors. Bounded owning queries and typed values are described in [Database Core](DATABASE_CORE.md).
