# MariaDB — alpha-0.7.2

**alpha-0.9.12 Benchmark:** [Alpha 0.8 performance measurements](BENCHMARK.md) times a fixed in-process set of permission and moderation decisions. This is not a capacity claim or a load test. Schema v9 is unchanged.
**alpha-0.9.11 Monitoring:** [CPU, memory, sessions and latency](MONITORING.md) publishes one process sample and session gauges on the existing bearer-only status route. This is not a host monitor or a stored history. Schema v9 is unchanged.
**alpha-0.9.10 Logging:** [Secure logs and event recording](LOGGING_SECURITY.md) keeps a log file private to its owner and records fixed security outcomes without peer data. This is not a production audit. Schema v9 is unchanged.
**alpha-0.9.9 System security:** [Debian and Ubuntu service hardening](SYSTEM_SECURITY.md) ships one systemd example that runs the hub as a locked system user with no new privileges and a strict filesystem. The build does not install or start that service. Schema v9 is unchanged.
**alpha-0.9.8 Database security:** [MariaDB backup and failure handling](DATABASE_SECURITY.md) writes one consistent owner-only snapshot when the operator asks, and restores it only into empty tables of the same schema. A lost acknowledgement is not replayed. Off-host copies stay with the operator. Schema v9 is unchanged.
**alpha-0.9.7 API security:** [Permissions, sessions and CSRF](API_SECURITY.md) requires the session id and the CSRF secret on every maintenance call except the password proof. A grant alone no longer reads. Copying all three values from page memory still works until the slot ends. Schema v9 is unchanged.
**alpha-0.9.6 Account security:** [Passwords, GPA/PAS and secret protection](ACCOUNT_SECURITY.md) refuses an ADC password identical to the account password and decrypts that password only after the account attempt budget and an account ban check. Online guessing, a captured PAS with require_tls off, and buckets cleared by restart remain. Schema v9 is unchanged.
**alpha-0.9.5 TLS security:** [ADCS certificate and cipher policy](TLS_SECURITY.md) checks the hub leaf before listen and limits ADCS to TLS 1.2 and 1.3 AEAD. Trust, hostname, HTTP/2, OCSP and automatic renewal stay separate. Schema v9 is unchanged.
**alpha-0.9.4 Traffic limits:** [Spam and excessive presence traffic](TRAFFIC_LIMITS.md) caps ready-session INF updates before fanout. Chat, search and transfer budgets stay. Schema v9 is unchanged. A bandwidth cap and a public gateway remain separate work.
**alpha-0.9.3 Network security:** [TCP connection protection, timeouts and buffer limits](NETWORK_SECURITY.md) caps the listen backlog, each accepted socket's kernel send and receive buffers, and the TCP user timeout. The existing application timeouts and output queues stay. Schema v9 is unchanged. A bandwidth cap and a public gateway remain separate work.
**alpha-0.9.2 ADC security:** [Parser validation, fuzzing and protocol control](ADC_SECURITY.md) checks every parsed client frame against the hub command profile before session state changes. A deterministic in-process fuzz campaign locks the parser, the stream decoder and that profile together. Schema v9 is unchanged. Coverage-guided fuzzing and a public gateway remain separate work.
**alpha-0.9.1 Threat model:** [Hub and WebAdmin threat analysis](THREAT_MODEL.md) records assets, actors, trust boundaries and residual risk for the current hub and local WebAdmin. It does not add a control, a schema change, a public gateway or a production audit. Remote administrator access remains separate work.
**alpha-0.8.15 Security sessions:** [Access control, CSRF, sessions and request limits](SECURITY_SESSIONS.md) binds each in-memory grant to a public session id and a separate CSRF secret. Maintenance writes require both on that same slot. Each session accepts 24 requests in 10 seconds. Reads stay grant-only. The values stay in process memory, not in SQL, cookies or web storage. A public authenticated gateway remains separate work.
**alpha-0.8.14 Apache2 HTTPS proxy:** [HTTPS reverse proxy and API protection](APACHE2_PROXY.md) forwards an allowlisted WebAdmin API to the loopback adapter. Client bearer tokens, cookies and forwarded identity are removed. The C++ Unix socket and the service token stay out of Apache. Local access remains the default.
**alpha-0.8.13 WebAdmin Management:** [Account, role and ban management](WEBADMIN_MANAGEMENT.md) shows which maintenance areas the current grant allows and links to the existing account, role and ban pages. The page does not perform those writes. Configuration writes, live hub administration and browser kick or mute stay outside this page.
**alpha-0.8.12 WebAdmin Users:** [Online users and moderation actions](WEBADMIN_USERS.md) publishes the ready-session directory and a bilingual page of kick, mute and unmute decisions. The page does not dispatch those commands, create bans or reveal CID, IP, account or profile fields. Browser kick controls remain on an authenticated Direct Connect client.
**alpha-0.8.11 WebAdmin Dashboard:** [Main panel and statistics](WEBADMIN_DASHBOARD.md) adds a fixed read-only summary of existing process counters to the authenticated status snapshot and a bilingual WebAdmin page. Account lists, ban records, audit rows, configuration writes and live hub administration stay outside this panel.
**alpha-0.8.10 Admin API:** [Proven administrative access](ADMIN_API.md) requires an active account password before maintenance reads and writes. The process token remains the transport credential. Configuration writes and live hub administration remain pending.
**alpha-0.8.9 Audit Logs:** [Successful maintenance history](AUDIT_LOGS.md) records completed account, ownership and ban maintenance in schema v9. An active account with audit.read lists that history. Live kick and mute, refusals, passwords and free-text reasons stay out. Configuration writes and hub administration remain pending.

