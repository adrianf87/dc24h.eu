# Temporary Bans — alpha-0.8.6

This increment builds on [Ban Core](BAN_CORE.md): absolute expiry and automatic
admission checks already existed in alpha-0.8.5. It adds relative durations,
central lifecycle helpers, filtered history and a server-based WebAdmin countdown.
The branch starts from main `bdcda8a7254fc5c71276bec28b9b2b96a09dcb47`, including
the ncdc cross-interface port reservation fix. There is no automatic merge.

## Time and lifecycle

All persisted times are whole Unix UTC seconds in `0..253402300799`.
`duration_seconds` is a canonical positive integer in `1..31536000` (365 days).
Admin Core computes `expires_at = command.time + duration_seconds`; invalid ranges,
overflow and combining relative and absolute expiry are refused before SQL access.
The HTTP service supplies command time; browser time is never used for relative
creation. The existing absolute `expires_at` path remains compatible, accepts a
future timestamp in the supported UTC range, and is not subject to the relative
365-day ceiling. Empty absolute expiry creates a permanent block.

| Derived state | Condition |
| --- | --- |
| revoked | `revoked_at` is present, regardless of expiry |
| permanent | Not revoked and no expiry |
| temporary | Not revoked and `expires_at > now` |
| expired | Not revoked and `expires_at <= now` |

Expiry is effective at equality. Account, CID and exact IP rules are checked on each
new admission against server UTC; no periodic SQL writer, cron task or restart is
needed for expiry. The immutable startup snapshot holds rules, never a cached
"active" flag. Duplicate rules retain the longest remaining expiry; a permanent
rule dominates. Expiring one record cannot bypass another active matching rule.
Expired rows retain ID, creator, reason, revision and timestamps. They are not deleted
or marked revoked, and no synthetic administrative action is counted. Expired rows
neither load into a fresh active snapshot nor consume active write capacity.

System clock corrections affect subsequent admission decisions: keep the host UTC
synchronized. Expiration does not disconnect established sessions. A rejected client
must reconnect. Creation/revocation still require stopping every serving instance,
using maintenance, and restarting to load policy; there is no live `+ban` command.
All role, password proof, target/creator hierarchy, write serialization and uncertain
commit protections from Ban Core remain in force. Schema **v8** and migration SQL
1–8 plus the manifest are unchanged; no migration 9 is needed.

## Private maintenance API

Same-UID Unix socket, Bearer authentication, maintenance origin and bounded
form bodies as [API](API.md). Reads return at most 16 records ordered by ID:

```http
GET /api/v1/bans?after=0&state=temporary
```

`after` is an optional canonical nonnegative signed-BIGINT cursor. `state` is
optional (`all` by default): `all`, `active`, `permanent`, `temporary`, `expired`,
`revoked`. `active` combines permanent and temporary records. `?state=expired` is
also valid; when both parameters are present, `after` precedes `state`. Unknown,
duplicate, empty or malformed parameters are refused. SQL filters before LIMIT;
`next_after` refers to the final matching row, or null for a short page. Pages are
live observations, not a transaction snapshot across HTTP requests; changing time
or another maintenance write can change membership. Refresh from the first page
when reviewing the complete current set.

Successful list and mutation responses add numeric `observed_at` (server UTC).
The list uses one captured timestamp for both SQL state predicates and its response.
Records keep their existing DTO fields; state and remaining time are derived.
The mutation timestamp is captured after the command completes, so a very short ban
may already have expired when the response arrives.

```text
POST /api/v1/bans
actor_id=42&proof=...&kind=ip&subject=192.0.2.10&reason=Policy&duration_seconds=3600
```

Exactly six fields are required: `actor_id`, `proof`, `kind`, `subject`, `reason`
and either `duration_seconds` or `expires_at`. Supplying both, even an empty
`expires_at`, is refused. Zero, signs, leading zeros, fractional or oversized
durations are refused. POST `/api/v1/bans/ID/revoke` still accepts exactly
`actor_id`, `proof`, `revision`. Duration is not accepted by revoke or any other
Admin Core action. Writes remain throttled and are never replayed after an unknown
outcome. Proofs are not returned, logged or retained in UI state/storage.

## WebAdmin

EN/US and Polish forms choose permanent, a whole duration in seconds/minutes/hours/
days, or an explicit local date converted to UTC. The total relative duration cannot
exceed 365 days. Status filters reset pagination. Revoking a row removes it from a
filter it no longer matches while preserving the selected record and its revision.

The countdown anchors `observed_at` to `performance.now()`, updates every second
and does not use `Date.now()`. A skewed browser wall clock cannot change derived
expiry. This is an estimate from the last server observation; transport delay and
later server clock corrections require refresh. Filter membership is refreshed on
request, so a displayed temporary row may turn expired before the next refresh.
The timer ends on unmount and clears when the listing is unavailable. Older API
responses without `observed_at` remain readable, with no fabricated countdown or
temporary/expired assertion. Permanent and revoked states remain unambiguous.
Filtered reads and relative creation require the new API.

## Verification

Native model/dispatcher tests cover second boundaries, zero/maximum/overflow,
revoke/permanent precedence, overlapping subjects and rejection before SQL.
Disposable MariaDB tests cover relative creation, stable revision/history,
SQL filtering before bounded paging, expired capacity and replacement. Private
HTTP tests cover duration, authoritative timestamps, expiry without writes, strict
query/body shapes and revocation. Existing real IP/CID expiration, pre-GPA account
checks, ncdc ADC/ADCS and uncertain-COMMIT checks remain in the full suite.
Node/Chromium cover DTO redaction, duration conversion, clock skew, live countdown,
filters, proof clearing and Polish mobile layout. Local builds/tests use eight CPU
threads; GitHub CI retains Ubuntu 24.04 with four logical CPUs per runner and
`nproc` parallelism. Remote CI completion is not awaited.
