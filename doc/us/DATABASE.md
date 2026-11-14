# MariaDB — alpha-0.7.1

**Current increment — alpha-0.7.1:** [Database Core](DATABASE_CORE.md) adds typed prepared parameters, bounded owning query results, execution metadata, owner-thread enforcement, redacted errors and RAII transactions. The immutable migration-1 baseline and operator setup below remain; dependent native components require rebuilding.

Implemented: optional Connector/C connection, RAII cleanup, bounded timeouts, UTF-8 (`utf8mb4`), prepared statements and an explicit migration-metadata baseline. Accounts, authentication/password hashing, roles, permissions, configuration persistence, bans, administrative records and event persistence remain planned. No hub user passwords are stored yet; a future authentication module must use an audited Argon2id implementation. WebAdmin will use the C++ API rather than direct SQL.

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

Provision an existing database and dedicated account with your database administration tools. Do not use SQL root as the service user. Use `SELECT` on the selected schema for the runtime account (database selection requires schema access); explicit migrations additionally require `CREATE` and `INSERT` on the selected schema. Keep separate operator credentials when adding future migrations. No database/users/grants are created automatically.

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

## Explicit migration baseline

```sh
./build/dc24h --config config/dc24h.ini --check-config
./build/dc24h --config config/dc24h.ini --check-database
./build/dc24h --config config/dc24h.ini --migrate-database
```

Check and migration commands connect but do not start Core, bind hub ports or open the configured log. They exit with code 0 on success, 1 on database failure, 2 on invalid arguments. These three actions are mutually exclusive; database actions require enabled mode. Check only pings; ordinary startup never applies schema changes.

Migration 1 creates `dc24h_schema_migrations` (InnoDB) with unsigned integer `version` primary key, ASCII `name`, ASCII SHA-256 `checksum`, and `applied_at` timestamp. The SQL source is `database/migrations/0001_schema_migrations.sql`; CMake embeds its exact bytes and SHA-256, tracks changes and installs the SQL under `share/dc24h/migrations`. Runtime does not load arbitrary files or split SQL on semicolons. The migration name is `schema_migrations`.

The runner obtains a zero-wait, connection-scoped `GET_LOCK` named `dc24h:` plus the first 52 hex digits of SHA-256 of the database name. This serializes cooperating dc24h migration runners on the same server/schema. Lock acquisition failure aborts; RAII releases it, including on errors. Baseline creation is idempotent and records version 1 only after successful DDL. Repeated runs validate the version/name/checksum and report `applied=0`; unknown versions, extra records or altered checksums fail closed. Do not edit an applied migration. This check validates recorded migration identity; it is not a complete schema drift audit.

MariaDB DDL can implicitly commit; no transactional rollback is promised. The baseline is retryable if interrupted between table creation and metadata insertion. Future non-idempotent migrations must record/recover partial failure explicitly. Back up the schema and review privileges/SQL before migrations; the runner provides no downgrade or backup automation. References: [Connector/C options](https://mariadb.com/docs/connectors/mariadb-connector-c/api-functions/mysql_optionsv), [MariaDB DDL implicit commits](https://mariadb.com/docs/server/reference/sql-statements/transactions/sql-statements-that-cause-an-implicit-commit).

## Adapter contract

`Connection` is noncopyable, synchronous and owned by one thread. Connector/C handles stay private. Queries use prepared statements for bound values; never concatenate untrusted identifiers or SQL. `execute()` accepts non-result statements; `scalar()` requires exactly one row/column, with a maximum 4096-byte value and `nullopt` for SQL NULL. SQL and individual bound values are capped at 64 KiB. No multi-statement execution or LOCAL INFILE is enabled. Closing each statement drains its resources even on result/prepare errors. There is no generic unbounded row-fetch API.
