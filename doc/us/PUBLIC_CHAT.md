# Public Chat — alpha-0.4.3

**Current increment — alpha-0.4.9:** [Anti-Flood](ANTI_FLOOD.md) adds enabled per-connection B/D/E message and decoded-byte buckets, configurable in `[chat]`. Excess messages are dropped before fanout with bounded recoverable notices; existing validation and general resource limits remain.

**Increment delivered in alpha-0.4.8:** [Chat Validation](CHAT_VALIDATION.md) centralizes client text, SID and sender readiness/ownership checks in both adapters. Recipient lifetime selection and dispatch revalidation remain authoritative; system IMSG stays separate.

**Increment delivered in alpha-0.4.7:** [Hub Messages](HUB_MESSAGES.md) adds trusted, lifetime-bound system IMSG delivery and a fixed welcome after own BINF. Client text cannot originate system messages; existing queue bounds and final-LF readiness remain.

**Increment delivered in alpha-0.4.6:** [Message Flags](MESSAGE_FLAGS.md) centralizes PM/ME decoding and canonical output for the codec and both chat adapters. Absent ME, ME0 and ME1 remain distinct; PM never grants delivery authority.

**Increment delivered in alpha-0.4.5:** [Message Routing](MESSAGE_ROUTING.md) centralizes MSG scope selection and bounded queue delivery. Plans are bound to their originating Chat Core domain and live session IDs; private intent never falls back to public delivery.

**Increment delivered in alpha-0.4.4:** [Private Chat](PRIVATE_CHAT.md) adds DMSG target-only and EMSG target-plus-sender delivery, deduplicated self-messages, lifetime-bound targets and PM origin checks. Public chat remains isolated; groups/history/moderation remain separate stages.

This branch starts directly from main `165c3596d7b2488599bbe6b8297f16cd35825f28`, after the alpha-0.4.2 Chat Core merge. The MSG codec and basic public delivery already existed; this increment makes public BMSG a dedicated adapter and avoids duplicating/serializing queued public frames for each recipient. Previous branches/history remain intact. Server/development default is EN/US UTF-8 with mirrored PL documentation.

## Public BMSG contract

`adc::PublicChat` composes the alpha-0.4.1 codec with protocol-independent chat::Core. Its prepare/dispatch interface lives in `include/dc24h/adc/public_chat.hpp`; implementation is `src/adc/public_chat.cpp`. The module has no socket, TLS, SQL, logger or persistent queue ownership. LoginHub supplies the existing bounded transport queues. Core remains independent of ADC, so no library cycle is introduced.

The main room accepts BMSG without PM. Every ready user, including the sender, is selected once in local lifetime-ID order. AW2 remains a client declaration: optional exclusion is not applied. `/me` is preserved with canonical ME0/ME1, and absent ME remains distinct from explicit false. Nonempty whitespace, Unicode/NFC, escaped LF and backslash remain literal message content. The server adds no nickname prefix or command interpretation; clients render the source SID using their current user list.

