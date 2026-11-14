# Database Core — alpha-0.7.1

**Schema versioning — alpha-0.7.2:** [Database Migrations](DATABASE_MIGRATIONS.md) documents the ordered catalog, v1 upgrade, read-only status and partial-failure contract.

Based directly on main `85a2d155270ff7f218ec336a7013fc65a6471275` after alpha-0.6.13. The earlier optional socket connection and migration registry remain. This increment adds a reusable synchronous C++20 MariaDB core: typed prepared values, bounded owning results, write metadata, typed redacted errors and scoped transactions. Accounts, authentication, roles, bans and application repositories remain later stages; no application migration is introduced.

## Connection ownership and transport

Build with MariaDB Connector/C 3.x (`libmariadb-dev` on the supported Debian/Ubuntu systems). Connector headers/handles remain private to `src/database/connection.cpp`; public callers include `dc24h/database/database.hpp`. Configuration, private password handling and the immutable migration baseline remain in `database.cpp`.

`Connection` is noncopyable/nonmovable and records its constructing thread. Calls from another thread fail before touching Connector/C or owner-thread mutable state. Create separate connections inside future database worker threads and join those workers before process/static teardown. This stage supplies no worker pool, asynchronous SQL dispatch or SQL on the Network reactor. Normal enabled startup still connects/pings before opening listeners, retains the connection until shutdown and applies no migrations automatically.

A C++ function-local runtime serializes `mysql_library_init` before the module's first `mysql_init` calls and performs library cleanup at process shutdown. Each statement/connection owns and releases its backend resources. Modern Connector/C thread-init/end functions are deprecated; the module does not rely on them for ownership or synchronization.

Local Unix-socket transport, `utf8mb4`, private password-file checks, per-operation connect/read/write timeouts, disabled reconnect and disabled LOCAL INFILE are retained. No TCP/TLS database transport, host-config reading, credentials in arguments, schema/user creation or grant changes are added. The temporary application password is constructed directly into its cleansed buffer; Connector/C may keep its own copies until close. [Database configuration](DATABASE.md) remains authoritative for operator setup.

## Typed values and execution

```cpp
using namespace dc24h::database;
Connection connection(options); // enabled, pre-provisioned local database
const auto result = connection.query(
    "SELECT ? AS label, ? AS quantity, ? AS missing",
    {std::string("Zażółć 😺"), std::int64_t{-7}, nullptr});

// Future application-owned InnoDB table; this core does not create it.
auto transaction = connection.transaction();
const auto inserted = connection.execute_prepared(
    "INSERT INTO application_table(label, payload) VALUES (?, ?)",
    {std::string("example"), Bytes{0, 255, 0}});
transaction.commit();
```

`Parameter` owns one of SQL NULL (`nullptr`), signed/unsigned 64-bit integer, `std::string` text or `Bytes` binary data. Signedness is bound explicitly using Connector/C integer storage; the full int64/uint64 ranges are preserved. Empty text/blob, embedded NUL and SQL NULL are distinct. Data is not interpolated, normalized, escaped as SQL syntax or case-folded. Text uses the connection's `utf8mb4`; binary data has an explicit BLOB binding. Floating-point parameters and domain-specific decoding are deferred.

SQL is trusted application code, never an ADC/browser/client query language. Placeholders bind values, not identifiers. Select identifiers and SQL structure from trusted code; never concatenate untrusted input. No multi-statement execution is enabled. `execute_prepared` requires a non-result statement and returns statement-specific `Execution{affected_rows, insert_id}`. `query` returns one result set; extra result sets are rejected and remaining statement results canceled. Legacy `execute` and `scalar` retain their string-parameter signatures and shape checks; `scalar` still requires one row/column, at most 4096 bytes and `nullopt` for SQL NULL.

## Result and input bounds

| Resource | Limit |
| --- | --- |
| SQL | Nonempty, at most 65536 bytes, no embedded NUL |
| Parameters | At most 256; at most 65536 bytes per text/blob; 1048576 bound bytes in aggregate |
| Default result | 256 rows, 32 columns, 65536 bytes/cell, 1048576 cell-payload bytes total |
| Maximum `QueryLimits` | 1024 rows, 64 columns, 65536 bytes/cell, 4194304 payload bytes total |
| Column name | At most 1024 bytes; metadata is separately bounded by column count |

Rows/cell/total limits may be zero for empty-result or NULL/empty-value checks; columns must be 1..64. Invalid limits fail before preparing SQL. Integer parameter storage counts toward the bound-byte total. Result object overhead is bounded by the row/column ceilings in addition to payload and metadata limits.

`Result` owns column names and rows (`vector<optional<string>>`). Numeric results are Connector/C's textual representations; blob and text result bytes are retained exactly, including NUL. Results survive subsequent queries. Fetch first obtains each field's length, checks limits before allocating its owning string, then retrieves the exact bytes with `mysql_stmt_fetch_column`. It does not call `mysql_stmt_store_result` or expose a borrowed server cursor/row pointer. Reaching a limit throws a typed error and closes the statement without returning a partial result.

