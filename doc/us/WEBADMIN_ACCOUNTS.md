# WebAdmin account management — alpha-0.7.14

**alpha-0.8.2 Operators:** [Staff directory and local ownership provisioning/transfer](OPERATORS.md) supersede earlier ownership-bootstrap/transfer limitations. Schema v7 and ordinary Owner protections remain.

**alpha-0.8.1 Permissions:** [the seven-role permission model](PERMISSIONS.md) adds exact capability keys, explicit rank checks, constrained VIP delegation, Guest-aware kicks and transactional maintenance role assignment. Explicit migration v7 broadens stored account roles without changing earlier SQL. Mute, bans, audit browsing and configuration writes remain policy definitions with engines pending. Global limits still apply to Owner.

The Accounts page supports bounded account lists, account selection, disabled
registration, activation, disabling, current-password changes and administrator
password resets. EN/US is the default language; Polish is available in the panel.
IDs and revisions are decimal strings to preserve MariaDB BIGINT precision.
There is no credential/hash export, account deletion, role editing, CID mapping
editor, first-administrator bootstrap or public self-registration in this stage.

## Maintenance lifecycle

The serving hub still loads its authentication directory once at startup. Stop
all serving hub processes sharing the database before account writes. The new
`dc24h-account-admin` executable opens only a private local API socket: it creates
no ADC/ADCS listener and loads no authentication directory. It refuses disabled
SQL/API settings, ADC authentication enabled in its configuration, dirty/pending
migrations or a schema other than v6. It never migrates automatically.

Use a separate private maintenance INI, for example:

```ini
[database]
enabled = on
socket = /run/mysqld/mysqld.sock
name = dc24h
user = dc24h_maintenance
password_file = /etc/dc24h/maintenance.password
[api]
enabled = on
socket = /run/user/1000/dc24h-maintenance/admin.sock
token_file = /etc/dc24h/maintenance.token
[authentication]
enabled = off
```

Provision the schema and dedicated credentials through the existing installation
procedure. The socket parent must belong to the effective UID with mode 0700;
password/token files require private owner-only permissions. The maintenance SQL
account needs SELECT on the schema, INSERT/UPDATE on dc24h_accounts and
dc24h_account_credentials, and DELETE on dc24h_account_adc_credentials; no DDL
privileges are needed after explicit migrations. Stop the hub, then:

```sh
dc24h-account-admin --config /etc/dc24h/maintenance.ini
cd webadmin
DC24H_API_SOCKET=/run/user/1000/dc24h-maintenance/admin.sock \
DC24H_API_TOKEN_FILE=/etc/dc24h/maintenance.token npm run dev
```

Open the local panel's Accounts page. The adapter reads the API token server-side;
the browser never supplies or receives it. Review the account and operation,
enter the applicable account password and confirm that the hub is stopped. New
accounts are registered/disabled. Create/activate/disable/reset require an existing
active administrator and fresh password proof. Administrators cannot manage their
own access or other administrators. Password change proves the selected account's
current password. No role can bypass the existing management service's checks. Disabled registration
also rechecks the exact administrator snapshot and proof record under row locks
in the transaction that inserts the account and verifier.

Reset leaves its target disabled. Activation, disabling and password changes
invalidate stored ADC credentials atomically with metadata updates. Enroll a fresh
separate ADC password using the trusted existing enrollment service when needed;
this stage does not add browser ADC enrollment. Stop maintenance and restart the
serving hub to load the final checked account snapshot. The maintenance process
cannot detect a separate hub on another host: stopping all serving instances is
an operator requirement, and the panel makes this precondition explicit.

## Local HTTP contract

All routes require the private Bearer token and the same-UID Unix-socket peer
check. Only the maintenance executable registers account handlers. The ordinary
`dc24h` serving API retains its bodyless, read-only health/status contract.

| Method | Route | Payload/result |
| --- | --- | --- |
| GET | `/api/v1/accounts?after=0` | Up to 25 accounts in ascending ID order; nullable next_after |
| GET | `/api/v1/accounts/{id}` | Checked account metadata and maintenance=true |
| POST | `/api/v1/accounts` | actor_id, proof, name, password; disabled registered account |
| POST | `/api/v1/accounts/{id}/activate` | revision, actor_id, proof |
| POST | `/api/v1/accounts/{id}/disable` | revision, actor_id, proof |
| POST | `/api/v1/accounts/{id}/reset` | revision, actor_id, proof, password |
| POST | `/api/v1/accounts/{id}/password` | revision, proof, password |

POST bodies use `application/x-www-form-urlencoded`, exact Content-Length and no
chunking, pipelining, duplicate headers or duplicate/unknown form fields. The
entire request remains capped at 8192 bytes; body at 7000 bytes and decoded field
at 1024 bytes. Percent-encoded UTF-8 is validated by the existing account/password
services. Password policy remains at least 15 characters, up to 1024 UTF-8 bytes.
Writes are serialized on the local API worker and admitted at most once per second
per maintenance process, including failed attempts. The socket request deadline
is 30 seconds in maintenance. SQL connections are constructed/closed on that worker;
there is no reactor SQL, persistent shared connection, retry or replay.

Account results expose only ID, name, role, status, revision and UTC created/updated/
last-login timestamps. Error responses are fixed and redacted. A revision conflict
returns 409; invalid proof/authority returns a refusal, throttling 429, unavailable
storage 503. Lost write acknowledgement returns `outcome_unknown`; refresh and
reconcile before any deliberate new write. HTTP/network errors never cause a
browser or adapter write retry.

The local Vite adapter retains loopback/Host/Origin/Fetch-Metadata gates. Writes
add an exact same-origin Origin and custom `X-DC24H-Admin: 1` header, bounded form
bodies and four concurrent upstream requests. Responses are validated and rebuilt
from an allowlist. Proofs/passwords are not placed in React state, local/session
storage, URLs, exports or logs. Inputs clear on submission; C++ owned form/request
buffers are cleansed. JavaScript/browser/connector internal copies cannot be
promised to be erased. Native shutdown joins in-progress bounded work.

This remains a local maintenance workflow. Apache examples continue to serve
static assets and deny API paths; they do not become an authenticated production
mutation gateway. Remote browser login, live account changes and production gateway
authentication remain later work.

## Verification

The disposable MariaDB `accounts.webadmin.process` suite exercises list paging,
real password proof, activation, disable/reset/change, duplicate names, revision
conflicts, role/self guards, throttling, private diagnostics and clean maintenance
shutdown without hub listeners. Existing account/ADC/ncdc suites remain enabled.
Node tests cover metadata validation, BIGINT precision, response redaction and
adapter origin/custom-header gates. Chromium covers selection, confirmed writes,
stale errors, cleared secrets, unavailable controls and Polish mobile layout.
CI uses GitHub-hosted Ubuntu 24.04 and four workers per build/test command. Remote
CI completion is not awaited.
