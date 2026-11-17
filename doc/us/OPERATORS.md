# Operators and hub ownership — alpha-0.8.2

**alpha-0.8.5 Ban Core:** [Persistent account/CID/IP admission blocks](BAN_CORE.md) add explicit schema v8, proven create/revoke, startup enforcement and expiry. Admin Core now has ten maintenance actions; users.ban/blocks.manage are implemented. This supersedes earlier schema-v7/ban-pending statements.

**alpha-0.8.4 Kick:** [Live authenticated kick](KICK.md) adds the private +kick chat command, strict hierarchy, terminal notices, immediate routing cleanup and aggregate counters. Maintenance actions and schema v7 remain unchanged.

**alpha-0.8.3 Admin Core:** [Central maintenance command dispatch](ADMIN_CORE.md) now serves the account API and owner CLI, with entry-point isolation, fixed outcomes, private discovery and process counters. Existing proof/transaction rules and schema v7 remain.

This increment builds on [Permissions](PERMISSIONS.md) from main. Staff accounts
are Moderator, Operator, Administrator and Owner. Their authority comes from
persistent checked account roles and successful authentication, never client INF
or a browser-selected role. Registered/VIP users are outside the staff directory.
Disabled staff remain visible, but cannot authenticate or prove administrative
operations. The directory describes persistent accounts, not online presence.

## Staff lifecycle

Use the existing private maintenance Accounts API/WebAdmin to create a disabled
Registered account, change its role and activate it after review. Administrator
may assign Moderator/Operator and lower roles; Owner may additionally assign
Administrator. Actors and proposed roles must follow the strictly lower-rank
rules. Role change disables the account and removes its ADC credential. After
activation, enroll a fresh separate ADC password and restart serving instances.

The Operators page requests `GET /api/v1/accounts?staff_after=0`, then uses the
returned `next_after` cursor. Filtering occurs in SQL before each bounded page of
25 rows, so non-staff accounts cannot hide later staff entries. Responses contain
`maintenance: true`, `staff_only: true` and only the existing safe account metadata.
The adapter/browser reject unfiltered responses from older servers, duplicate or
unordered IDs, malformed records and non-staff roles. No credentials are exported.
GET access still requires the private service Bearer token and same-UID Unix
socket; there is no new public staff roster or browser account-login gateway.

Staff can be selected for the existing proven role, activation, disable/reset and
own-password operations. The server rechecks authority independently of the UI.
Demotion out of staff removes the row from that view. Use Accounts to promote an
account that is not yet staff. Owner cannot be assigned or removed by that form.

## Local ownership tool

`dc24h-owner` is installed alongside `dc24h` and `dc24h-account-admin`. It opens no
hub/API listeners, performs no migration and prints only fixed diagnostic text
plus IDs/revisions. It reads an existing private INI configuration with database
enabled and ADC authentication disabled. It requires the current clean schema v7.
No SQL files, manifest or stored account schema change in this increment.

Stop **every** serving hub and maintenance writer before use. The explicit
`--confirm-stopped` acknowledges this requirement; it does not stop or detect
other processes. Startup authentication directories are immutable, so an existing
session must never be treated as updated by maintenance. Keep configuration,
database credentials, account proofs and runtime paths under the trusted service
UID. Password files must be regular, same-UID, private (0600/0400), single-link and
not symlinks. Passwords are not accepted as command-line values. File contents and
derived verifier buffers are erased from application-owned buffers after use.
No password, name, path, verifier or database exception text enters CLI output.

A minimal maintenance configuration (adapt the private paths locally) is:

```ini
[general]
language = us
[database]
enabled = on
socket = /run/mysqld/mysqld.sock
name = dc24h
user = dc24h
password_file = /etc/dc24h/database.password
[authentication]
enabled = off
[api]
enabled = off
```

The existing SQL account needs SELECT, INSERT, UPDATE and DELETE privileges on the
relevant account/credential tables; no new database grants are applied by the tool.
Store account passwords in separate private files with at least 15 characters,
following the existing account password policy. A trailing LF/CRLF is stripped.
`[general] language = pl` selects Polish completion/refusal messages; field names
and flags remain EN/US. `--help` describes both command forms; `--version` reports
the release without reading configuration or contacting SQL.

