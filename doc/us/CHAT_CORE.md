# Chat Core — alpha-0.4.2

**Current increment — alpha-0.4.5:** [Message Routing](MESSAGE_ROUTING.md) centralizes MSG scope selection and bounded queue delivery. Plans are bound to their originating Chat Core domain and live session IDs; private intent never falls back to public delivery.

**Increment delivered in alpha-0.4.4:** [Private Chat](PRIVATE_CHAT.md) adds DMSG target-only and EMSG target-plus-sender delivery, deduplicated self-messages, lifetime-bound targets and PM origin checks. Public chat remains isolated; groups/history/moderation remain separate stages.

**Increment delivered in alpha-0.4.3:** [Public Chat](PUBLIC_CHAT.md) adds a dedicated BMSG policy/dispatch adapter and one immutable shared public frame per publication. Recipient offsets and logical queue charges remain independent; source/state/private-message gates and FIFO are preserved.

Based directly on main `508b43df7316ff1a45a41374ca0a38ce2a0ab6f0`, containing the merged alpha-0.4.1 ADC MSG codec. Existing code/history and branches are retained. EN/US UTF-8 remains the server default; documentation and operator help also support PL.

## Scope and module boundary

`dc24h_chat` is a separate C++20 library with `include/dc24h/chat/core.hpp` and `src/chat/core.cpp`. It depends on Users and shared text validation, with no ADC, sockets, TLS, logging, database, clock or persistent queue dependency. ADC depends on Chat, so there is no circular library dependency. LoginHub owns the engine beside the session registry and supplies the transport output adapter.

This first handler delivers public BMSG to all currently ready users, including the sender, and preserves optional third-person `/me` metadata. This follows the [ADC broadcast and MSG rules](https://adc.sourceforge.io/ADC.html). It introduces no private/group conversation, direct/echo/feature/hub-chat handler, history, moderation, account authorization or new chat-rate settings. Existing input/frame/security quotas and output caps apply. AW2 exclusion is optional in ADC and is not applied here; all ready users receive public chat.

## Admission and owned plans

`Core::prepare(sender, source, text, third_person)` returns an owning `Broadcast` or typed `Error`. It requires an existing ready session with a public identity and exact source-key match. Accepted, negotiating, identified/welcome-pending, closing, closed and missing senders cannot publish. Source validation is repeated at dispatch, so a removed or closing sender cannot use an old plan.

Text is nonempty already-NFC UTF-8, with decoded LF permitted and other raw ASCII controls/DEL rejected. It is not trimmed, silently normalized or interpreted as commands. The neutral text ceiling is 16384 decoded bytes; ADC retains its stricter 16384-byte encoded whole-frame limit including header/separators/escaping. Shared `text::valid_chat_text` has the same text-character policy as the existing ADC codec. Newlines and backslashes are application content, never extra wire frames.

A Broadcast owns sender lifetime ID, source key, decoded text, optional action flag and an ordered snapshot of recipient lifetime IDs. It copies no CID, nick, profile, peer address, credentials or unknown ADC fields. Recipients are selected from paged SessionManager membership (256 IDs/page), filtered to ready and bounded by registry capacity. A plan is immutable through its public interface and survives caller-string/model removal. It is a point-in-time admission snapshot, not history or permission to publish after the source closes.

## Dispatch and lifetime safety

`Core::dispatch(plan, sink)` rechecks the source, then resolves each recorded lifetime ID immediately before calling the sink. Removed/closing/unready recipients are skipped; later joins are not retroactively added. A reused protocol SID belongs to a new lifetime ID and cannot inherit an old recipient entry. Sender rename preserves source SID and does not invalidate an otherwise live plan.

The trusted sink returns whether it accepted one message into the recipient output queue. DispatchReport distinguishes queued, unavailable and refused counts. Queued means accepted for output, not that the client received or read the text. A refused recipient does not stop other recipients. Dispatch itself allocates no storage after the sink has been constructed. Sink exceptions propagate after possible partial delivery; external adapters must not throw or retry a partially published plan. There is no network-wide atomic fanout, retransmission, acknowledgment, persistent message ID or exactly-once guarantee across caller retries.

The engine is confined to one reactor thread. Registry queries expose only const models, and the sink may close recipients without invalidating traversal because no live container iterator or borrowed model is retained across callback calls. Once dispatch starts, a sink failure closing the sender does not cancel the already admitted current publication to remaining recipients.

## ADC adapter and public privacy policy

After existing direction/NORMAL/source gates, LoginHub delegates MSG to the Chat path. Only BMSG without PM is supported for public delivery. DMSG, EMSG, FMSG, HMSG and any valid PM-marked BMSG receive recoverable ISTA 125 with the offending FOURCC. They are never converted to public messages. No unsupported private text is relayed or echoed to other users. Malformed PM/ME, source forgery and state errors retain existing fatal STA/own-QUI behavior.

The adapter validates MSG using alpha-0.4.1, prepares the Chat plan and builds a fresh public ADC frame from source, text and optional ME only. Unknown extension fields, including PD-like fields, are omitted. ME decimal spelling is canonicalized to ME0/ME1 while absent/explicit false remain distinguishable. User-authored message text itself is preserved. Projection/serialization completes before fanout begins, and each enqueue still checks the existing encoded client/global output limits.

Only ready recipients are queued. A welcome-pending user receives its welcome/list normally and no old chat backlog. Existing FIFO ordering preserves public INF changes before later chat from the same reactor event sequence. A closing recipient receives no ordinary chat after terminal processing begins. Per-recipient output-cap refusal marks that recipient for immediate output_limit cleanup without changing its pinned front buffer. Healthy recipients continue. Allocation failure in MSG state validation or prepublication preparation closes the sender with internal_error; a queue-copy/allocation failure closes only that recipient and fanout continues. Existing disconnect ownership reclaims charges, SID/CID/nickname reservations and transport resources. These guards do not claim complete parser/process-wide OOM recovery.

Messages are not logged, persisted, exported through the administrative API or shown in WebAdmin. Existing numeric traffic/session statistics still apply; no new API JSON/configuration/SQL/dependency is added. US/PL help now accurately describes public chat support. Server/WebAdmin metadata is alpha-0.4.2.

## Validation

`chat.core` tests protocol-independent sender/text validation, owned plans, ordered 270-user pagination, every non-ready phase, late joins/removals, callback refusal, immutable lifetime selection, rename, stale source, allocation rollback and allocation-free dispatch with an existing sink.

`chat.adc` tests projection/privacy, pending welcome and closing exclusion, PM-marked broadcast refusal, bounded slow-recipient queues, pinned-output stability, charge cleanup and transient allocation campaigns spanning state validation, preparation and isolated fanout.

`chat.process` exercises real mixed ADC/ADCS Unicode/action chat in both directions, field sanitization, INF/chat FIFO, unsupported private/group/feature/hub modes, syntax recovery, no pre-ready backlog, reconnect, spoof rejection, and log/API privacy. Actual ncdc ADC/ADCS tests verify incoming public messages on screen, outgoing delivery to an observer, `/me`, own echo, reconnect and unsupported private messages; plaintext MSG codec roundtrip coverage remains.

Earlier tests using recoverable unimplemented responses as queue fixtures now use HMSG, preserving their original disconnect/queue intent. Public-chat assertions replace the old BMSG-not-implemented expectations. Full server and WebAdmin regressions remain required. Production load and all supported OS/client combinations need separate validation.
