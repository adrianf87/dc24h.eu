# Session Core — alpha-0.3.1

**Current increment — alpha-0.3.9:** [User List](USER_LIST.md) builds an owned deterministic initial welcome from paged identified/ready membership and validated public ADC metadata. The complete escaped wire size is checked before enqueue/registration. Own INF stays last; subsequent join/update/quit frames preserve FIFO synchronization. Earlier module descriptions retain their historical scope.

**Increment delivered in alpha-0.3.5:** [User Core](USER_CORE.md) adds checked UTF-8/NFC Nickname values, shared ADC/User text validation, immutable rename candidates and explicit projection from validated ClientInfo/VerifiedIdentity. Users now links the neutral dc24h_text library using the existing utf8proc package. Manager ownership/uniqueness remains unchanged. Earlier descriptions retain their historical scope.

**Increment delivered in alpha-0.3.4:** [Session Manager](SESSION_MANAGER.md) owns SID/CID/nickname indexes together with live models. Const lookups and managed lifecycle/User mutations preserve uniqueness through admission, rename and cleanup. LoginHub retains protocol and output ownership. Earlier descriptions retain their historical delivery scope.

**Increment delivered in alpha-0.3.3:** [User Core](USER_CORE.md) adds an owning public guest identity/profile to Session, with validated full-INF projection and lifecycle-guarded updates. It was developed directly from main with Session Core; main now combines it with the previously merged alpha-0.3.2 Session Manager. Earlier descriptions retain their historical scope.

**Increment delivered in alpha-0.3.2:** [Session Manager](SESSION_MANAGER.md) centrally owns all registered lifecycle models, with bounded capacity, stable lookup, phase counts and ordered snapshot pages. Protocol peers borrow models; their queues and public routing indexes remain in ADC. Earlier descriptions retain their historical scope.

The new `dc24h_users` C++20 library owns the user-session lifecycle model used by the live ADC/ADCS hub. This branch starts from main `5bc79d0a12eb37d0869651eb48b57d2bf338d7fe`, after alpha-0.2.10 negotiation security. The protocol, guest admission rules, wire replies, security settings, SQL schema and API v1 remain unchanged. Historical branches are preserved; this branch is not merged automatically.

## Identity and ownership

`users::SessionId` is a nonzero 64-bit local lifetime identity. It is independent of the ADC SID, socket descriptor, CID, nickname and account. One noncopyable SessionIds sequence belongs to the reactor; IDs increase, can skip failed setup attempts, and never wrap or reuse a prior value. Exhaustion rejects new peers while existing sessions continue. It is not a persistent UUID across process/reactor restarts, an authentication credential or a secret.

Network uses the same numeric value for its epoll token, opt-in log ID and SessionId. Listener/wakeup token values remain reserved. LoginHub requires monotonically increasing IDs and rejects reuse even after cleanup, so a stale local lookup cannot resolve a replacement session. Closed session IDs return no snapshot. This does not promise that the OS reuses a specific fd during a test; the identifier is independent of that behavior.

One noncopyable/nonmovable Session belongs to each registered LoginHub peer, whose unique_ptr keeps its address stable across registry rehashes. The protocol ClientSession remains responsible for ADC state/SUP/SID/INF/Tiger and SID leases; sockets/TLS and quotas remain owned by Network/Security. No socket, ADC parser, SQL, OpenSSL, logger, credentials or clock reads are hidden in the Users library.

## Lifecycle and timeline

| Users phase | Meaning / transition | Relation to ADC state |
| --- | --- | --- |
| accepted | Registered transport accepted; ADCS awaits successful TLS | PROTOCOL, without application input |
| negotiating | Plain ADC is usable, or TLS handshake completed | PROTOCOL and IDENTIFY before admission |
| identified | Valid guest identity passed uniqueness/admission and its bounded welcome was queued | Internal ADC NORMAL, but not application-ready |
| ready | Own final initial INF was completely sent, including LF | NORMAL, eligible for normal routing |
| closing | One close decision; no more application negotiation/updates | ADC state is retained while terminal output drains |
| closed | Protocol/registry cleanup finalized | Entry erased; transport closes in the same reactor turn |

Plain ADC moves from accepted to negotiating immediately at the accept timestamp. ADCS moves only through `transport_ready` after TLS success; application input before this hook is an embedding error and cannot advance the model. Expiry before TLS readiness closes immediately without queuing application notices.

Accepted, negotiating, identified, ready, closing and closed timestamps use supplied `steady_clock::time_point` values. The model performs no system-clock reads or scheduling. They are monotonic process-local measurements, not UTC dates or data ready to serialize as civil time. Each successful mutation requires a nondecreasing time; failed transitions leave the model unchanged. Invalid jumps, repeated identification/readiness, rename after closing and backwards mutation time are rejected. Equal timestamps are allowed.

