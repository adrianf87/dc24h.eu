# Account Repository — alpha-0.7.4

Based directly on main `5947add6c5b6e318841c612c93b9cb5cf5eae061`. Existing alpha-0.7.1 Database Core, alpha-0.7.2 Migrations and alpha-0.7.3 Account Model are retained.

## Scope and API

The C++20 `dc24h_account_repository` library adapts the checked [Account Model](ACCOUNT_MODEL.md) to [Database Core](DATABASE_CORE.md). Public interface: `include/dc24h/accounts/repository.hpp`. Link that CMake target; it exports both underlying libraries. The repository borrows a connection that must outlive it, on the connection's owning thread. Calls are synchronous and belong outside the Network reactor. Construction performs no SQL. Every operation checks a current, clean migration history and expected account engine, columns and identity/name indexes. Missing/pending/dirty schemas are refused; nothing migrates implicitly. Metadata/history errors can also propagate as `database::MigrationError`.

| Operation | Contract |
| --- | --- |
| `insert(NewAccount)` | Validates name, role/status and explicit UTC times before SQL. MariaDB assigns a positive signed BIGINT ID; returns owning `StoredAccount {account, revision=1}` after commit. Defaults: registered, disabled, epoch creation/update, absent login. |
| `find_by_id(AccountId)` | Returns an owning optional stored snapshot; an absent row is `nullopt`. |
| `find_by_name(AccountName)` | Exact NFC UTF-8 **byte** match. `Alice` and `alice` can coexist. Confusable characters are separate names. Moved-from names are refused. |
| `update(expected, replacement)` | Locks and validates the current row, compares the complete expected snapshot and revision, and updates metadata with revision+1. Missing or changed rows report `stale`. Identity/creation time are immutable; update and existing last-login times cannot decrease, and existing login cannot be erased. No-op updates also advance revision. |

The model supports caller-supplied timestamps, including future values through year 9999; the repository does not read a clock or infer a successful login. Both read and write operations refuse an existing caller-owned transaction. Each write owns one RAII transaction. Return values are constructed before commit and move without allocation. Validation, duplicate, stale, decode or allocation failures before commit roll back on scope exit. IDs can have gaps after failures and are never connection/session identities.

`RepositoryErrorKind` distinguishes `invalid_account`, `duplicate`, `stale`, `invalid_record`, and `schema`, using fixed input-free messages. Duplicate key 1062 is mapped only for a known constraint failure. Other Database Core errors propagate, retaining `kind`, operation, numeric native code and `outcome_unknown`. A lost execution/commit acknowledgement may mean the write committed: reconnect, inspect and reconcile; never blindly retry an insert/update. Conflict handling requires a fresh read and an explicit caller decision.

## Schema and rollout

Migration `0003_accounts.sql` creates `dc24h_accounts` in InnoDB with auto-increment signed BIGINT primary key, full unique `VARBINARY(64)` name, binary stable role/status keys, signed BIGINT UTC-second times, nullable last-login and positive signed BIGINT revision. Binary names deliberately avoid locale, case and padding equivalence. SQL checks enforce name length, role/status membership, timestamp ranges/order and revision positivity; full UTF-8/NFC and ID validation also run in C++ when reading. CHECK on the AUTO_INCREMENT ID is not portable to MariaDB; generated IDs and imported IDs are checked in the adapter. No credentials are stored. A revision at 9223372036854775807 can be read but cannot advance.

Back up and stop writers before explicit migration with operator credentials:

```sh
./build/dc24h --config config/dc24h.ini --database-status
./build/dc24h --config config/dc24h.ini --migrate-database
./build/dc24h --config config/dc24h.ini --database-status
```

An empty schema applies 3 versions, v1 applies 2 and v2 applies 1. The clean latest schema reports `version=3 latest=3 pending=0 dirty=no`. Applied migration 1/2 bytes are unchanged. v3 records durable `applying` before DDL; any partial failure requires operator recovery, with no automatic replay or `CREATE IF NOT EXISTS` assumption. See [Database Migrations](DATABASE_MIGRATIONS.md).

Migration credentials require the existing SELECT/INSERT/UPDATE/CREATE/ALTER privileges; runtime repository credentials require SELECT/INSERT/UPDATE on the account table and SELECT for migration metadata. A read-only account can read but cannot write. Keep migration and runtime credentials separate; application tests need no sudo or committed password; clone provisioning uses the restricted helper. Direct administrator SQL, triggers, concurrent DDL, session-mode changes or deleted/reused identities bypass the adapter's contract. Schema checks are a bounded compatibility check, not a full audit of triggers, checks, defaults or external writes. Runtime accounts should have no such powers.

## Verification and remaining work

`accounts.repository.process` provisions a private MariaDB with networking disabled, then tests round trips, null/epoch/max timestamps, SQL-looking Unicode names, case-sensitive uniqueness, duplicates, revision/no-op conflicts, immutable metadata, independent connections and processes, concurrent writers, corrupt records, schema/index/engine drift, dirty history, read-only privileges and allocation-failure sweeps through successful insert/update. Existing migration, Core and real ncdc ADC/ADCS tests remain enabled. Run:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DDC24H_REQUIRE_INTEGRATION_TESTS=ON
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

Account registration, password hashing/verification, live authenticated account/session mapping, role permission grants, account API/WebAdmin writes, delete/list operations and connection pooling remain future increments. Persisting an active administrator account does not grant a guest session authority. No production service or host database is changed by the tests. Actual results are recorded in [Testing](TESTING.md) and [Changelog](../../CHANGELOG.md).

MariaDB references: [VARBINARY](https://mariadb.com/docs/server/reference/data-types/string-data-types/varbinary), [constraints](https://mariadb.com/docs/server/reference/sql-statements/data-definition/constraint), [affected rows](https://mariadb.com/docs/connectors/mariadb-connector-c/api-prepared-statement-functions/mysql_stmt_affected_rows), [DDL implicit commits](https://mariadb.com/docs/server/reference/sql-statements/transactions/sql-statements-that-cause-an-implicit-commit).