These bounds constrain returned C++ data and binding structures, not server execution plans, Connector/C packet buffers or a total wall-clock deadline. Use trusted bounded schema/query shapes and SQL `LIMIT` where appropriate; per-operation connector timeouts are not a query cancellation service. Never interpret a result/shape error as proof that SQL had no effect: an autocommitted `INSERT ... RETURNING` can finish before result validation fails. Use an explicit transaction when result checks must control write atomicity.

## Transaction contract

`Connection::transaction()` issues fixed `START TRANSACTION` and returns a move-constructible `Transaction`. Nested transactions are rejected before SQL. Explicit `commit`/`rollback` finish that scope; repeated calls and a completed old scope cannot commit or roll back a later scope. Destruction of an active scope rolls back, including during exception unwinding. A weak lifetime link makes a transaction safe to expire after its Connection: closing the connection already rolls back, and later commit/rollback fails locally.

After an SQL/result/allocation failure inside an active scope, the connection becomes rollback-only: further SQL and commit are refused until rollback. Lock timeouts (1205) and deadlocks (1213) are typed conflicts; no statement or transaction is retried automatically. Explicit rollback restores normal use if transport/state remains valid. Cleanup failure retires the connection. If a Transaction is destroyed on the wrong thread, it requests retirement atomically; the owner closes/rolls back before its next operation, avoiding foreign-thread SQL cleanup.

Only plain SELECT/INSERT/UPDATE/DELETE/REPLACE/WITH statements are accepted inside a scope, ignoring ASCII keyword case and leading SQL whitespace. Leading comments, DDL, SET, stored-procedure calls and manual transaction commands are rejected before prepare. This is an atomicity guard, not a general SQL parser or an untrusted-query sandbox. Use controlled InnoDB tables: nontransactional engines or external effects cannot be rolled back by this library. Migration operations are rejected inside a transaction before taking their advisory lock.

Server autocommit/transaction flags are checked through `mariadb_get_infov`. Unowned transactions or disabled autocommit caused by manual SQL retire the connection, including when discovered after a result error. Inside an active failed transaction, server-initiated rollback still requires the owner to finish its RAII scope. No reset, implicit reconnect, retry, savepoint or silent transaction replacement is provided.

## Errors and uncertain outcomes

`database::Error` carries `ErrorKind`, `Operation`, an optional numeric native code and `outcome_unknown`. Categories cover initialization, credentials, connection, statement, constraint, conflict, result shape, resource limits, transaction state and wrong-thread use. `what()` is fixed EN/US text with PL application translations; SQL, parameter values, credentials, paths and raw server error strings are never embedded. Allocation failures can still propagate as standard C++ exceptions.

Transport/protocol failures retire the connection after statement cleanup. Later operations fail locally and never reconnect or replay. A missing execute/fetch/commit acknowledgement is conservatively marked uncertain: a write may already have committed, and the core cannot resolve that from a broken connection. The flag concerns acknowledgement uncertainty, not a general guarantee of zero effects when false. Application-level idempotency/reconciliation and any deliberate new-connection retry belong to future services.

## Verification and remaining scope

`database.core` checks bounds and typed error metadata without a server. The disposable MariaDB harness retains `database.unit`/`database.process` and adds `database.core.process` plus `database.ncdc.process`. Real-server coverage includes concurrent connection initialization, Unicode/injection-shaped and binary/NUL values, integer extremes, row/column/cell/aggregate/input bounds, write metadata, owning results, commit/rollback/unwinding/moves/expired owners, nesting/DDL/control guards, read-only privileges, SQL/result errors, multiple results, lock timeout, deterministic deadlock, killed connections, retirement and uncertain commit reporting. A 64-position C++ allocation sweep verifies rollback-only behavior and recovery without affecting the backend password or unrelated work.

The database-enabled hub is additionally tested with real ncdc login/reconnect/chat over ADC and ADCS; existing protocol and frontend regression suites remain. Tests provision only private temporary datadirs and SQL temporary directories (bootstrap and runtime), Unix sockets, users, tables and a procedure, use `--no-defaults --skip-networking`, and remove them on completion. No host MariaDB configuration, account, schema or external runner is modified. Final local results are recorded in [Changelog](../../CHANGELOG.md); remote Check status is not awaited.

This stage adds no application/account schema, login authentication, password hashing, ORM/repository layer, pool/worker, database health monitor, WebAdmin write control, dependency or production deployment. The API `database.enabled` flag still describes startup configuration, not current SQL health. Rebuild dependent native components after the public C++ API/layout changes.

References: [parameter binding](https://mariadb.com/docs/connectors/mariadb-connector-c/api-prepared-statement-functions/mysql_stmt_bind_param), [column fetch](https://mariadb.com/docs/connectors/mariadb-connector-c/api-prepared-statement-functions/mysql_stmt_fetch_column), [statement cancellation/close](https://mariadb.com/docs/connectors/mariadb-connector-c/api-prepared-statement-functions/mysql_stmt_close), [server status flags](https://mariadb.com/docs/connectors/mariadb-connector-c/api-functions/mariadb_get_infov), [transactions](https://mariadb.com/docs/server/reference/sql-statements/transactions/start-transaction).
