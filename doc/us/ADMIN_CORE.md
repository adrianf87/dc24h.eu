# Admin Core — alpha-0.8.3

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
