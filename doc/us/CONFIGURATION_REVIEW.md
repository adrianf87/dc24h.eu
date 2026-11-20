# Configuration review — beta-0.1.2

`chatgpt-beta-0.1.2` adds read-only inspection and in-memory validation to the **separate local maintenance process** (`dc24h-account-admin --config ...`). It does not run a second hub, expose a web editor, change the existing ADC service, or write a file. Configuration operations require the existing `configuration.manage` permission (Administrator or Owner) **and** the process bearer token, a live administrator grant, matching session id, and CSRF secret.

## Exact local routes

`GET /api/v1/management/configuration` returns schema 1 with `read_only: true`, `restart_required: true`, an opaque `revision`, an allowlisted `saved` snapshot (reparsed from the INI for each request), and `active` (the maintenance process's startup values). `matches_active` compares those four fields. `active_scope` is `maintenance_process` and `hub_active_available` is **false**: a stopped or separate hub's effective running values cannot be inferred from a disk file. In particular, the response must not be described as the running hub's configuration.

Only these non-secret settings are returned, and only these can be proposed:

| Form/JSON field | INI field | Accepted values |
|---|---|---|
| `network_max_connections` | `[network] max_connections` | Integer 1–65535 |
| `network_idle_timeout` | `[network] idle_timeout` | Integer 1–86400 seconds |
| `logging_level` | `[logging] level` | `info`, `warning`, `error`, `off` |
| `general_language` | `[general] language` | `us`, `pl` |

`POST /api/v1/management/configuration` accepts `application/x-www-form-urlencoded` with `revision=<64 lowercase hex digits>` and one to four distinct allowlisted fields. It loads the saved file again, compares its content-bound revision, changes a **temporary in-memory** Settings object, and runs the existing whole-configuration checks. On success it returns `{ "valid": true, "written": false, "restart_required": true }`. An invalid candidate returns `valid: false` without exposing an INI value or parser diagnostics. Nothing is persisted. Use URL encoding for the form and no path or file field.

The revision is HMAC-SHA256 of the complete file with an ephemeral random process key. It is opaque; it is not a reusable authentication token and changes after maintenance process restart. When another edit changes the file, POST returns `409` with `stale`; clients must reread and review the new snapshot. Even if the file is repaired into a valid state, an older revision is refused. Duplicate or unknown fields, malformed revisions and oversized requests are rejected. Read errors, unsafe symlinks and invalid saved files fail closed with a generic unavailable result. The existing INI parser allows no includes or environment expansion and enforces a 64 KiB ceiling.

The endpoint is intentionally **not** allowlisted through Apache or the WebAdmin loopback adapter in this phase. It can only be called through the already protected local Unix socket; `GET /api/v1/status` remains a separate bearer-only route. The legacy `POST /api/v1/management/authorize` discovery contract still answers `501` for listed operations, and the five-area catalog still marks full configuration management unimplemented. Neither this endpoint nor the WebAdmin page provides file writes, backups, rollback, a configuration editor or systemd operations. Those are later beta stages.

No SQL migration or audit row is added; schema v9 remains. Tests cover snapshots, changed-file conflicts, denied values, malformed saved files and symlink rejection. This is a development beta and not a production configuration editor.
