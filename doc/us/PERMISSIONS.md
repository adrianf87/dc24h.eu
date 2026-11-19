# Permissions and role hierarchy — alpha-0.8.1

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

**alpha-0.8.2 Operators:** [Staff directory and local ownership provisioning/transfer](OPERATORS.md) supersede earlier ownership-bootstrap/transfer limitations. Schema v7 and ordinary Owner protections remain.

The hub recognizes seven fixed roles and seventeen named permissions. Role authority
comes from a checked database account and successful GPA/PAS login, never client
INF/SUP metadata. Guest is the ready accountless session role; it cannot be stored
as an account or bound as an authenticated account session. All names are exact,
case-sensitive EN/US keys. Unknown roles, permissions and VIP masks deny access.

## Default hierarchy and capabilities

| Role | Rank | Default capability additions |
| --- | --- | --- |
| Guest | 0 | hub.use, chat.public, chat.private; Search/Transfers follow existing guest settings |
| Registered | 1 | accounts.password.change and normal registered Search/Transfers grants, independent of guest restrictions |
| VIP | 2 | Registered capabilities plus explicitly configured VIP delegations |
| Moderator | 3 | users.kick, users.mute, audit.read |
| Operator | 4 | Moderator capabilities plus users.ban, blocks.manage |
| Administrator | 5 | Operator capabilities plus accounts.manage, roles.manage, configuration.manage |
| Owner | 6 | Every recognized permission, including hub.manage |

Ranks are explicit, not the underlying C++ enum numbers. Capability inheritance
never bypasses global feature switches, guest restrictions where applicable,
connection permissions, source/identity checks, ADCS requirements or flood/resource
limits. Owner receives all recognized capabilities, not an OS privilege or a
bypass of security controls. Account roles remain immutable within each startup
authentication directory; edits require stopped-hub maintenance and restart.

| Permission key | Enforcing mechanism available in this increment |
| --- | --- |
| hub.use | Existing ready-session/admission engine |
| chat.public / chat.private | Existing chat engine and configured gates |
| search.queries / search.results | Existing search grants and routing gates |
| transfers.initiate / transfers.receive | Existing transfer grants and routing gates |
| users.kick | Trusted role-aware LoginHub session revocation, including ready Guest targets |
| accounts.password.change | Existing current-password-proven own-account maintenance |
| accounts.manage | Administrator/Owner-proven maintenance account service |
| roles.manage | Administrator/Owner-proven maintenance role change |
| users.mute | Policy definition only; mute state/commands remain planned |
| users.ban / blocks.manage | Policy definitions only; ban/block engines remain planned |
| audit.read | Policy definition only; event-browsing service remains planned |
| configuration.manage | Policy definition only; Configuration page remains read-only |
| hub.manage | Policy definition only; live administrative lifecycle endpoint remains planned |

A grant is not an endpoint, complete authorization decision or implemented command.
The public catalog includes an `implemented` flag for each permission. The panel
separately shows grants and available core mechanisms; it does not offer fake
mute, ban, audit or configuration-write controls. Existing native session APIs
still require a trusted transport actor and owner-thread invocation.

## VIP configuration

```ini
[roles]
vip_permissions = none
# Optional delegated capabilities:
# vip_permissions = users.kick,users.mute,audit.read
```

VIP has no extra grants by default. Only these three named delegations are accepted;
administrative account/role/configuration grants and ban/block authority cannot be
assigned to VIP through this setting. Empty values, wildcards, unknown/duplicate
keys, duplicate capabilities, invalid case and trailing commas fail configuration.
Whitespace around comma-separated keys is allowed. Changes require restart.

A delegated VIP kick remains subject to hierarchy: only Guest and Registered are
lower. Delegating mute/audit defines future policy but does not create those
engines in this stage. The policy is validated at INI, Network and trusted Core
boundaries and copied into immutable runtime settings. Hot reload is not added.

## Target hierarchy and account safety

