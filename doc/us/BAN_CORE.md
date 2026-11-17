# Ban Core — alpha-0.8.5

The hub now stores and enforces permanent or expiring admission blocks for account
IDs, canonical Tiger CIDs and exact IPv4/IPv6 addresses. The private maintenance
service and bilingual WebAdmin can create/revoke records. Serving hubs load an
immutable startup directory; admission performs no SQL, password work or allocation.
This increment adds schema **v8** and keeps migration SQL files 1–7 unchanged.

## Scope and identifiers

| Kind | Subject | Enforcement |
| --- | --- | --- |
| account | Positive canonical decimal account ID | Account-backed identification, before GPA is sent |
| cid | Canonical 39-character Tiger CID | Verified client INF, before public presence or GPA |
| ip | One numeric IPv4/IPv6 address | Accept, before TLS/model/SID allocation |

Account bans apply to account authentication; with account authentication disabled,
there is no account identity to check. CID/IP blocks still work whenever the database
ban directory is loaded. A database-disabled serving hub has no persistent directory.
CID remains a client-controlled identifier; it is not a substitute for account proof.
IP blocks affect **every** connection from the address, including staff and shared
NAT clients. Roles do not bypass resource blocks. Exact IPs only: hostnames, CIDR,
ports, brackets and zone suffixes are refused. IPv6 is normalized; mapped IPv4
addresses become ordinary IPv4, so mapped aliases cannot evade matching.

Reasons are private metadata, nonempty already-NFC UTF-8 up to 256 bytes without
controls/formatting/line separators. They never become public protocol or log text.
`expires_at = null` means permanent. Otherwise expiry is strictly after creation,
within the existing UTC range. Expiration is checked at every new admission without
restart; an already rejected connection must reconnect. Blocks do not change account
status, password/ADC credentials or identity mappings. Maintenance password authority
remains a separate private capability, enabling recovery of an IP block locally.

## Maintenance authority and storage

Stop all serving instances before writes, then restart to load creation/revocation.
Existing sessions are not mutated and no live `+ban` command is introduced. Ban Core
adds `bans.create` and `bans.revoke` to the central Admin Core catalog (ten maintenance
commands in total). Both are available only through the private maintenance origin;
the ownership CLI retains its two ownership-only actions.

An active Operator or higher must prove the current account password and have both
`users.ban` and `blocks.manage`. Account targets and already-mapped CID targets must
have strictly lower rank; self/equal/higher targets are refused. Unknown CID targets
have no account role to compare. Their mappings are rechecked under lock. Revoke
also requires the original creator to be the actor or currently lower-ranked.
A future trusted identity binding can change who uses a previously blocked CID;
review resource blocks when provisioning staff. There is no hidden Owner exemption
or force-revoke bypass. Use trusted offline recovery for conflicting legacy data.

Each write validates canonical input, current schema, fresh actor/target metadata
and the exact verifier, performs the KDF before locking, then rechecks account rows
in ID order, CID mapping and exact proof under transaction locks. A per-database
nonblocking advisory lock serializes ban writes and the active-capacity check;
reentrant acquisition is refused without releasing the caller's lock. Record
revision guards prevent stale revoke/replay. A new block inserts one record;
revocation increments its revision and sets revoked time/actor without deleting it.
At most 4096 simultaneously active records are accepted. Duplicate subjects are
allowed: any remaining active record blocks, and the strongest expiry/permanent
rule wins. Revoke every applicable record to unblock a duplicated subject.

SQL migration `0008_bans.sql` creates the InnoDB table, indexes, kind/time/revision
checks and restrictive creator/revoker account foreign keys. Earlier account rows,
roles, credentials and mappings are preserved. Startup does not auto-migrate.
Back up, stop the hub, run the existing explicit `--migrate-database` action and
verify clean v8 before restarting. Older migration/account-authentication tools reject newer history. Older guest-only
binaries may not inspect it and cannot enforce these bans; downgrade requires a
compatible restored backup and reviewed serving behavior. Runtime SQL needs SELECT on the
ban table and migration metadata; maintenance additionally needs INSERT/UPDATE on
bans plus existing account/proof access. The application grants no privileges.

The directory loads active records in bounded 1024-row pages within a read
transaction and builds immutable account/CID/IP hash indexes. Malformed active
records, capacity overflow, incompatible/dirty schema or SQL failure abort startup
before listeners bind. Expired/revoked rows are excluded. No reason/creator/proof
is retained in the serving index. Out-of-band SQL writers remain outside the
maintenance protocol and must stay stopped during maintenance/startup.

## Protocol and API

IP bans close the accepted socket before TLS; no application notice is possible
at that stage. CID/account bans send generic `ISTA 231 Access blocked` for permanent
or `ISTA 232 ... TL<seconds>` for temporary blocks, then an own final IQUI with
matching TL (-1 for permanent). No GPA or public join is published for the refused
identity. The messages use existing [ADC STA/QUI semantics](https://adc.sourceforge.io/ADC.html).
Fixed `event=banned` logging contains no subject/reason/password; IP drops use the
existing aggregate admission rejection counters without per-address payload logs.

The stopped-hub service adds:

```text
GET  /api/v1/bans?after=0
POST /api/v1/bans
POST /api/v1/bans/{id}/revoke
```

Creation form: `actor_id`, `proof`, `kind`, `subject`, `reason`, `expires_at` (empty
for permanent, otherwise Unix UTC seconds). Revoke form: `actor_id`, `proof`,
`revision`. The existing same-UID private Unix socket, service Bearer token, bounded
same-origin/custom-header adapter forms, write throttle and fresh proof apply.
There is no public browser administrator login or serving-hub ban mutation route.
GET pages contain at most 16 records, including expired/revoked metadata, ordered
by ID with `next_after`. IDs/revisions are exact decimal strings. Responses mark
`maintenance: true` and `restart_required: true`; create/revoke return a `ban` record.
Reasons/subjects are visible only through this private maintenance interface.

No operation reconnects or retries writes. An uncertain SQL acknowledgement remains
`outcome_unknown` and must be inspected before another write. Response construction
failure after a successful mutation is likewise reported as uncertain to the client;
central command counters still reflect the completed backend operation. A private
fault proxy test loses a real successful ban COMMIT reply and independently proves
one persisted record without replay.

The Bans page supports exact subject selection, optional local-time expiry input,
proof/confirmation, stale-revision refusal and explicit restart guidance. Passwords
are cleared on submit and never put in storage, URLs, state or exports. The adapter
and browser allowlist ban DTOs, preserve BIGINT precision, validate UTF-8/time/subject
shape and strip additional fields. Older/serving APIs disable the form. Ban records
provide lifecycle metadata, not a complete persistent audit event system.

## Validation and remaining scope

Tests cover canonical/mapped addresses, CID shape, expiry/permanent precedence,
allocation-free lookup, authority/hierarchy, exact proof races, stale revoke,
malformed stored data, v7→v8 preservation and historical migrations. Native live
admission verifies account/CID denial before GPA/presence. A real disposable SQL hub
checks IP/CID rejection and expiry without restart; private HTTP exercises creation
and revoke through Admin Core. Node/Chromium and real API/adapter/React regressions
cover DTO redaction, forms, revisions, precision and Polish mobile UI. Existing
ncdc ADC/ADCS login and kick tests remain required.

CIDR/subnet bans, live ban commands/reload, public remote administrator sessions,
full audit browsing and mute remain later increments. Local commands use eight CPU
threads. GitHub CI retains Ubuntu 24.04 with four logical CPUs per runner and nproc
parallelism; remote completion is not awaited. No production migration or deployment
is performed as part of this release.