**alpha-0.8.8 Moderation Rules:** [Shared permission and target checks](MODERATION_RULES.md) decide kick, mute, unmute and account, CID and IP bans, including revocation. Schema v8 is unchanged.

**alpha-0.8.7 Mute:** [Live public/private chat moderation](MUTE.md) implements users.mute with authenticated hierarchy, session flags, +mute/+unmute and private aggregate monitoring. Schema v8 is unchanged. This supersedes earlier mute-pending statements.

**alpha-0.8.6 Temporary Bans:** [Relative duration, automatic expiry and server-clock UI](TEMPORARY_BANS.md) extend Ban Core while preserving schema v8 and SQL migrations 1–8. Use the new duration/filter API contract described there.

**alpha-0.8.5 Ban Core:** [Persistent account/CID/IP admission blocks](BAN_CORE.md) add explicit schema v8, proven create/revoke, startup enforcement and expiry. Admin Core now has ten maintenance actions; users.ban/blocks.manage are implemented. This supersedes earlier schema-v7/ban-pending statements.

**alpha-0.8.2 Operators:** [Staff directory and local ownership provisioning/transfer](OPERATORS.md) supersede earlier ownership-bootstrap/transfer limitations. Schema v7 and ordinary Owner protections remain.

**alpha-0.8.1 Permissions:** [the seven-role permission model](PERMISSIONS.md) adds exact capability keys, explicit rank checks, constrained VIP delegation, Guest-aware kicks and transactional maintenance role assignment. Explicit migration v7 broadens stored account roles without changing earlier SQL. Mute, bans, audit browsing and configuration writes remain policy definitions with engines pending. Global limits still apply to Owner.

**alpha-0.7.15 Configuration:** [database and login startup policy](DATABASE_AUTH_CONFIGURATION.md) adds shared effective timeout resolution, strict disabled-mode authentication bounds, a redacted --show-configuration preview and a bilingual read-only Configuration page. Secrets/private paths stay outside public snapshots; changes require validation and restart. CI verifies four logical CPUs per runner and derives parallelism from nproc, without a fixed test-worker cap or waiting for remote completion.

**alpha-0.7.14 WebAdmin Accounts:** [local account maintenance](WEBADMIN_ACCOUNTS.md) adds paginated checked metadata, disabled registration, activation, disable/reset and password changes through a separate non-serving executable. Stop serving hubs before writes; fresh administrator proof and revision checks protect changes. Schema v6 and startup authentication snapshots remain. CI uses four workers per command and is not awaited.

