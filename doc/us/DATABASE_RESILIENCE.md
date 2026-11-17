# Database Resilience — alpha-0.7.13

This increment starts directly from main `eb6b44e2` (the alpha-0.7.12 integration).
It retains schema v6, account data, password formats and the existing ADC/ADCS
startup authentication directory. The complete base SHA is recorded in CHANGELOG.md.

## Timeouts

All database connections use the local Unix socket. The existing `[database]`
`timeout` setting remains the default for connect/read/write. Optional independent
overrides are accepted in the same section:

```ini
[database]
enabled = on
socket = /run/mysqld/mysqld.sock
name = dc24h
user = dc24h
password_file = /etc/dc24h/database.password
timeout = 5
connect_timeout = 3
read_timeout = 5
write_timeout = 5
```

Each explicit INI value must be an integer from 1 through 60 seconds; duplicates,
zero, negative and unknown values are rejected, including in disabled mode.
Omitting an override inherits `timeout`. In the C++ `Options` structure only,
an override of zero means inheritance. Configuration validation never opens the
password file or contacts MariaDB.

These are Connector/C connection and packet-I/O timeouts, not a total query,
transaction or shutdown deadline. Statement cleanup and large multi-packet results
can involve multiple I/O operations. Trusted query shapes and server lock limits
remain necessary. A timeout can have the same native code as connection loss;
callers must not infer that SQL was canceled or rolled back.

## Explicit connection recovery

`Connection::reconnect()` runs synchronously on the constructing thread. It closes
the previous session, performs exactly one fresh connection attempt using the
original copied settings, and reads the private password file again. Permissions,
ownership and secret validation are rechecked. No application plaintext password
is retained for recovery. Connector/C may retain internal credentials until close.

A successful reconnect reinstates `utf8mb4`, autocommit, local-socket transport,
disabled LOCAL INFILE, and disabled Connector/C automatic reconnect. Temporary
tables, advisory locks, session variables and manual session settings are discarded.
Repositories referencing the same `Connection` can make new deliberate operations.
Old Transaction weak links expire; they cannot finish a later transaction.

A failed reconnect leaves the old connection retired. Later `query`, `execute` and
`ping` calls fail locally. Another explicit `reconnect()` is possible after fixing
the cause. Credential errors are redacted; allocation failures remain
`std::bad_alloc`. Wrong-thread reconnect fails before changing the session.

Reconnect is refused while an active `Transaction` scope remains, including a
rollback-only or transport-retired scope. Roll back or destroy that scope first.
A failed COMMIT attempt finishes its scope, but its error can have
`outcome_unknown=true`. Closing a scope on the wrong thread requests retirement;
subsequent owner-thread recovery discards that old session safely. Completed,
moved-from and expired scopes cannot commit or roll back new work.

```cpp
try {
    auto tx = connection.transaction();
    // Trusted application writes.
    tx.commit();
} catch (const dc24h::database::Error& error) {
    if (error.kind() != dc24h::database::ErrorKind::connection) {
        throw;
    }
    // The transaction scope has unwound. Record/reconcile any uncertain result
    // through application-specific business keys before deciding on new work.
    connection.reconnect(); // One attempt; this does not replay the transaction.
}
```

There is no automatic SQL, transaction, DDL or migration replay, retry loop,
background reconnect, connection pool or database work on the Network reactor.
Conflicts and constraints retain their existing typed error behavior. Broken
read/write/compression transport codes retire the connection. Fixed diagnostics
contain no SQL, parameters, passwords, private paths or raw server messages.

## Serving behavior during outages

Enabled startup connects/pings and loads the checked authentication directory
before opening listeners. An unavailable database fails startup without network
listeners. A serving hub uses its immutable in-memory account snapshot, so a
later SQL outage does not block the reactor, disconnect ready users or change
roles. New logins use the same loaded snapshot. Account/credential changes and
revocations still require the documented stopped-hub maintenance and restart.

The authenticated API `database.enabled` flag describes startup configuration;
it is not a current health report. This increment adds no background SQL health
probe or live authentication reload. Operator CLI database actions perform fresh
connections and fail with bounded, redacted diagnostics during an outage. Recovery
of MariaDB does not silently refresh the serving hub's directory.

## Verification

`database.resilience` checks timeout option bounds. `database.resilience.process`
uses only a disposable local MariaDB datadir and socket. It covers killed idle and
transactional sessions, uncertain COMMIT, refusal with open scopes, stale scope
isolation, session reset, lock release, wrong-thread cleanup, changed/unsafe
password files, allocation-failure recovery, legacy and overridden read timeouts,
a stalled large write, a committed transaction with its acknowledgement deliberately dropped,
an unanswered Unix-socket handshake, failed recovery while the database is down,
and successful recovery after a real MariaDB stop/start. The serving hub remains
responsive to API and ADC during that outage. Existing database/ncdc suites verify
real guest and password-authenticated login, reconnect and chat over ADC and ADCS.
Tests never modify the host database or run production migrations.

[Connector/C timeout and reconnect options](https://mariadb.com/docs/connectors/mariadb-connector-c/api-functions/mysql_optionsv)
define the underlying socket behavior. [Database Core](DATABASE_CORE.md),
[Database configuration](DATABASE.md), [Testing](TESTING.md) and
[CI](GITHUB_CI.md) describe the retained contracts.
