# MariaDB persistence in Alpha 0.1

Install the database server and the client development package on Debian or Ubuntu.

```sh
sudo apt-get install mariadb-server mariadb-client libmariadb-dev
```

The hub connects through a local Unix socket. On a MariaDB instance dedicated to the hub, add the following to `/etc/mysql/mariadb.conf.d/60-dc24h-local.cnf` and restart MariaDB to disable its TCP listener.

```ini
[mariadbd]
skip-networking
```

## Database and users

Use a DBA session to create the database and three local accounts. Replace the placeholders with distinct generated passwords. You can start the local DBA client with `sudo env MYSQL_HISTFILE=/dev/null mariadb --protocol=socket` to keep the provisioning statements out of client history.

```sql
CREATE DATABASE dc24h CHARACTER SET utf8mb4;
CREATE USER 'dc24h_migrate'@'localhost' IDENTIFIED BY '<migration password>';
CREATE USER 'dc24h_hub'@'localhost' IDENTIFIED BY '<hub password>';
CREATE USER 'dc24h_admin'@'localhost' IDENTIFIED BY '<operator password>';
GRANT CREATE, SELECT, INSERT ON dc24h.* TO 'dc24h_migrate'@'localhost';
```

Set the connection variables for the migration account in your shell. Point `DC24H_DB_PASSWORD_FILE` at a regular file readable only by its owner, such as mode `0600` or `0400`. Write one password to that file. Set `DC24H_DB_SOCKET` when the MariaDB socket differs from `/run/mysqld/mysqld.sock`.

```sh
export DC24H_DB_NAME=dc24h
export DC24H_DB_USER=dc24h_migrate
export DC24H_DB_PASSWORD_FILE=/path/to/private/migration-password
./build/debug/dc24h-hub --db migrate
./build/debug/dc24h-hub --db status
```

After applying the migrations, grant the daemon read access and grant the operator access for administrative commands. The migration account's `INSERT` can now be limited to the migration history table.

```sql
REVOKE INSERT ON dc24h.* FROM 'dc24h_migrate'@'localhost';
GRANT INSERT ON dc24h.schema_migrations TO 'dc24h_migrate'@'localhost';
GRANT SELECT ON dc24h.schema_migrations TO 'dc24h_hub'@'localhost', 'dc24h_admin'@'localhost';
GRANT SELECT ON dc24h.hub_settings TO 'dc24h_hub'@'localhost';
GRANT SELECT ON dc24h.bans TO 'dc24h_hub'@'localhost';
GRANT SELECT, INSERT, UPDATE ON dc24h.hub_settings TO 'dc24h_admin'@'localhost';
GRANT SELECT, INSERT, DELETE ON dc24h.bans TO 'dc24h_admin'@'localhost';
GRANT SELECT, INSERT ON dc24h.audit_events TO 'dc24h_admin'@'localhost';
```

Run the hub under a dedicated, unprivileged OS account with read access to the hub password file and its TLS files. For example, after provisioning an OS account named `dc24h`, use the following command from a directory it can access.

```sh
sudo -u dc24h -- env \
  DC24H_DB_NAME=dc24h DC24H_DB_USER=dc24h_hub \
  DC24H_DB_PASSWORD_FILE=/etc/dc24h/secrets/hub-db \
  /usr/local/bin/dc24h-hub --serve --mariadb \
  --tls-cert /etc/dc24h/tls/fullchain.pem --tls-key /etc/dc24h/tls/key.pem
```

## Migrations

