# ADC State — alpha-0.2.3

**Current increment — alpha-0.4.5:** [Message Routing](MESSAGE_ROUTING.md) centralizes MSG scope selection and bounded queue delivery. Plans are bound to their originating Chat Core domain and live session IDs; private intent never falls back to public delivery.

**Increment delivered in alpha-0.4.4:** [Private Chat](PRIVATE_CHAT.md) adds DMSG target-only and EMSG target-plus-sender delivery, deduplicated self-messages, lifetime-bound targets and PM origin checks. Public chat remains isolated; groups/history/moderation remain separate stages.

**Increment delivered in alpha-0.4.3:** [Public Chat](PUBLIC_CHAT.md) adds a dedicated BMSG policy/dispatch adapter and one immutable shared public frame per publication. Recipient offsets and logical queue charges remain independent; source/state/private-message gates and FIFO are preserved.

**Increment delivered in alpha-0.4.2:** [Chat Core](CHAT_CORE.md) adds a separate protocol-independent message engine and live public BMSG delivery, including sender echo and /me. Private/group/feature/hub chat, moderation and history remain separate increments.

**Increment delivered in alpha-0.4.1:** [ADC MSG](ADC_MSG.md) adds a dedicated MSG codec with typed PM/ME views, checked builders, bounded serialization and live state-machine validation. Chat delivery and conversation policy remain later increments.

**Increment delivered in alpha-0.3.12:** [Limits](LIMITS.md) adds configurable session/user ceilings shared by ADC/ADCS, bounded by the connection cap. SessionManager checks capacity transactionally; LoginHub rejects a full user registry before welcome/list preparation with ISTA 211 and own IQUI. Closing reservations count until cleanup. Earlier descriptions retain their historical scope.

**Increment delivered in alpha-0.2.10:** [Negotiation security](NEGOTIATION_SECURITY.md) adds pending admission caps, cumulative pre-readiness input budgets and shared pre-INF/Tiger attempt limits. Earlier release descriptions below retain their original scope.

**Increment delivered in alpha-0.2.9:** [STA/QUI](ADC_STATUS.md) adds semantic status/quit validation, source-checked B/D/E/F status routing, precise errors and bounded terminal ISTA/own-IQUI output. Historical descriptions below retain the behavior of their original releases.

**Integration delivered in alpha-0.2.8:** [Guest login](ADC_LOGIN.md) attaches the state/SUP/SID/INF/Tiger stack to the actual C++ TCP/TLS reactor with bounded ordered output, nickname/CID admission, public presence and complete ncdc NORMAL/reconnect tests. Historical stage descriptions below retain their original scope; statements that transport attachment/output/login were still planned describe those earlier versions. Registered-user authentication, chat, search and transfer dispatch remain planned.

Since alpha-0.2.7, [CID/PID verification](ADC_IDENTITY.md) is a dedicated shared module with TigerCid/VerifiedIdentity values, direct cleansed output buffers and a constant-time final digest comparison. INF snapshots retain only a public verified result. Basic verification introduced in alpha-0.2.6 remains compatible; live transport attachment and account/registry policy remain separate.

Since alpha-0.2.6, [ADC INF](ADC_INF.md) supplies ClientSession, combining this lower-level stack with intrinsic INF validation and Tiger PID/CID verification. It requires validated metadata before external identity admission and never retains/exports PD. Registry uniqueness and actual Network attachment remain planned; older APIs/fixtures retain their original scope.

Since alpha-0.2.5, [ADC SID](ADC_SID.md) supplies a shared hub pool and SidSession composition with checked ISID ownership and automatic reservation cleanup. Parser/state SID syntax uses the same codec; AAAA remains valid syntax but is reserved by the pool as local policy. The original lower-level APIs and historical fixtures remain available. Network still uses only the parser.

Since alpha-0.2.4, [ADC SUP](ADC_SUP.md) supplies `ProtocolConnection`, composing this low-level machine with feature/hash negotiation and automatic protocol approval only after valid SUP. Identity/password decisions remain external. This document describes the original standalone state API; Network attachment still requires ordered output.

## Scope and ownership

`dc24h/adc/state.hpp` adds an independent hub-side `ConnectionStateMachine` to `dc24h_adc`. It consumes parsed messages and explicit application decisions. It does not send bytes, allocate global SIDs, negotiate hashes, validate identities or authenticate passwords. Network still invokes only the parser: no live login or new wire behavior is delivered by this stage. Existing real ncdc ADC/ADCS tests remain initial-HSUP transport checks.

