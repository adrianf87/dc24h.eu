# Moderation rules — alpha-0.8.8

Kick, mute, unmute, account/CID/IP bans and ban revocation now share one
allocation-free decision. The actor must hold the permission for that operation.
The target must be the kind of subject the operation accepts, and must pass that
operation's rank rule. This does not add a chat command, a SQL migration, a
stored mute, a live `+ban`, or a browser button that performs moderation.

Schema v8 and migration files 1–8 stay unchanged. Wire and maintenance result
codes stay the codes already returned by kick, mute and Ban Core. A specific
refusal is not sent to the client and is not an audit record.

## Decision

`security::moderate(operation, actor, target, policy)` returns one of:

| Refusal | Meaning |
| --- | --- |
| `none` | The permission and the target both pass |
| `missing_permission` | The actor lacks the operation permission, VIP was not delegated it, or a ban operation lacks `blocks.manage` |
| `self_target` | The operation forbids acting on the actor |
| `not_lower` | The comparable rank is missing, equal or higher |
| `wrong_subject` | The subject kind does not belong to the operation |

Unknown operations, unknown roles and a VIP mask outside `users.kick`,
`users.mute` and `audit.read` fail closed. The function does not allocate, read
a clock, open SQL or retain a nickname, CID, address, reason or password.
Account status, password proof, revision locks and session readiness remain the
caller's checks. A disabled account still cannot maintain bans.

## Operations

| Operation | Permission | Target |
| --- | --- | --- |
| Kick | `users.kick` | Ready session, strictly lower role, including Guest. Self is refused. VIP only when `users.kick` is delegated |
| Mute and unmute | `users.mute` | Same session rule as kick. The two operations are not interchangeable with kick. VIP only when `users.mute` is delegated |
| Ban account | `users.ban` and `blocks.manage` | Persistent account of strictly lower role. Self is refused. Guest is not an account |
| Ban CID | `users.ban` and `blocks.manage` | A CID mapped to an account uses the account rule. An unmapped CID has no role target |
| Ban IP address | `users.ban` and `blocks.manage` | Exact address only. There is no role target, so the block can include staff addresses |
| Revoke ban | `users.ban` and `blocks.manage` | The actor may revoke a record they created. Otherwise the creator's account must be strictly lower. An account or mapped-CID subject must also still be strictly lower |

Moderator and higher roles receive kick and mute. Operator and higher roles
receive both ban permissions together. Owner is included and does not bypass
the target rule. Global feature switches, flood limits, ADCS policy and
connection limits still apply.

Session checks keep their existing order. Kick reports a missing or unready
target as unavailable before the permission result, except a missing actor or a
self target, which stay forbidden. Mute still refuses a missing permission
before it reports an unavailable target. Ban maintenance still reports a missing
account as not found, a missing creator as an invalid record, and a refused
rank as forbidden, all before password proof is accepted.

## Call sites

- `KickCore` and the private `+kick` command
- `MuteCore` and `+mute` / `+unmute`
- `LoginHub::revoke_auth_session_as`
- Ban create and revoke, including mapped CID and creator rank

`can_revoke` and `can_mute` use the same decision, with no separate identity, so
self remains the caller's check there.

## API and WebAdmin

Authenticated status and `--show-configuration` add an optional
`configuration.moderation` array. Each entry names the operation, permission,
subject, target rule, and whether a guest session is allowed, self is forbidden,
VIP delegation is possible, and `blocks.manage` is also required. The array
describes the fixed rules. The configured VIP list remains `configuration.roles`.
No account, nickname, address or secret is included.

Older clients ignore the field. A malformed array clears the WebAdmin snapshot
instead of showing a weaker rule. The bilingual Moderation page is read-only.
It does not choose an actor or submit kick, mute or ban.

## Not in this increment

Persistent or timed mute, a live `+ban` command, subnet or hostname bans, an
audit browser, and any change to who may already kick, mute or ban are not
implemented. Publishing the table does not create those engines.

## Validation

Native tests cover the seven-role matrix for kick, mute, unmute, account bans,
mapped and unmapped CID bans, address bans and revocation, including self,
guest, VIP delegation, the wrong subject and an invalid VIP mask. The decision
does not allocate. Existing kick, mute, role and ban suites keep their outcomes.
Local commands use eight CPU threads. GitHub-hosted Ubuntu 24.04 runners keep
four logical CPUs, checked with `nproc`, and derive parallelism from `nproc`.
Remote CI completion is not awaited.