Moderation requires the corresponding permission and a strictly lower target rank.
The trusted kick actor must be a ready authenticated session in this hub; the
ready target may be authenticated or Guest. Missing, pending, closing, stale,
equal/higher-rank, self and backwards-time requests fail. Kicking clears account
association and search windows before any further routing. Pure permission checks
and session cleanup remain allocation-free.

Persistent account operations require accounts.manage, fresh active actor metadata,
current password proof and a strictly lower account target. Administrator can
manage Registered/VIP/Moderator/Operator. Owner can additionally manage
Administrator. Password change of one's own active account retains its separate
current-password-proven path. Guest has no persistent target account.

Role assignment also requires roles.manage and a persistent proposed role strictly
below the actor. Self, equal/higher targets, Guest assignment and promotion to the
actor's own or a higher rank are refused. Owner assignment/ownership transfer is
reserved for trusted offline provisioning, not this endpoint. No API operation
can demote, disable or reset an Owner, preventing accidental loss of ownership.
This increment does not provision a production Owner or change existing accounts.

`AccountManagementService::change_role` reuses the transactional management service:
validate/prove before locking, lock actor/target in ID order, recheck complete
snapshots and the exact verifier, increment revision, change role, disable the
target and delete its encrypted ADC credential in the same transaction. The account
password verifier and identity mappings are preserved. Stale snapshots, failed
proof, SQL/permission failures and uncertain commit outcomes do not trigger replay.
Existing account maintenance still requires all serving instances to be stopped.

The local maintenance HTTP route is:

```text
POST /api/v1/accounts/{id}/role
Content-Type: application/x-www-form-urlencoded
revision=...&actor_id=...&proof=...&role=vip
```

It retains private same-UID Unix sockets, server-side Bearer credentials,
same-origin/custom-header browser gates, exact bounded form framing, write-rate
limits and redacted responses. GET account metadata is authorized by the private
service token; it is not a new browser account-login session. Mutation authority
comes from the freshly proven actor, never a role string supplied by the browser.

After a role change, review the returned revision/status, explicitly reactivate
when appropriate, enroll a fresh separate ADC password through the existing trusted
service after activation, then stop maintenance and restart the serving hub. Role
changes do not silently upgrade an existing network session.

## Schema, API and panel

Explicit migration **v7**, `0007_roles.sql`, replaces only the account role CHECK
constraint with Registered/VIP/Moderator/Operator/Administrator/Owner. It preserves
rows, IDs, revisions, timestamps, credentials and mappings; Guest and unknown roles
remain forbidden in SQL and the C++ model. Migration files 1–6 remain byte-identical.
Back up and stop the hub before `--migrate-database`; startup never auto-migrates,
and dirty/interrupted history requires operator recovery. Older binaries refuse
the newer migration history; downgrade requires a compatible restored backup.

Authenticated status and `--show-configuration` publish a safe optional
`configuration.roles` object: canonical VIP delegations, seven ordered role/rank/
permission entries, and the permission/mechanism catalog. No account list, proof,
private path or secret is included. The bilingual Roles page supports selecting
any role and inspecting exact grants; older APIs show unavailable policy.
The Accounts page recognizes the six persisted roles and offers confirmed role
changes through maintenance. Owner/Guest cannot be selected for assignment.

## Validation

Tests cover the exhaustive seven-role/seventeen-permission matrix, strict target and
assignment hierarchy, unknown values, VIP parsing, Guest account/session refusal,
allocation-free checks, readiness barriers, VIP kicks and staff kicks of guests,
new role session binding, disabled/global routing policy precedence, transactional
role updates, proof/actor races and ADC credential removal. Disposable MariaDB
covers v6-to-v7 preservation, new values, Guest rejection and historical migration
prefixes. Actual ADC/ADCS login loads VIP/Operator/Owner roles and real ncdc remains
in the authentication/regression suites. Node/Chromium tests verify role parsing,
secret stripping, pending-action labels, role submission and Polish mobile UI.

Local Codex builds/tests use eight CPU threads. CI retains GitHub Ubuntu 24.04,
verifies four logical CPUs per runner and derives command parallelism from nproc;
there is no fixed test-worker cap. Remote CI completion is not awaited.
