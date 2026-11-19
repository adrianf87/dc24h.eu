# Database security — alpha-0.9.8

MariaDB failure handling and backup. The branch starts from main `2a2737b`. The branch name is `alpha-0.9.8`.

The hub already keeps one Unix-socket session, retires it when an acknowledgement is lost, and reconnects only when the owner asks. This increment adds one explicit backup. It is not a schedule, a replica, or an INI key.

## Snapshot

`--backup-database FILE` connects with the existing settings, pings, and takes the migration advisory lock. The schema must already be clean version 9. The command then starts `START TRANSACTION WITH CONSISTENT SNAPSHOT`, reads the seven catalog tables, and rolls the snapshot back. It does not commit and it does not change rows.

The file is created with `O_EXCL` and `O_NOFOLLOW`, mode `0600`, owned by the hub user, with one link. A symlink, an existing path, or a mode other than `0600` is refused. If the snapshot does not finish, including a lost rollback acknowledgement, the partial file is removed. A crash can still leave a partial file; restore rejects it because the SHA-256 trailer no longer matches.

The file holds the table rows, including password verifiers and sealed ADC secrets. It does not hold the MariaDB password, the socket path, or server error text. Treat the file like the password file. Standard output reports only `Database backup written bytes=` and the length.

Each table is limited to 4096 rows. The file is limited to 32 MiB. A cell is limited to 65536 bytes. A larger database is refused and no file is kept. The backup does not reconnect and does not replay SQL.

## Restore

`--restore-database FILE` reads that private file and checks the trailer before any write. The live schema must be the same clean version 9, and the migration rows must match the file. Every application table must be empty. Matching migration history is not rewritten.

The rows are inserted in one ordinary transaction with bound parameters. The file is not executed as SQL. A lost commit acknowledgement stays `outcome_unknown` and is not replayed; the operator inspects the tables before trying again. Scope exit rolls back any restore that fails before commit.

## Unchanged

There is no scheduled backup, no off-host copy, and no replica. MariaDB datadir permissions stay with the operator. This does not close residual threat T14. Schema v9 is unchanged. No status counter, WebAdmin page, INI key, or Apache setting is added. Startup still does not migrate. `Require local` stays.

`database.process` checks a clean snapshot, owner-only mode, refusal of a symlink, an existing path, a group-readable file and a damaged trailer, a binary credential round trip, and refusal while the schema is not current or the database is disabled.
