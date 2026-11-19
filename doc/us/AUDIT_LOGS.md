# Audit logs — alpha-0.8.9

This increment records successful administrative maintenance. It starts from main `e88e596`. There is no automatic merge. The branch name is `alpha-0.8.9`.

Schema v9 adds the append-only table `dc24h_audit_events` in `database/migrations/0009_audit_events.sql`. Migrations 1–8 stay unchanged. Apply the upgrade with the explicit database migration command after a backup. Ordinary startup does not migrate. Account and ban maintenance require the current catalog, so those writes wait until version 9 is applied.

Each committed maintenance action inserts one row in the same SQL transaction, before commit. A failed insert rolls the action back. The stored facts are the event id, UTC `occurred_at`, a fixed action code, the actor account id, the target kind (`account` or `ban`), the target id, and the outcome `succeeded`.

The action codes are `accounts.create`, `accounts.activate`, `accounts.disable`, `accounts.password`, `accounts.reset`, `accounts.role`, `ownership.bootstrap`, `ownership.transfer`, `bans.create` and `bans.revoke`. Registration outside Admin Core, directory reloads and live kick or mute do not add rows. Refusals do not add rows. Passwords, proofs, account names and free-text ban reasons are not columns.

`audit.read` is an implemented permission. A moderator or higher role has it. A VIP account has it only when the VIP mask includes that key. `configuration.manage` and `hub.manage` remain unimplemented.

`POST /api/v1/audit` lists one page of 16 rows for a proven reader. The form fields are exactly `actor_id`, `proof` and `after`. `after` may be `0`. The account must be active, the password must match, and the role must allow `audit.read`. The read does not insert a row and does not change Admin Core counters. `GET /api/v1/audit` is rejected. A full page sets `next_after` to the last id. A shorter page sets `next_after` to null. Identifiers in JSON are strings. Times are numbers. The private maintenance adapter accepts the same POST at `/admin-api/v1/audit` and removes any extra fields before the browser sees the body.

WebAdmin shows the history at `#audit` in English and Polish. The page is read-only. The password stays in memory only long enough to request the next page, the field is cleared after submit, and the value is not written to web storage. Stop serving hubs before a maintenance write, then restart. Browsing the history does not require that restart by itself.

Live kick and mute remain session counters. Configuration writes and live hub administration remain pending.