This follows the [official ADC broadcast/MSG rules](https://adc.sourceforge.io/ADC.html). Public chat does not imply private/direct/echo/feature/hub chat, groups, history, moderation, permissions, offline delivery, acknowledgments or automatic retries. DMSG/EMSG/FMSG/HMSG and PM-marked BMSG retain recoverable ISTA 125 through the live adapter. Private intent is never downgraded to public. Existing source forgery, state, malformed PM/ME and syntax policies retain their STA/QUI/discard behavior.

## Preparation and typed failures

`PublicChat::prepare(sender, message)` validates the entire input, then requires the B type, absent PM and Core admission. PublicChatError distinguishes invalid_message, unsupported_type, private_message, invalid_state and wrong_source. A generic parsed Message is not proof of source ownership or readiness. Hand-built objects are checked too; unknown extension fields cannot bypass wire/text limits.

After admission, the adapter builds a fresh allowlisted Message with source, text and optional canonical ME only. Unknown fields, including PD-like metadata, are removed. It serializes the outgoing public frame once during preparation and creates a SharedPublicChatFrame. No recipient queue changes until preparation has completed. The result PublicBroadcast owns the Core lifetime snapshot and immutable shared packet. Allocation/validation failure publishes nothing and does not mutate the registry.

Input and output retain the existing application bounds: 16384 encoded body bytes excluding LF, 128 parameters and 128 feature selectors. Header/separators and every expanded escape count. The maximum queued public frame is 16385 bytes including LF. Text is not truncated, trimmed or silently normalized. No new text/flood limit, configuration key or dependency is introduced.

## Shared packet and queue ownership

PublicChatFrame can only be constructed by PublicChat and is noncopyable/nonmovable. It owns the checked sanitized Message and LF-terminated wire string, exposing only const references. PublicBroadcast holds `shared_ptr<const PublicChatFrame>` and a Core admission plan. Plan copies share packet ownership; moved-from plans cannot be dispatched and return invalid_text. The shared packet owns no session/model pointers, nickname, CID, address or credentials.

`PublicChat::dispatch(plan, sink)` uses Core to recheck the source and each recipient lifetime. The sink receives the same packet object and wire storage for every selected recipient. A recipient removed/closing before its turn is skipped, later joins get no old chat, and a reused SID cannot inherit an old lifetime entry. A missing sink returns missing_sink. A false sink result contributes to refused; queued means accepted for output, not received/read by a client. Exceptions follow Core's documented partial-publication contract; LoginHub isolates per-recipient allocation failures and never retries a partially published message.

LoginHub output frames are now a variant of ordinary owned protocol frames and shared public packets. Welcome, INF, status and QUI retain ordinary ownership and completion semantics. A public enqueue stores a shared pointer without copying Message/text or encoding another queued wire string. Each queue keeps its own byte offset, FIFO position and full logical wire charge. Existing completion-time protocol/state validation remains in place.

Sharing storage does not multiply capacity: each recipient's complete frame size is charged against both its own cap and the hub's global logical output cap. A partially sent frame stays fully charged until completion, matching earlier behavior. Completion, canceled tail frames and disconnect release each logical charge exactly once. Physical packet storage is reclaimed only when its last owner releases it. The per-queue metadata and transient plan still have bounded allocation cost; no benchmark or exact physical-memory budget is claimed.

A pinned TCP/TLS front remains at the same address through other queues' appends, partial sends, source disconnect and other recipients' completion. Closing preserves any pinned front, cancels later ordinary traffic, then appends existing ISTA/own-IQUI frames in order. One recipient's offset never mutates the shared wire or another offset. No public data is appended after that recipient's terminal QUI. Output-cap refusal and queue allocation failure close only the affected recipient while healthy recipients continue, under the existing handler semantics. Prepublication failure closes the sender; this does not claim complete process-wide OOM recovery.

## Runtime and privacy

Only fully ready users receive chat; identified/welcome-pending/anonymous/closing sessions do not acquire a backlog. INF changes and later BMSG retain reactor/FIFO order. The client chooses how to render user-authored text; no message body or unknown field is logged, persisted or exported to WebAdmin/API. Existing numeric traffic/session statistics remain available. No new API JSON, SQL migration, listener, operator message key or WebAdmin feature is added. Release metadata is alpha-0.4.3; existing US/PL operator catalogs remain applicable.

## Tests

`chat.public` covers typed policy errors, 270-recipient shared packet identity, allocation-free dispatch with an existing sink, plan ownership/moves, exact plain/escaped wire boundaries, preparation allocation rollback and shared-queue pointer equality. It checks independent partial offsets/full charges, source cleanup while other owners remain, pinned suffix followed by fatal STA/QUI and final charge release.

`chat.public.process` uses mixed real ADC/ADCS clients for ordered bursts, INF/BMSG ordering, Unicode/action and extension sanitization, AW2 inclusion, exact 16 KiB bodies, oversize recovery, private-intent refusal, public fences, escaped whitespace and log privacy. Existing chat.core/chat.adc/chat.process tests continue to cover lifetime selection, slow recipients, PM isolation, allocation faults and reconnect. The transient queue-allocation campaign now varies deque block positions because shared packet enqueues often require no allocation.

Actual ncdc ADC/ADCS now adds a public Unicode/backslash/space message to incoming/outgoing, observer delivery, `/me`, reconnect and plaintext codec roundtrip checks. Full server and WebAdmin regression checks remain required. Remote CI, the full OS/client matrix, production load and measured performance require separate evidence.
