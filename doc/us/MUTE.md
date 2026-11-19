# Mute — alpha-0.8.7

**alpha-0.8.8 Moderation Rules:** [Shared permission and target checks](MODERATION_RULES.md) decide kick, mute, unmute and account, CID and IP bans, including revocation. Schema v8 is unchanged.

This increment adds live public/private chat moderation on top of
[Temporary Bans](TEMPORARY_BANS.md). It starts from main
`bcbba253074ad7dbba8aa75752c51b1e68767475`. There is no automatic merge.
The protocol-neutral Admin Core policy, session model and chat engine enforce the
change; the ADC adapter exposes commands to authenticated clients. SQL schema **v8**,
all migration files and the manifest remain unchanged.

## Commands and authority

Send one ordinary public BMSG from your authenticated Direct Connect client:

```text
+mute <nickname> [public|private|all]
+unmute <nickname> [public|private|all]
```

Names are exact and case-sensitive; this token syntax supports nicknames without
spaces. Default scope is `all`. Public and private scopes are independent: muting
public then private enables both; unmuting public leaves private muted. Repeated
operations are idempotent and count as successful operations. There is no duration
or reason argument in this stage. Unknown scopes, extra tokens or MSG flags are
refused privately. Commands in DMSG/EMSG are ordinary private text and do not invoke
administration. Prefixes such as `+muted` remain ordinary chat.

The actor is the real current connection lifetime, never a client-supplied actor ID.
It must be ready and authenticated, have `users.mute`, and have a strictly higher
role than the target. Moderator, Operator, Administrator and Owner have this
permission. VIP requires explicit `roles.vip_permissions=users.mute` and can affect
Guest/Registered only. A `users.kick` grant alone does not grant mute, or vice versa.
Self/equal/higher targets, missing/closing sessions and invalid time/scope are
refused. Unknown roles and invalid policy masks fail closed. Both mute and unmute
use the same authority checks; any authorized higher-role actor may clear a target's
mute, regardless of who applied it. Muted staff retain authorized moderation access.

State/source/MSG checks and the existing shared message/byte flood guard run before
command dispatch. Valid, malformed and denied reserved public commands are consumed
privately and excluded from ordinary chat counters; flood-limited commands never
reach the moderation policy. Actor acknowledgements do not include the target name,
message content or command text. Commands never enter public fanout.

## Enforcement and lifetime

| Scope | Blocked outgoing traffic |
| --- | --- |
| public | BMSG, including ME0/ME1 action variants and sender echo |
| private | DMSG/EMSG, including PM, action, self-target and private echo |
| all | Both scopes |

The session owns two fixed flags; no separate unbounded map, timer, account mutation
or SQL write is introduced. `MuteCore` checks authority and commits through
`SessionManager` without allocation. Target readiness and monotonic transition time
are checked before mutation. The chat engine checks flags during both preparation
and dispatch; an already prepared plan cannot bypass a later mute. Private denial
happens before destination lookup and produces no echo or recipient availability
information. Blocked traffic remains subject to existing flood/output limits.

Mute affects sending. Incoming public/private chat, hub notices, authenticated role,
presence, search and transfer permissions remain available. Generic ISTA 125 reports
a muted operation without disconnecting the sender. The target receives a fixed
scope notice (ISTA 000) and the actor a private success/refusal. Notices are prepared
before commit; allocation failure there leaves state unchanged. A post-commit notice
failure does not roll back a successful change or permit automatic replay. Existing
queue limits may close an overflowing peer. No mute subject, command or message text
is added to logs or the status API.

State follows the session lifetime through a guest nickname update. It clears when
the session begins closing and does not transfer to a replacement connection, SID,
CID or account session. Disconnect, reconnect and hub restart therefore end mute.
Persistent account mutes and timed mutes are future work. Mute does not cancel frames
already placed in recipient queues before commit; those may still arrive. No live
HTTP moderation endpoint or browser-selected privileged actor is added.

## Status API and WebAdmin

Private GET `/api/v1/status` adds optional-on-read `mute` aggregate counters. Each is
a canonical decimal string bounded by UINT64_MAX, saturates instead of wrapping,
and resets on server restart:

| Counter | Meaning |
| --- | --- |
| attempts | Moderation operations reaching dispatch, including syntax refusal |
| applied | Successful mute operations, including idempotent repeats |
| cleared | Successful unmute operations, including idempotent repeats |
| refused | Authorization, target, time, scope or command-shape refusal |
| errors | Notice preparation allocation failures before commit |
| notice_failures | Failed target notice after a committed change |
| public_blocked | Public MSG rejected by mute after validation/flood gates |
| private_blocked | Private MSG rejected by mute after validation/flood gates |

There is no current-muted-user gauge, target list or audit trail. Blocked ordinary
MSG also increments existing chat received/rejected counters, with no delivery
counts. Preparing or dispatching through the trusted protocol-neutral Core alone
does not change ADC adapter counters. Network publishes aggregate atomics; each
counter can be observed independently across a reactor update.

The EN/US and Polish Mute page documents commands and scope/lifetime behavior and
renders counters with exact BigInt precision. DTO normalization strips unknown
fields. Older servers without `mute` show unavailable counters; malformed or failed
status refresh clears stale values. The panel does not execute a moderation action.
The existing private socket/Bearer/same-UID protection and same-origin browser
adapter remain in force. Maintenance Admin Core's ten persistent actions are
unchanged; live mute is a separate authenticated session service, like kick.

## Verification

Native tests cover hierarchy and independent VIP delegation, guests/pending/closing
sessions, invalid scope/time/source, independent flags, malformed private commands,
flood gates, prepared-plan denial, no-allocation commit/dispatch, nickname/reconnect
lifetime, incoming notices/search continuity and allocation faults before/after
commit. Actual authenticated TCP/TLS tests exercise public/private denial and
restoration, no echo/leak, counters and secret-free logs. Real ncdc is muted and
unmuted on ADC and ADCS; a witness verifies suppressed content is absent and restored
chat arrives. Node/Chromium cover precision, redaction, older/malformed APIs, refresh
and Polish mobile; real C++ API → adapter → React covers the new status fields.
Local commands use eight CPU threads. GitHub CI retains Ubuntu 24.04 with four
logical CPUs per runner and `nproc` parallelism. Remote CI completion is not awaited.