Run `--db migrate` with the migration account when deploying or upgrading, then verify `--db status`. E5 introduced version 3. The current E6a schema is version 5. A database-scoped advisory lock serializes schema changes. Each table creation is repeatable and the runner checks that the table uses InnoDB and exposes the expected columns before recording the version. MariaDB commits DDL implicitly, so the runner can verify a table left by an interruption and then record its version. Applied migrations preserve existing rows. Unknown or inconsistent migration history requires operator review. [Admin API migrations](ADMIN-API.md), [MariaDB advisory locks](https://mariadb.com/docs/server/reference/sql-functions/secondary-functions/miscellaneous-functions/get_lock), [MariaDB implicit commits](https://mariadb.com/docs/server/reference/sql-statements/transactions/sql-statements-that-cause-an-implicit-commit)

| Table | Contents |
| --- | --- |
| `schema_migrations` | Version, name, and UTC application time |
| `hub_settings` | Setting name, value, and UTC update time |
| `bans` | ID, CID or IP, reason, UTC creation and optional expiration time, and optional actor |
| `audit_events` | ID, event type, UTC time, optional actor and object ID, and fixed details |

## Settings

Run administrative commands using `DC24H_DB_USER=dc24h_admin` and its private password file.

```sh
./build/debug/dc24h-hub --db settings set hub-name 'Example hub' --actor operator.1
./build/debug/dc24h-hub --db settings set hub-description 'Local ADC and ADCS'
./build/debug/dc24h-hub --db settings set max-clients 256
./build/debug/dc24h-hub --db settings list
```

With `--serve --mariadb`, the hub reads settings once before it starts the network workers. Explicit serve options take precedence over stored values. An absent or invalid entry uses the original default unless a flag overrides it. Listener addresses, ports, and the TLS file paths are configured through the existing serve options.

| Setting key and corresponding serve flag | Accepted values |
| --- | --- |
| `hub-name` | 1–128 bytes of NFC UTF-8 text |
| `hub-description` | 0–255 bytes of NFC UTF-8 text |
| `max-clients`, `input-bytes`, `pending-bytes` | Positive integers up to 1048575 clients or 1048576 bytes |
| `max-nick-codepoints`, `max-message-bytes` | Positive integers up to 256 code points or 4096 bytes |
| `setup-timeout-ms`, `tls-timeout-ms`, `identify-timeout-ms`, `idle-timeout-ms`, `shutdown-timeout-ms` | 1–86400000 milliseconds |

Setting changes take effect after a hub restart. Without `--mariadb`, the hub uses its serve options and defaults and skips persistent ban checks.

## Bans and audit

```sh
./build/debug/dc24h-hub --db bans add ip 192.0.2.25 3600 'Repeated spam' --actor operator.1
./build/debug/dc24h-hub --db bans add ip 2001:db8::10 permanent 'Abuse'
./build/debug/dc24h-hub --db bans list
./build/debug/dc24h-hub --db bans remove 1 --actor operator.1
./build/debug/dc24h-hub --db audit list
```

Use `cid` for a canonical ADC CID or `ip` for a single numeric IPv4 or IPv6 address. IPv4-mapped IPv6 addresses match their IPv4 forms. Give the duration in positive seconds or use `permanent`. An expired ban remains visible in the list and stops matching new logins. Ban and audit lists return up to 100 rows. Pass the last numeric ID as `AFTER_ID` to get the next page.

Both listeners check the verified CID and the peer socket address before adding a user to the roster. A permanent ban takes precedence over a timed ban. Timed bans report the remaining seconds to the client. Ban changes apply to new logins, so connected users stay online until their session ends. [ADC protocol](https://adc.sourceforge.io/ADC.html)

Successful changes and their audit events commit in one transaction. The event stores a setting name or a ban ID and fixed details. Admin failures detected by the store are recorded when the database is writable. Use a nonsecret identifier of up to 64 ASCII letters, digits, dots, hyphens, underscores, or at signs for an optional actor. Configuration values, ban reasons, client session data, and credentials stay out of audit details.

## Backup and restore

Use a backup account with `SELECT` on every table in the database, including `admin_users` and `admin_sessions` on E6a. The client prompts for its password. Store dumps in a private location because they include identifiers, ban reasons, and password hashes. Schedule schema upgrades outside the dump window.

```sh
umask 077
mariadb-dump --protocol=socket --user=dc24h_backup --password \
  --single-transaction --skip-lock-tables --no-tablespaces --skip-triggers \
  --skip-add-drop-table --skip-add-locks --skip-disable-keys \
  dc24h > dc24h-backup.sql
```

Create a fresh database with `CREATE DATABASE dc24h_restore CHARACTER SET utf8mb4;` in a DBA session. Give a temporary restore account `CREATE` and `INSERT` on that database and import the dump.

```sh
mariadb --protocol=socket --user=dc24h_restore --password dc24h_restore < dc24h-backup.sql
```

Provision the table-level grants for the restored database, run `--db status`, and inspect the restored settings, bans, and audit before pointing the hub at it. Run migrations if the dump is from an earlier schema. Remove the restore account after verification.

## Troubleshooting

- For connection or access errors, verify that MariaDB is listening on the configured socket and that the selected account has the grants for its command. The hub reports operation names and numeric error codes.
- On a missing or incomplete schema, run `--db migrate` with the migration account and retry. Resolve an incompatible existing table before rerunning a failed step.
- If the database settings snapshot fails at startup, the hub uses explicit options and defaults for that process. Restart after recovery to load the saved settings.
- An enabled hub with an unavailable database, timed-out checks, or a full ban-check queue declines new logins with a temporary hub error. Established users can continue using ADC and ADCS. A new login can reconnect to MariaDB after a brief retry delay. The SQL worker has one connection and a queue of 32 checks. Connector I/O has two-second timeouts and MariaDB applies one-second statement and lock-wait limits.
- If the connection drops during an administrative commit, check the relevant list and audit entries before retrying.

To run the isolated integration suite, install `openssl` and use `cmake --preset debug -DDC24H_TEST_MARIADB=ON`, `cmake --build --preset debug`, and `ctest --preset debug -L mariadb`. The suite starts its own MariaDB instance in a temporary directory.