### First Owner

```sh
dc24h-owner --config /etc/dc24h/owner-maintenance.ini \
  --bootstrap HubOwner --password-file /etc/dc24h/owner-account.password \
  --confirm-stopped
```

Bootstrap creates one **new active Owner** with a scrypt account verifier. It
refuses any existing Owner, including disabled ones, and never promotes an
existing account or resets a forgotten password. An existing name is rejected.
A per-database, nonblocking MariaDB advisory lock serializes bootstrap/transfer
requests; concurrent first-Owner requests cannot both succeed. Trusted SQL writers
must remain stopped: they are outside this application lock protocol.

Creation is atomic with credential insertion. The new Owner has no ADC identity,
ADC password or live session. For hub login, explicitly bind a checked identity
and enroll a separate ADC password through the existing trusted
`IdentityRepository`/`AdcCredentialService` interfaces before restarting. Browser
ADC enrollment and a general end-user provisioning CLI remain future work.
The account password can prove maintenance account/staff operations immediately.
Bootstrap is unavailable through HTTP, WebAdmin and ADC client commands.

### Ownership transfer

Prepare an active Administrator successor through the reviewed staff lifecycle,
then obtain fresh IDs/revisions from maintenance metadata and stop maintenance.
Both the current active Owner and the active Administrator must prove their own
account passwords, supplied through separate private files:

```sh
dc24h-owner --config /etc/dc24h/owner-maintenance.ini \
  --transfer 1 --revision 3 --to 2 --to-revision 5 \
  --password-file /etc/dc24h/owner-account.password \
  --target-password-file /etc/dc24h/successor-account.password --confirm-stopped
```

The transaction makes the successor an **active Owner** and the former Owner a
**disabled Administrator**, increments both revisions and deletes both ADC
credentials. It preserves account passwords, creation/login metadata and identity
mappings. Re-enroll the successor's ADC password explicitly before hub login.
A current active Owner remains after successful transfer; there is no intermediate
committed ownerless state. Legacy multiple Owners created through trusted SQL
are preserved; transfer replaces only the proven actor and does not change their
count. This tool does not silently consolidate or recover such accounts.

Proofs are checked before locking; complete metadata and exact credential records
are rechecked under account locks in ascending ID order. Wrong proofs, stale
revisions, self-transfer, disabled/non-Owner actors and non-active/non-Administrator
successors are refused. Ordinary account operations continue to protect Owner
from demotion/disable/reset. Password changes on one's own active account retain
the existing separate proof path. There is no force-transfer or recovery bypass.

SQL failures roll back both role changes and both credential deletions. The tool
never reconnects or retries writes. An uncertain SQL acknowledgement exits with
code 3 and instructs inspection before retry; normal refusals exit 1 and success
exits 0. Inspect current metadata/storage after any unknown outcome before taking
further action. Schema migration, deployments, production Owner creation and
production account changes are explicit operator actions, not release steps.

## Validation and remaining scope

Native tests cover simultaneous first-Owner requests, disabled-owner bootstrap
refusal, both proofs, hierarchy, stale snapshots/credential races, rollback of
both updated accounts on missing DELETE privileges, revision increments, secret
preservation, ADC invalidation and SQL staff pagination across non-staff gaps.
A real CLI test uses disposable MariaDB for bootstrap/transfer, stale replay,
private-file checks and redacted output. Actual private HTTP, Node adapter and
Chromium tests cover filtered staff records, old-API refusal, demotion and Polish
mobile navigation. Existing real ncdc ADC/ADCS and authentication suites remain.

Mute, bans/blocks, audit browsing, live ownership commands and remote browser
ownership transfer remain future work; alpha-0.8.1 capability definitions alone
do not provide those engines. Local tests use eight CPU threads. GitHub CI keeps
Ubuntu 24.04 with four logical CPUs per runner and CPU-derived parallelism;
remote CI completion is not awaited.