The model follows the hub login ordering in [ADC 1.0.4, sections 5.1 and 6.1](https://adc.sourceforge.io/ADC.html). BASE hub connections have no DATA state; client-to-client binary transfers and hub-transfer extensions are outside this profile. `disconnected` is an additional local terminal state for transport/application failures.

Use one noncopyable, nonmovable instance per future connection, on its owning reactor thread. There are no sockets, locks, callbacks, loggers, clocks or database dependencies. Storage is one initially empty/finally four-character SID and six fixed flags. No command payload, PID, password response, nickname or feature set is retained. `sid()` returns a borrowed view; do not retain it beyond the instance lifetime or SID assignment. It is a local binding, not proof of global SID uniqueness.

## Ordered transitions

| Current state | Accepted event / decision | Result |
| --- | --- | --- |
| PROTOCOL | Client HSUP | Mark negotiation pending; remain PROTOCOL |
| PROTOCOL | `approve_protocol()` after successful external checks | Permit hub reply; remain PROTOCOL |
| PROTOCOL | Completed hub ISUP, then completed ISID | Bind SID and enter IDENTIFY |
| IDENTIFY | Optional hub IINF | Remain IDENTIFY |
| IDENTIFY | Client BINF with this SID | Mark identity pending; remain IDENTIFY |
| IDENTIFY | `approve_identity(guest)` | Enter NORMAL |
| IDENTIFY | `approve_identity(password_required)` | Enter VERIFY |
| VERIFY | Completed hub IGPA, then client HPAS | Mark password pending; remain VERIFY |
| VERIFY | `approve_password()` after successful external verification | Enter NORMAL |
| Live state | Keepalive or nonfatal peer/hub STA | Remain in current state |
| IDENTIFY / VERIFY / NORMAL | Completed IQUI naming this connection's SID | Enter DISCONNECTED |
| Live state | Peer/hub fatal HSTA/ISTA, or `disconnect()` | Enter DISCONNECTED |

Approvals are trusted application decisions, not validators. The caller must check feature/hash compatibility before protocol approval, PID/CID and required fields/IP/nickname/CID uniqueness before identity approval, and the challenge response against the selected account before password approval. No test approval is an implemented authentication service. Reject failed checks with a sanitized response when output is available, then disconnect. There is no reject-to-NORMAL path, timeout renewal, automatic retry or reset. A new connection needs a new instance.

SUP, approval, reply, initial INF, challenge and password replays are rejected while their step is pending/completed. INF and PAS alone never advance the state. A success STA never substitutes for any approval. NORMAL SUP updates and INF changes pass sequencing checks but still require an external semantic consumer. IQUI about a different user and routed fatal STA do not close this connection.

## Supported command profile

`observe(Direction, const Message&)` returns `StateResult` (`optional<StateError>`). Empty means accepted; errors are fixed categories: disconnected, invalid_message, invalid_type, invalid_state, invalid_sequence, wrong_source and unsupported_command. Errors leave all stored state unchanged, allowing the owner to choose error reporting/closure. This is not a recommendation to continue indefinitely after malicious input. In allocation failure, ISID storage is assigned before changing the state; allocation exceptions propagate.

| State | Commands |
| --- | --- |
| PROTOCOL | STA, SUP, outbound SID |
| IDENTIFY | STA, INF, outbound QUI |
| VERIFY | STA, outbound GPA, inbound PAS, outbound QUI |
| NORMAL | STA, SUP, INF, MSG, SCH, RES, CTM, RCM, outbound QUI |
| DISCONNECTED | None, including keepalives and approvals |

Direction/type policy is an explicit BASE hub subset, not complete extension authorization:

- Inbound SUP/PAS use H; outbound SUP uses I. SID/GPA/QUI are outbound I only; PAS is inbound only.
- Inbound INF uses B; outbound INF uses I or B. Before identity approval only outbound IINF is permitted.
- STA/MSG accept B/D/E/F and H inbound or I outbound. Before NORMAL status uses HSTA/ISTA only.
- SCH/RES/CTM/RCM accept B/D/E/F; permission to route, target existence, active/passive policy and per-field semantics remain the future dispatcher's responsibility. Passing the gate does not execute a command.
- Every inbound B/D/E/F source must equal this connection's assigned SID. Outbound relays may name other users. Client-to-client C, UDP U and inbound hub I types are rejected.
- Unknown commands and GET/GFI/SND return unsupported_command without side effects. The parser still preserves their syntax; negotiated extension dispatch and binary framing require separate design.

Only parser-produced messages are valid inputs. The module additionally guards command/kind mismatch and SID/STA/QUI positional accesses, but does not revalidate all text or arguments of hand-built `Message` objects. Pass `KeepAlive` to `keepalive()`; handle `ParseError` through the existing invalid-frame policy without calling `observe`. A keepalive never authorizes or advances login.

## Future transport integration

`to_client` represents a completed ordered send, not queue insertion. Before sending, the future dispatcher must validate outbound messages against the state and enforce bounded output. Commit state events in send order; disconnect after partial-send failure. Never observe ISID/GPA/QUI when merely enqueued. Process inbound results and send completions serially to avoid moving past an incomplete reply; early pipelined commands must not skip gates. This release provides no output preflight API or transport adapter.

Integration also requires a unique SID allocator, feature/hash and identity services, password design, PD removal before forwarding, bounded client lists, handshake deadlines, dispatch and output queues. The existing receive-only transport cannot correctly complete these transitions, so attaching an enforcing state object to it is deferred. API counters continue to mean accepted syntax, not authorized commands or logged-in users.

## Verification

`adc.state` uses release-active checks for both login paths, a command/state/direction matrix for all 15 BASE commands plus an unknown extension, premature/repeated approvals and replies, wrong sources across B/D/E/F, per-instance isolation, status/QUI effects, terminal rejection and parser-to-state stream order. It covers early pipelining, malformed syntax recovery and bytewise UTF-8 INF. Existing ncdc tests exercise the actual receive-only server separately; they do not demonstrate the state machine attached to Network or full ncdc login. See [TESTING.md](TESTING.md).
