# Private Chat — alpha-0.4.4

**Current increment — alpha-0.4.6:** [Message Flags](MESSAGE_FLAGS.md) centralizes PM/ME decoding and canonical output for the codec and both chat adapters. Absent ME, ME0 and ME1 remain distinct; PM never grants delivery authority.

**Increment delivered in alpha-0.4.5:** [Message Routing](MESSAGE_ROUTING.md) centralizes MSG scope selection and bounded queue delivery. Plans are bound to their originating Chat Core domain and live session IDs; private intent never falls back to public delivery.

Built directly from main `b7aff0bb6a886b96e31dbf63321271861b351d88`, after alpha-0.4.3 Public Chat. Earlier branches/history and working public chat remain preserved. EN/US UTF-8 remains the server default, with mirrored PL documentation and updated US/PL operator help.

## DMSG, EMSG and PM policy

The [official ADC routing and MSG specification](https://adc.sourceforge.io/ADC.html) defines direct messages to the target and echo messages to target plus source. This increment supports both through `adc::PrivateChat` and protocol-independent directed plans in Chat Core.

| Input | Selected recipients |
| --- | --- |
| `DMSG source target text` | Target only |
| `EMSG source target text` | Target, then source |
| DMSG/EMSG with source=target | That session once |

There is no broadcast fallback. Only existing fully ready lifetimes may send or receive. Pending welcome, anonymous, closing, closed and missing targets are unavailable. Public BMSG without PM continues through PublicChat; BMSG with PM, FMSG/HMSG and private groups remain unsupported. AW flags do not prevent a ready user receiving targeted private traffic.

PM is optional for directed routing. If present, it must equal the source SID for an ordinary private conversation; it is preserved as PM<source>. A different valid PM group returns recoverable ISTA 125 without sending text to anyone. Missing PM remains absent, allowing targeted non-PM MSG without inventing conversation metadata. This is routing to selected users regardless of how a client chooses to display an unflagged MSG. Malformed/duplicate PM/ME retain the existing fatal MSG-validation path.

Text, Unicode/NFC, whitespace, escaped LF/backslash and optional third-person metadata retain the shared codec policy. `/me` works in private conversations. The adapter constructs a fresh frame from type/source/target/text and optional PM/ME only; unknown fields, including PD-like metadata, are removed. ME is canonical ME0/ME1, with absent versus false preserved. No user-authored text is truncated, trimmed or treated as a hub command.

## Target lifetime and Core API

`chat::Directed` is an owning immutable plan with sender ID/key, target lifetime ID/key, text, action flag and a fixed two-element ID array. Its recipient span contains one or two entries, with self-echo deduplicated. `Core::prepare_direct` checks source readiness/binding, shared text rules and exact SID-indexed target readiness. Recipient lookup is bounded O(log n), with no whole-user-list scan.

`Core::dispatch(Directed, DirectedSink)` rechecks source and the original target lifetime plus key before any queue callback. A removed target cannot be replaced by a new session using the same SID; a stale plan returns recipient_unavailable and emits no echo. Target rename preserves a live SID/lifetime binding. Each selected lifetime is checked again at callback time, and no unrelated user can enter the plan. The Core has no ADC/socket/TLS/SQL/logger/history dependency.

DispatchReport counts queued, unavailable and refused queue attempts. Echo is a source copy, not a network delivery/read acknowledgment. After a valid dispatch starts, a target queue refusal does not cancel the independently selected sender echo; a sender refusal does not cancel an already queued target. No atomic cross-socket delivery or automatic retry is provided. Queue callbacks must isolate their exceptions; blindly retrying a partially published plan can duplicate output.

## ADC adapter and errors

`include/dc24h/adc/private_chat.hpp` and `src/adc/private_chat.cpp` define PrivateChatError, PrivateDelivery and immutable PrivateChatFrame. `prepare` validates hostile hand-built Message objects too, applies D/E/PM policy, obtains a Directed plan, projects allowlisted metadata and serializes one complete frame before queue mutation. A shared const packet is used for the target and optional echo. Moved-from plans are rejected. Validation/allocation failure cannot partially register or mutate users.

| Condition | Live result |
| --- | --- |
| Missing/not-ready target at admission or before dispatch | Recoverable ISTA 140, fixed description and FCDMSG/FCEMSG; no content/echo |
| Valid unsupported group PM | Recoverable ISTA 125 with offending FOURCC; no content |
| Wrong source / malformed known fields | Existing fatal ISTA 240, own IQUI, bounded cleanup |
| Sender before NORMAL or before welcome completion | Existing fatal ISTA 244 |
| Preparation allocation failure | Sender internal_error/immediate cleanup without publishing text |
| Recipient queue limit/allocation failure | That recipient closes; selected other recipient may continue |

Unavailable errors do not reflect text, peer address, target key, group or credentials. There is no offline mailbox, group membership, moderation/history or registered-account permission policy in this stage.

## Queue ownership, limits and privacy

LoginHub now stores ordinary owned frames, shared public packets or shared private packets in one tagged queue. A common typed enqueue helper applies the existing per-client/global logical byte caps, FIFO and immutable pinned-front rules to both chat modes. Every selected recipient pays its full frame charge independently, including partial sends. DMSG charges one recipient; EMSG charges one or two depending on self-deduplication. Completion, tail cancellation and disconnect reclaim charges once, while shared physical storage lives until its last owner releases it.

Each queue retains its own offset. Source disconnect cannot invalidate a target's queued packet, and a reused SID cannot redirect old queued content to a new transport token. Identified/pending users never receive private backlog. Private/public/status/user-list frames preserve recipient FIFO. Existing outgoing completion validation remains in place. Frame bodies remain capped at 16384 encoded bytes excluding LF, with all headers, flags, separators and escaping counted; the PM-bearing D/E prefix leaves less room for text than BMSG. No new limits, configuration options, dependencies, SQL migrations, API JSON fields or WebAdmin features are added.

Private means selected-session routing through the hub. The hub handles the text in memory to relay it. Message bodies and unknown fields are not logged, persisted, copied into unrelated queues or exported through API/WebAdmin. Existing numeric traffic/session statistics remain available. Tests cover the application paths described here; complete process-wide OOM recovery, remote CI and production performance are not claimed.

## Verification

`chat.private` covers D/E recipient sets/order, shared packet identity, self-deduplication, optional PM, group/source/state errors, exact encoded bounds, immutable plan moves, target SID reuse, rename, stale source, allocation-free dispatch with an existing sink and preparation rollback. Queue tests exercise partial offsets, source cleanup, slow-target caps/pinned output, independent echo, pending-welcome exclusion and transient per-recipient allocation failures with an unaffected observer.

`chat.private.process` uses mixed actual ADC/ADCS clients plus an unrelated observer and a pending client. It verifies D target-only via ordered public fences, E copies, self-once, Unicode/newlines/actions, unknown/private-field sanitization, unavailable/group refusals, unflagged direct routing, no pending backlog, exact 16 KiB frame bodies, disconnect/reconnect, stale SID refusal, source forgery and log/API privacy.

Real ncdc ADC/ADCS performs `/msg` to a live observer, receives a private reply on screen and sends private `/me`; public chat, user-list/INF/reconnect/statistics and plaintext MSG codec roundtrip remain covered. Full server and WebAdmin regressions remain required before publishing. Other OS/client combinations, measured load, groups, offline delivery, roles, moderation and history remain separate work.
