# Admin Core — alpha-0.8.3

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

**alpha-0.8.5 Ban Core:** [Persistent account/CID/IP admission blocks](BAN_CORE.md) add explicit schema v8, proven create/revoke, startup enforcement and expiry. Admin Core now has ten maintenance actions; users.ban/blocks.manage are implemented. This supersedes earlier schema-v7/ban-pending statements.

**alpha-0.8.4 Kick:** [Live authenticated kick](KICK.md) adds the private +kick chat command, strict hierarchy, terminal notices, immediate routing cleanup and aggregate counters. Maintenance actions and schema v7 remain unchanged.

The central `dc24h_admin` library now dispatches account and ownership mutations
from the private maintenance API and local `dc24h-owner` tool. Both adapters
construct an `admin::Command`; they no longer resolve mutation targets, compare
revisions or dispatch directly to individual account operations. The engine owns
that sequence and delegates the existing cryptographic, hierarchy and transaction
rules to `AccountManagementService`.

This is a synchronous **maintenance** engine. Stop every serving hub before writes;
immutable startup authentication directories and active sessions are not updated.
It is not a live ADC command channel, public remote administrator login or a new
ban/mute/audit engine. Existing trusted `LoginHub` moderation remains separate.
Schema v7, every migration SQL file and the manifest remain unchanged. No production
account, credential, migration or deployment is changed by publishing this release.

## Commands and entry-point isolation

| Exact command key | Private maintenance API | Local owner CLI |
| --- | --- | --- |
| accounts.create | Existing disabled registration | Refused |
| accounts.activate | Existing proven activation | Refused |
| accounts.disable | Existing proven disable | Refused |
| accounts.password | Own active account, current password proof | Refused |
| accounts.reset | Stronger actor proof, target disabled | Refused |
| accounts.role | Stronger actor/proposed rank, target disabled | Refused |
| ownership.bootstrap | Refused | Existing first-Owner provisioning |
| ownership.transfer | Refused | Existing dual-proof atomic handover |

These are command names, not new role permissions. Availability describes the
entry point only: each operation still needs account authorization. A Bearer token
allows reaching the private API, not bypassing the proven actor and role rules.
Origin is fixed by the trusted adapter when constructing the engine; neither HTTP
form fields nor a client role string can select it. Unknown commands, origins,
roles, missing revisions and irrelevant command fields fail closed before SQL.
There is no generic HTTP execute endpoint or ownership HTTP route.

Commands carry borrowed string views for proofs/passwords, checked numeric account
references and a UTC timestamp. The engine does not retain the command, secrets,
account IDs, names or exception text. The caller owns and clears plaintext after
the synchronous call. Ordinary actor revision is optional for compatible existing
HTTP forms, but the engine loads fresh actor metadata and the account service
rechecks it with the exact proof under its row lock. Ownership transfer requires
both explicit revisions. Existing-account target revisions are always mandatory.

For each mutation the engine validates the action/entry point and command shape,
opens one connection on the calling thread, resolves fresh account snapshots,
checks requested revisions, and calls the existing service. Results contain only
safe stored account metadata or the two metadata records for ownership transfer.
Account creation remains disabled; role changes/reset disable and invalidate ADC;
ownership transfer preserves an active successor and disables the former Owner.
Proof verification, ordered row locks, verifier rechecks, revision increments,
rollback and ADC invalidation remain in the tested account service. Owner cannot
be assigned/removed through ordinary role management. See [Operators](OPERATORS.md)
and [Permissions](PERMISSIONS.md) for the complete lifecycle.

The engine never migrates, reconnects or replays writes. All SQL errors with an
uncertain outcome remain `outcome_unknown`, including lost COMMIT acknowledgement;
they must not become a normal refusal or a successful result. `std::bad_alloc`
propagates after recording service unavailability. Other mutation failures become
fixed `admin::Error` kinds: invalid_input, forbidden, stale, not_found, unavailable,
or outcome_unknown. Error construction and fixed diagnostics require no allocation. Native backend exception text is not exposed. The owner CLI
retains its private-file input, stopped-hub acknowledgement and unknown-outcome
exit code 3. The account HTTP routes and their request forms remain compatible.

## Safe discovery and process counters

The current private maintenance server adds authenticated:

```text
GET /api/v1/admin
```

The same-UID Unix socket, private server-side Bearer token and framing bounds remain
mandatory. Serving `dc24h` instances have no administrative handler and return 404.
GET discovery performs no SQL query, KDF, credential read or account lookup; it is
not a database-health probe. It remains useful when SQL is unavailable.

```json
{
  "maintenance": true,
  "actions": [
    {"key": "accounts.create", "available": true, "local_owner_only": false},
    {"key": "ownership.transfer", "available": false, "local_owner_only": true}
  ],
  "statistics": {
    "attempts": "9", "succeeded": "5", "refused": "2",
    "unavailable": "1", "outcome_unknown": "1"
  }
}
```

The excerpt abbreviates the canonical eight-element ordered catalog. Counters are
exact unsigned 64-bit decimal strings and saturate rather than wrap. Attempts
count calls reaching this engine's `execute`, with exactly one terminal outcome
per call. Refused covers input, authority, revision and missing-account refusals;
unavailable covers known service failures. Unknown write outcomes have their own
counter and never increase succeeded. Transport authentication, malformed forms,
HTTP rate limiting and read-only queries are outside these command counters.
Counters are local to the engine instance, reset on restart and retain no event
history. They are not a persistent audit trail or a retry/idempotency mechanism.
Use each engine from one caller/thread at a time; the maintenance server owns a
single HTTP worker. Independent instances have independent counters.

WebAdmin's bilingual Admin Core page requests the allowlisted adapter route
`GET /admin-api/v1/admin`. The adapter and browser validate the exact catalog,
local-only ownership flags, counter types/ranges and maintenance marker, and strip
all additional fields. Large integers are preserved without JavaScript rounding.
The page has manual refresh and clears stale values after refusal, invalid data
or service loss. Older APIs/serving hubs show unavailable. It supplies no new
mutation form; Accounts/Operators retain their existing proven operations.

## Validation and remaining scope

Tests cover command parsing/shape, unknown origins/actions, entry-point isolation,
all eight operations through the central engine, own-password behavior, hierarchy,
stale actor/target revisions, missing accounts, private HTTP discovery, command
counters, service loss and rollback on denied SQL privileges. A private MariaDB
fault proxy discards an actual successful COMMIT acknowledgement: the engine
reports outcome_unknown once, while an independent connection proves exactly one
atomic account/credential insertion. Existing real owner CLI and HTTP lifecycle
suites now exercise the shared dispatcher. ncdc ADC/ADCS regressions remain.
Node/Chromium tests cover catalog/precision validation, redaction, local-only
ownership labels, stale-state clearing and Polish mobile rendering.

Live moderation dispatch, durable audit/event browsing, ban/block/mute engines,
remote administrator sessions and live configuration writes remain future work.
Local builds/tests use eight CPU threads. GitHub CI retains Ubuntu 24.04 with four
logical CPUs per runner and nproc-derived command parallelism. Its completion is
not awaited.