Closing and final cleanup are idempotent. The first close cause/time remains fixed. `force_close` records the first forced cause and changes the requested mode to immediate without restarting the drain deadline. `finish_close` records the actual final cause/time; direct EOF/failure from an earlier live phase creates immediate closing and closed at the same time. A requested drain mode does not prove that terminal bytes were delivered: EOF, output failure or close_timeout may finish it differently. was_ready comes from the retained ready timestamp and stays true after closing.

## Public identity and snapshots

`users::Identity` holds only three owning public values: protocol session key (SID), client key (CID) and nickname. Its factory checks nonempty/NUL-free bounded storage: 64, 128 and 256 bytes respectively. These are generic model storage bounds, not a second ADC validator. The adapter first applies the stricter ADC SID/CID/Tiger/NFC/nickname/IP policy; the model factory alone provides no cryptographic, uniqueness, account or role proof. PID/PAS/passwords, peer IPs and arbitrary INF extension fields are never copied into this model.

Identity is attached once. Session/client keys are immutable thereafter; only a validated unique nickname can be renamed through the lifecycle gate. Existing ClientInfo retains the other validated protocol metadata. Session Core replaces the previous duplicated admitted/ready/closing/abort booleans and public SID/CID/nickname copies; it does not create a second user database or move every protocol registry into Users.

`LoginHub::session_model(id)` returns a const borrowed reference, valid only while that entry exists and only on the reactor thread. `snapshot(id)` returns an owning copy or nullopt for an absent/stale ID. Copies keep their original nickname/timeline/causes after rename, close or destruction of the source. Snapshot copies have no live authority or handle to a new connection. Live model reads/snapshot creation are not cross-thread APIs; future administration must arrange synchronized publication. No closed-session history or tombstone list is retained by the hub.

## Ordered completion and cleanup

LoginHub owns the model. Admission creates identified only after validation, uniqueness checks and successful bounded welcome construction; an output-budget failure before admission leaves no public model identity or registry reservation. `consume(id, bytes)` now returns true exactly once, on completed own-INF delivery. Partial, ordinary, terminal or cancelled-welcome sends return false. Network uses that transient completion to release pending admission and log login_ready, avoiding a second stored readiness flag.

Network derives pending cleanup and cumulative-login eligibility from model.was_ready, and login/TLS deadlines from model.timeline.accepted. It derives the terminal drain deadline from model.timeline.closing, which begins at the close decision rather than a later poll. The previous connection-level accepted/close timestamps, close-reason copy and readiness flag are removed. Partial-frame, idle, output-progress and TLS I/O state remain transport-specific.

Normal close/timeout/quota rejection begins a drain with its fixed Users cause. Fatal peer status, output overflow or an internal output-state failure force immediate closure. Closing causes can be inspected independently from a later final transport cause (for example idle_timeout then close_timeout). Fixed mappings retain the existing typed connection log events. Logs contain only internal ID/transport/event, never snapshots, nicknames, CIDs, status text or IPs. The new exhaustion warning has US/PL translations.

Cleanup announces one bare IQUI for an admitted identity (except process shutdown), removes owned nickname/CID/SID indexes, cancels queued output, disconnects the protocol/SID lease, marks the model closed and erases the peer. Quota release uses historical readiness, so a ready socket cannot consume someone else's pending reservation. Repeated cleanup is harmless. On normal process stop, accepted TLS and ready sessions both close without waiting for output; resource ownership still follows existing RAII/exception cleanup. No persistent per-session audit history is added.

## Verification and scope

The strict gate has 47 CTest suites. `users.session` checks nonzero IDs and exact UINT64_MAX exhaustion, copy/move restrictions, identity bounds, legal/illegal progression, monotonic fake-clock boundaries, immutable public keys, owned snapshots, rename rollback, first/forced/final close causes, early EOF cleanup and terminal idempotence. `users.session.adc` checks TLS transport readiness, internal NORMAL versus identified/ready, bytewise once-only welcome completion, cancellation, overflow before public admission, stale ID rejection, reconnect and snapshots after source removal.

`users.session.process` uses actual mixed TCP/OpenSSL peers: a pending IDENTIFY blocks another admission until full welcome; reconnect keeps public identity but gets a new local token; fatal reports/TLS failure/shutdown clean up their distinct lifetimes and preserve healthy peers. Existing real ncdc ADC/ADCS complete-login/reconnect tests now assert distinct increasing accepted IDs, one login_ready and exactly one cleanup per connection, with TLS-ready only for ADCS. Prior negotiation/provider/STA/QUI/blocked-write, MariaDB, Apache and frontend gates remain required.

This stage is the in-memory guest session model and its live integration. Registered accounts, role/ban policy, sessions exposed through HTTP/WebAdmin, mutation APIs, chat/search/transfers, distributed/persistent identities and session history remain planned. No new dependencies, migration or API JSON fields are introduced. Remaining OS/remote CI, arbitrary allocation failures and production performance/security audits require separate evidence.

References: [architecture](ARCHITECTURE.md), [guest login](ADC_LOGIN.md), [STA/QUI](ADC_STATUS.md), [negotiation security](NEGOTIATION_SECURITY.md) and [ADC 1.0.4](https://adc.sourceforge.io/ADC.html).
