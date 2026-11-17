# Kick — alpha-0.8.4

**alpha-0.8.5 Ban Core:** [Persistent account/CID/IP admission blocks](BAN_CORE.md) add explicit schema v8, proven create/revoke, startup enforcement and expiry. Admin Core now has ten maintenance actions; users.ban/blocks.manage are implemented. This supersedes earlier schema-v7/ban-pending statements.

Authenticated staff can now kick a live user from an ADC/ADCS client by sending
ordinary public-chat text:

```text
+kick <nickname> [reason]
```

The exact lowercase prefix must start the message. Use the exact current nickname;
matching follows the hub's existing nickname registry. A missing reason uses
`Kicked from hub`. A reason is at most 256 UTF-8 bytes, already NFC, without
controls, formatting characters or line/paragraph separators. The command is a
hub-specific chat convention, not a new ADC command. Private DMSG/EMSG content is
never interpreted as an administration request. Other command flags, including
ME/PM, are refused for the reserved public prefix. Normal chat without that prefix
keeps its existing behavior. There is no browser or HTTP kick mutation endpoint.

## Authority and live Admin Core

`admin::KickCore` is a protocol-neutral, allocation-free live policy engine. It
borrows the reactor's SessionManager and startup RolePolicy. It checks the actual
ready authenticated transport actor, `users.kick`, a ready target and strictly
lower target rank. Moderator/Operator/Administrator/Owner have the default grant;
VIP requires explicit `[roles] vip_permissions=users.kick` delegation and can then
kick only lower Guest/Registered targets. Guest and Registered cannot kick.
Self, equal/higher roles, missing, pending, closing and stale lifetime IDs are
refused. Unknown roles/policies deny. Backwards timestamps never change the target.

The trusted in-process `LoginHub::kick(actor_lifetime, target_lifetime, reason, now)`
uses lifetime IDs, not fd numbers or an assumed account/SID identity. The chat
adapter resolves the nickname once to the current target lifetime and binds the
actor to the actual receiving connection. Protocol state, source SID and known
MSG fields are checked first; spoofing another sender fails before kick dispatch.
Role grants originate in GPA/PAS account authentication, never INF client flags.
The existing shared MSG token buckets apply before command dispatch, including
malformed/denied commands. Ordinary public-chat enablement does not disable this
separate administrative capability. No ordinary chat text is published as a fallback.

The synchronous maintenance Admin Core retains its eight account/ownership actions
and stopped-hub rules. Live kick uses the reactor-specific core in `dc24h_moderation`;
no SQL, KDF, mutex, API worker or cross-thread session mutation enters the reactor.
Account/role changes still require stopped serving instances and restart, as with
the existing immutable authentication directory. No hot privilege edits are added.

## Termination and privacy

Before changing the target, the adapter validates the bounded reason and prepares
its terminal messages. KickCore then rechecks authority and begins drain-close
with `CloseCause::kicked`. Authentication is removed and readiness is lost at
that point; the adapter immediately forgets all target-owned search windows.
Chat/Search/Transfers stop accepting or routing to that session before transport
cleanup. Account credentials, identity mappings and SQL rows are untouched.

The target receives a fatal `ISTA 230` followed by its own final `IQUI`, with the
initiator `ID`, private reason `MS` and `TL0`. The hub sends no data after that
IQUI. This uses the [ADC STA/QUI definitions](https://adc.sourceforge.io/ADC.html#_qui).
Kick is not a ban: TL0 imposes no reconnect delay, and a fresh normal GPA/PAS login
is allowed with unchanged credentials. There is no ban, unwanted-transfer flag,
redirect, persistent block or automatic account disabling.

A partially sent/TLS-pinned front keeps its original address and remaining length;
later queued ordinary frames are dropped. Terminal frames follow the retained
front under existing per-client/global queue caps and close/output deadlines.
Preparation allocation failure leaves the target unchanged. After close commits,
queue/allocation failure forces bounded immediate closure; the kick remains
successful and a notice-failure counter records the missing notification. It is
never retried. Slow clients cannot keep routing while terminal output drains.

The actor receives only a private success/refusal status. Commands and reasons do
not fan out to public chat, including malformed/denied reserved commands. Other
users receive exactly one ordinary bare `IQUI <sid>` on physical disconnect; the
reason and initiator are not copied into their departure notice. Connection
logging uses the fixed `event=kicked` identifier, without command text, nickname,
reason, account proof or password. Existing physical cleanup releases indexes,
SID, output references and capacity. A repeated request cannot kick a closed
lifetime or publish another terminal/departure notification.

## Status API and WebAdmin

Authenticated `GET /api/v1/status` adds an aggregate `kick` object. Its five fields
are unsigned 64-bit decimal strings (saturating rather than wrapping):

| Field | Meaning |
| --- | --- |
| attempts | Requests reaching kick dispatch, including syntax/authority refusals |
| succeeded | Sessions moved out of ready state by kick |
| refused | Syntax, authority, target or time refusals |
| errors | Preparation resource failures, before any kick commit |
| notice_failures | Successful kicks whose terminal notification could not queue |

The first four describe terminal outcomes; notice_failures is an additional subset
of successful kicks. Protocol-invalid input and shared flood denials happen before
kick dispatch and are excluded. Reserved kick commands are excluded from ordinary
chat dispatch counters. Counters are process-local, reset on restart and sampled
independently across threads; no transactional sum is promised in one API sample.
No IDs, nicknames, reasons, histories or command payloads appear in the DTO.
Maintenance mode reports zero live counters, not a functioning live kick endpoint.

WebAdmin's bilingual Kick page shows instructions and these read-only counters.
The adapter/browser accept optional counters for older APIs, preserve uint64
precision, validate types/ranges and strip unknown fields. Invalid/offline snapshots
clear values using the existing polling rules. Browser users cannot supply a
privileged actor ID to kick on someone else's behalf. Use an authenticated Direct
Connect client for the actual operation.

## Validation and remaining scope

Native tests cover role hierarchy, VIP delegation, self/upward/Guest refusal,
pending/backwards/stale targets, sender spoofing, flood limits, private command
consumption, disabled ordinary chat, search cleanup, pinned output, allocation
failures and queue-cap fallback. Real TCP/TLS tests verify private terminal fields,
exactly one public departure, no secret logs, counters and fresh GPA/PAS login.
An actual ncdc victim displays the kick reason in both ADC and ADCS. Node/Chromium
cover optional DTO parsing, redaction, integer precision and Polish mobile UI;
real C++/adapter/React integration also verifies a refused guest kick counter.

Persistent bans, mute, audit history, a remote administrator session gateway and
browser kick controls remain future increments. Schema v7, migrations and manifest
are unchanged. Local builds/tests use eight CPU threads. GitHub CI retains Ubuntu
24.04 with four logical CPUs per runner and nproc-derived parallelism. Its remote
completion is not awaited. No production deployment or account mutation is done.
