# User List — alpha-0.3.9

**Increment delivered in alpha-0.3.12:** [Limits](LIMITS.md) adds configurable session/user ceilings shared by ADC/ADCS, bounded by the connection cap. SessionManager checks capacity transactionally; LoginHub rejects a full user registry before welcome/list preparation with ISTA 211 and own IQUI. Closing reservations count until cleanup. Earlier descriptions retain their historical scope.

This increment starts from main `b191141ad1cfa5a6a915945b5a934fe82640340e` after the alpha-0.3.8 User Login merge. Earlier requested stages are already present. The alpha-0.3.9 branch preserves main/history and is not merged automatically.

## Initial synchronization

`adc::UserList` prepares one owned `InitialUserList`: hub IINF, one BINF per currently public existing user in ascending local SessionId order, and the joining client's own BINF last. It borrows SessionManager and a trusted callback resolving each lifetime to that peer's validated ClientInfo. It owns no persistent user registry, sockets, queue, clock or credentials. The caller must stay on the owner reactor thread and must not mutate state through the callback. The manager and metadata sources must outlive preparation; the resulting plan owns its messages and survives later source changes.

The [ADC protocol](https://adc.sourceforge.io/ADC.html) uses public INF and QUI for user information and departure. The own-INF delimiter and FIFO ordering here preserve the established guest-login behavior; no new protocol command or resynchronization extension is introduced. UserLogin remains responsible for registration/readiness and LoginHub for actual wire delivery.

`SessionManager::public_users(after, limit)` returns only identified/ready lifetime IDs in ascending order, strictly after an exclusive cursor. It copies no User/profile data. Limits are 1–256 (default 64); invalid limits throw invalid_argument. Accepted/negotiating/closing/closed sessions are omitted. UserRegistry still retains closing/closed reservations until removal; public membership and reserved identity count are distinct. Each page is an independent owner-thread read, not a cross-thread/frozen transaction.

UserList iterates 256-ID pages until exhaustion, so lists do not truncate at that boundary. Identified users awaiting their welcome are included, preventing simultaneous admissions from missing each other. Closing/closed users are omitted even if their public keys remain reserved. Existing session objects and indexes remain authoritative; there is no duplicate membership store.

## Validation and limits

`prepare(joining, client, lookup, available_wire_bytes)` requires a negotiating joining model, protocol NORMAL after guest approval, a valid nonzero SID and a retained verified public CID/name. Its own SID/CID/nickname must not already be reserved. Existing entries must resolve to validated metadata whose CID/name match the managed identity and whose managed SID is valid ADC syntax. Missing/stale/mismatched metadata fails the entire plan rather than silently omitting a public user.

Public frames come from ClientInfo::public_message, retaining validated ADC-only fields such as addresses and connection declarations while excluding PID and unknown fields. The generic User profile alone would lose these fields. No raw client buffers, passwords/PAS or account privileges enter the list. Existing ADC text/field limits and serialization remain authoritative.

The builder serializes each candidate message to measure its actual escaped UTF-8 wire bytes, including LF, and checks subtraction before adding. The budget covers every welcome frame, including hub and own INF. An exact-size budget succeeds; one byte less fails. The output owner supplies the smaller remaining per-client/hub queue allowance, so no new configurable limit is needed. Preparing does not enqueue, reserve keys, register a User, mark readiness or mutate any model. Errors are typed invalid_state, invalid_identity, inconsistent_source or output_limit; allocation failure propagates bad_alloc. Partial temporary plans are destroyed on failure.

Messages/IDs are bounded by session capacity (at most 65535) and the supplied queue budget. Only one page of IDs is needed beyond the owned plan. The measured encoding stays in that plan, and login queues those bytes instead of serializing the same messages again. Object/string/vector overhead means wire bytes are not an exact resident-memory quota. Membership scanning is O(number of sessions), plus bounded identity lookups and frame serialization. No load/throughput guarantee is claimed.

## Live synchronization and publication

LoginHub validates/approves the guest, prepares the complete list using actual remaining queue allowances, enqueues it in order with the last frame marked welcome, then registers through UserLogin. Only after successful staging/registration does it relay the new user's public INF. Planning overflow or inconsistent data forces immediate output_limit/internal_error closure before publication; allocation failure during preparation, queueing or registration forces immediate internal_error closure without allocating another status frame. Existing pinned output remains intact until cleanup. Presence relay after the commit remains outside this allocation guard; full system out-of-memory recovery is not claimed.

The immutable FIFO front and per-peer queue ensure later joins, full/canceled INF updates and IQUI departures follow the prepared own INF. A concurrent guest either appears in the initial snapshot or is appended as a join after its delimiter, never both. UserLogin checks identity and marks ready only after the own-INF final LF is actually sent. Partial/canceled/terminal output cannot complete synchronization. Existing limits/timeouts can disconnect slow/overflowing recipients; unbounded lists or a persistent retry stream are not introduced.

No new dependency, SQL migration, API JSON field, WebAdmin feature or runtime translation key is required. EN/US UTF-8 remains the default with existing languages/us and languages/pl catalogs. Accounts, remote list APIs, explicit client resync commands, distributed snapshots and chat/search/transfers remain future work.

## Verification

`users.list` checks identified/ready selection, pending/closing/closed exclusion, public ID cursors/bounds, deterministic ordering, own INF last, public-only metadata, retention of validated ADC-only fields, exact wire budgets, missing/stale sources, immutable prepared messages and more than one membership page (260 existing users). A scoped C++ allocation campaign proves preparation leaves registry/models unchanged and can be retried; it does not inject the C allocator or OS failures. Existing UserLogin allocation/overflow tests cover the live prepublication guard and cleanup.

`users.list.process` compares actual client-side membership maps across overlapping ADC/ADCS joins, ordered update fences, Polish nickname change, field cancellation, a later newcomer's initial list, departure and same-CID/new-SID reconnect. Both mixed-transport directions converge with no old SID or duplicate CID. Real ncdc ADC/ADCS now logs in with another live observer, displays two users, receives the observer in its initial list, and reconnects while that observer receives the matching quit/new join with unchanged CID. See [TESTING.md](TESTING.md) for local regression results and limitations.
