# Roles — alpha-0.7.10

This stage starts from main `4390bd0ab8f4680f4ece63afb1354db66aaff9e9`.
It activates the existing persisted `registered`, `moderator` and `administrator`
account roles in authenticated ADC/ADCS sessions. The branch is not merged into
main automatically. No SQL migration is added; schema v6 and credentials remain unchanged.

## Trusted role binding

The immutable startup `AdcAuthenticationDirectory` now retains the validated
account role read from MariaDB. `PasswordAuthentication` carries it as trusted
adapter metadata. A pending GPA or successful PAS alone grants no live role:
only the final own-BINF LF commits readiness and the checked role together in
`users::AuthSession`. Guest sessions have no account role. Unknown enum values
fail closed in the policy module, the authentication adapter and the managed
ready transition; failures leave the managed model unchanged.

Account names, nicknames, CID/SID, SUP feature claims and client INF fields never
select a role. Existing client privilege-claim rejection remains enabled. No new
public CT badges or operator commands are added. The [ADC specification](https://dc-protocols.github.io/ADC.html)
defines protocol metadata and GPA/PAS; the internal capability matrix below is
hub policy, not a protocol claim supplied by the client.

## Capability matrix

`dc24h_roles` exposes allocation-free `security::allows(Role, Permission)` and
`security::can_revoke(actor_role, target_role)`. Permission and role enums are
checked explicitly, without numeric ordering or an allow-by-default fallback.

| Operation | Guest | registered | moderator | administrator |
| --- | --- | --- | --- | --- |
| Send search queries/results | Configured guest policy | Allowed | Allowed | Allowed |
| Initiate/receive CTM or RCM | Configured guest policy | Allowed | Allowed | Allowed |
| Revoke a registered account session | Denied | Denied | Allowed | Allowed |
| Revoke a moderator session | Denied | Denied | Denied | Allowed |
| Revoke an administrator session | Denied | Denied | Denied | Denied |
| Revoke own session through the staff API | Denied | Denied | Denied | Denied |

On authenticated readiness the hub seeds existing per-connection Search and
Transfers grants from that role. Guests retain the configured defaults, including
the existing default of enabled guest communication. To restrict search and
transfer operations to authenticated accounts, configure:

```ini
[search]
guest_queries = off
guest_results = off
[transfers]
guest_initiate = off
guest_receive = off
```

Global feature switches, protocol allowlists, syntax/source/identity checks,
active/passive connectivity requirements, output/user/token capacities and
sender/recipient rate limits still apply to every role. Administrator does not
bypass these gates. Chat retains its existing common policy and flood limits.
Trusted `set_search_permissions` / `set_transfer_permissions` overrides remain
in force for that lifetime after readiness and do not get reapplied/reset on
SUP, INF or ordinary traffic. Revoking query permission still deletes tracked
search windows. Role assignment never refills rate credits.

## Staff operations and current session checks

`LoginHub::role_allows(id, permission)` checks the role capability of an exact
ready authenticated lifetime. Missing, zero, guest, pending and closing IDs deny.
It is a role-policy query only: it is not a substitute for the effective
operation-specific checks described above.

`LoginHub::revoke_auth_session_as(actor, target, now)` is an owner-thread trusted
adapter API. Its caller must obtain the actor ID from the actual authenticated
transport/context, never from a request body or an arbitrary account/SID.
The hub re-resolves both current lifetimes, checks the explicit hierarchy, rejects
self/equal/higher-role targets and backwards times, then invokes the existing
allocation-free connection revocation. The actor must still be ready. Repeated,
stale and removed IDs cannot affect a replacement connection. Saved role/session
snapshots are historical data, not authority. Closing clears the role together
with the account association and immediately prevents further role-authorized actions.

The prior `revoke_auth_session(target)` remains a privileged control-plane API
for trusted hub ownership code; it is not a client-facing authorization surface.
No ADC kick command, HTTP/WebAdmin role management endpoint, browser session,
account-wide ban or persistent role editor is exposed by this stage. The existing
private API token does not identify an ADC account or grant an ADC role.

## Role assignment and changes

Roles are stored in the existing accounts table. Trusted administration code may
use `Account::with_access(role, status, changed_at)` and `AccountRepository::update`
with its existing revision checks; the caller must authorize that operation.
Do not run SQL, scrypt or enrollment on the network reactor. Registration still
creates disabled registered accounts and never accepts a public role grant.

Role, account-status, mapping and credential changes remain startup snapshots.
Restart the hub after updating persistent access policy. Existing connections
retain their original role until closed; loading a new directory reflects the
updated role. Per-connection revocation disconnects only that lifetime and does
not block a fresh valid login against the unchanged directory. Custom roles,
dynamic permission editing/reload and persistent moderation audit are future stages.

## Verification

`security.roles` tests the full role/capability and revocation matrix, unknown
values, atomic invalid-role refusal, final-LF publication, guest separation,
readiness/close/stale-ID checks, staff revocation under allocation failure,
role grants through real LoginHub routing, global disable precedence and explicit
per-session overrides. Existing auth-session tests remain enabled.

`accounts.adc_authentication.process` validates role loading from real MariaDB,
role snapshots versus fresh directory loads, disabled staff refusal, registered
and moderator routing with guest operations disabled, global administrator
restrictions over actual TCP/TLS, client privilege-claim refusal and secret
isolation. It retains real ncdc wrong/correct-password, reconnect, welcome and
chat tests for ADC/ADCS. No production credentials or database are used.

See [Testing](TESTING.md) and [Changelog](../../CHANGELOG.md) for local validation.
[GitHub CI](GITHUB_CI.md) runs Debug/Release and WebAdmin on hosted Ubuntu 24.04
using all available CPUs through `nproc`; remote completion is not awaited.
