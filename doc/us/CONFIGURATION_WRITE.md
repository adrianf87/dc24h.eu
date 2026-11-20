# Atomic configuration storage — beta-0.1.3

This development increment extends the **separate local maintenance process**, not the serving ADC hub. Stop the hub before starting `dc24h-account-admin --config /absolute/private/hub.ini`. Apply database migration v10 through the explicit maintenance/migration CLI before starting this process; back up the database first. The INI remains the source of startup configuration. Neither Apache nor the browser adapter forwards write/restore routes.

## API and authorization

All requests use the existing local Unix socket and require the process bearer token, short-lived administrator grant, matching session and CSRF secret. The C++ access gate rechecks `configuration.manage` (Administrator or Owner) for each request. The actor ID comes from the verified grant, never from a client field.

- `GET /api/v1/management/configuration`: read saved and maintenance-startup settings plus an opaque revision.
- `POST /api/v1/management/configuration`: validate an in-memory proposal (no write).
- `POST /api/v1/management/configuration/write`: send `revision` and one to four of `network_max_connections`, `network_idle_timeout`, `logging_level`, `general_language` as an URL-encoded form. A successful response has `written:true`, `restart_required:true`, a new `revision`, and `backup_available:true`.
- `POST /api/v1/management/configuration/restore`: send exactly `revision` to restore the server-owned sidecar backup. The current INI is preserved in a private undo copy before replacement.

No endpoint accepts a filename, path, system command, arbitrary INI key or secret. Bad forms are rejected, invalid proposed values are `422`, stale revisions are `409`, and ambiguous file outcomes return `503 outcome_unknown`. Never retry an ambiguous write blindly; reread and inspect the INI, backup and audit first. The old `management/authorize` discovery contract remains non-executing; the WebAdmin editor is planned for beta-0.1.4.

## Filesystem safety and recovery

Only a canonical, fixed INI path from process startup is used. The parent directory and source must belong to the effective service UID and must not be group/world writable. The source must be a regular non-hardlinked file. The writer rejects symlinks and unsafe existing sidecars. A fixed owner-only advisory `.dc24h.lock` serializes cooperative writers; a content-bound revision and inode/metadata recheck detect stale edits. External processes that ignore the lock must be coordinated separately.

Before each write, the complete original INI is copied and durably synced to `<ini>.dc24h.bak` (mode 0600). The validated updated bytes are written to a private same-directory temporary file, synced, atomically renamed over the INI and followed by a directory sync. Existing comments and keys outside the four allowlisted settings are retained. Restoration validates the backup, saves the pre-restore bytes to `<ini>.dc24h.undo` (0600) and replaces the primary atomically. No live settings change without a controlled hub restart.

If a write reports `outcome_unknown` or the process crashes, stop further writes, inspect and validate the live INI and both sidecars as the service owner, inspect the audit row and restart only after recovery. Backup rotation is intentionally one-generation in this phase; arrange independent operator backups before upgrades.

## Database and audit

New migration `0010_configuration_audit.sql` extends the existing v9 audit table constraints without modifying applied v9 SQL or account/ban rows. An authorized change creates a durable `configuration.write` or `configuration.restore` event with `target_kind=configuration`, `target_id=1`, `outcome=started` before touching files. The result is updated to `succeeded` or `failed`. A crash/unknown acknowledgement intentionally leaves `started` for operator review. Audit never stores file contents, tokens, keys, passwords or proposed values. Database backups made on schema v9 remain restorable into an empty v10 application schema after verified migration-prefix matching.

## Scope limits

This is not remote administration, a WebAdmin editor, a live hub configuration snapshot, a hot reload or a service manager. The existing owner-controlled service lifecycle remains separate. This is a development beta; do not merge until all required CodeQL and C++/WebAdmin checks pass.