**alpha-0.7.13 Database Resilience:** [explicit recovery and independent timeouts](DATABASE_RESILIENCE.md) add one owner-thread reconnect attempt, fresh password-file validation and protection against SQL replay or live transaction replacement. Schema v6 and startup authentication snapshots remain. CI uses GitHub-hosted Ubuntu 24.04 runners; remote completion is not awaited. This entry supersedes earlier descriptions of reconnect as unavailable.

**alpha-0.7.11 Account Management:** [trusted password change, disable and reset](ACCOUNT_MANAGEMENT.md) now compose atomic account/credential writes with fresh password proof, revision checks and ADC credential removal. Perform these operations with the hub stopped, then explicitly reenroll/reactivate as appropriate and restart. No live reload, new SQL migration or public management endpoint.

**alpha-0.7.8:** [ADC Authentication](ADC_AUTHENTICATION.md) implements account-backed GPA/PAS with separate AES-256-GCM encrypted ADC passwords and an external private key, while preserving scrypt. Explicit migration v6 and active exact nickname/CID mappings are required; ADCS is the default. The bounded startup directory requires restart for changes/revocations. Public provisioning/reset/rotation and role grants remain future work. This entry supersedes earlier stage descriptions of authentication as planned.

**Current increment: alpha-0.7.6.** [Identity Mapping](IDENTITY_MAPPING.md) adds `dc24h_account_identities` through explicit schema v5. Existing account/credential rows and migrations 1–4 remain unchanged. Historical schema numbers below describe their original delivery scope.

**Current increment — alpha-0.7.5:** [Registration](REGISTRATION.md) atomically creates registered/disabled accounts and separately stored, randomly salted OpenSSL scrypt password verifiers. Migration v4 adds credential storage; validated Unicode inputs, fixed KDF costs and rollback prevent partial accounts. Authentication/session grants and public API/WebAdmin registration remain planned. CI retains the pinned Proxmox Ubuntu 24.04/22.04 workflow without merging github-runner-v0.1.

**Schema versioning — alpha-0.7.2:** [Database Migrations](DATABASE_MIGRATIONS.md) documents the ordered catalog, v1 upgrade, read-only status and partial-failure contract.

**Core delivered in alpha-0.7.1:** [Database Core](DATABASE_CORE.md) adds typed prepared parameters, bounded owning query results, execution metadata, owner-thread enforcement, redacted errors and RAII transactions. The immutable migration-1 baseline and operator setup below remain; dependent native components require rebuilding.

Implemented: optional Connector/C connection, RAII cleanup, bounded timeouts, UTF-8 (`utf8mb4`), prepared statements and an explicit migration-metadata baseline. Account metadata storage and role/status keys are implemented in [Account Repository](ACCOUNT_REPOSITORY.md). [Registration](REGISTRATION.md) adds separate salted scrypt verifiers using existing OpenSSL and an atomic account/credential transaction. Authentication, live role/permission mapping, configuration persistence, bans, administrative records and event persistence remain planned. Plaintext hub passwords are never stored. WebAdmin will use the C++ API rather than direct SQL.

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

`--migrate-database` now applies the ordered compiled catalog through v4, preserving v1's exact checksum and adding the `state` metadata column and account/credential tables. `--database-status` reads the current/latest/pending/dirty state without DDL; ordinary startup never migrates. Current bootstrap/upgrade credentials additionally need ALTER. Future non-bootstrap migrations persist `applying` before SQL and refuse automatic replay after partial failure. The full contract, source manifest, checksum format, privilege and operator recovery guidance are in [Database Migrations](DATABASE_MIGRATIONS.md).

## Adapter contract

`Connection` is noncopyable, synchronous and owned by one thread. Connector/C handles stay private. Queries use prepared statements for bound values; never concatenate untrusted identifiers or SQL. `execute()` accepts non-result statements; `scalar()` requires exactly one row/column, with a maximum 4096-byte value and `nullopt` for SQL NULL. SQL and individual bound values are capped at 64 KiB. No multi-statement execution or LOCAL INFILE is enabled. Closing each statement drains its resources even on result/prepare errors. Bounded owning queries and typed values are described in [Database Core](DATABASE_CORE.md).
