# User Login — alpha-0.3.8

**Increment delivered in alpha-0.3.12:** [Limits](LIMITS.md) adds configurable session/user ceilings shared by ADC/ADCS, bounded by the connection cap. SessionManager checks capacity transactionally; LoginHub rejects a full user registry before welcome/list preparation with ISTA 211 and own IQUI. Closing reservations count until cleanup. Earlier descriptions retain their historical scope.

This increment starts directly from main `cc2a52603d0e67a6de722c783d84922f204ca0db`, after the alpha-0.3.7 User Registry merge. Earlier requested stages are already present. The alpha-0.3.8 branch preserves main/history and is not merged automatically.

## Responsibility and prerequisites

`adc::UserLogin` is the explicit bridge from successful ADC identification to the managed public session. It borrows SessionManager on the owner reactor thread and owns no sockets, queues, credentials, User copies or clock. It is noncopyable/nonmovable. The manager must outlive it. Callers supply the ClientSession belonging to the supplied local SessionId; LoginHub supplies this pair from the same Peer. The module does not independently authenticate a transport token.

The [ADC specification](https://adc.sourceforge.io/ADC.html) separates protocol negotiation, identification, optional verification and normal operation. This release continues to admit guests only. ClientSession first validates SUP/hash/SID, source-bound initial INF, required NI/ID/PD, Tiger(PID)==CID and public metadata. LoginHub explicitly approves the verified identity as a guest through the existing ClientSession gate. Protocol NORMAL at this point is distinct from application readiness: the welcome may still be queued.

`UserLogin` does not parse raw INF, rehash private IDs, implement registered accounts, passwords/PAS, roles or persistent accounts. It accepts only the composed ClientSession, not an arbitrary User/ClientInfo/text parameter. PID is already discarded by ClientInfo; only its public VerifiedIdentity is used. Generic User factories and indexes alone cannot replace the ADC proof.

## Registration contract

`register_guest(id, client, now)` requires an existing negotiating model, protocol NORMAL after explicit identity approval, retained validated ClientInfo, a parseable nonzero leased SID and public CID matching the verified Tiger result. The public snapshot is projected with the shared `user_from_info` adapter into a bounded User/profile. All allocating projection work precedes mutation. SessionManager then enforces monotonic time, SID/CID/nickname uniqueness and transactional UserRegistry/NickManager insertion before committing the model to identified.

Success registers the User once with public SID/CID/name lookups but does not make it ready. A repeated call, accepted/unstarted, already identified, ready, closing or closed model is rejected by lifecycle gates. Protocol IDENTIFY with valid INF but without explicit guest approval is also rejected. Typed failure or allocation exception leaves managed model/indexes unchanged. The ClientSession is read-only to this module, including on failure; its prior protocol approval is not rolled back. The caller handles transport rejection/closure. Standalone allocation-failure tests can retry with the same approved ClientSession; LoginHub catches bad_alloc specifically around this registration call and forces immediate internal-error closure without allocating a rejection frame. Earlier protocol/welcome allocations and later presence-relay allocations remain outside this narrow guard; system-wide out-of-memory recovery is not claimed.

`UserLoginResult` is an optional variant of UserLoginError, RegistryError or SessionError. UserLoginError distinguishes invalid_protocol_state, unverified_identity (including a public snapshot that cannot be projected), and identity_mismatch. Missing session, duplicates, lifecycle and backwards-time errors keep their manager types. `user_login_status` maps them to fixed existing ADC statuses: duplicate CID 224, nickname 222, capacity 211, bad protocol/lifecycle state 244, unverified identity 227, identity mismatch 240, invalid model identity 243 and internal/missing/time/SID invariants 210. No input payload is embedded in these errors.

## Welcome completion and live ordering

`complete_welcome(id, client, now)` requires an identified model and protocol NORMAL, rechecks verified identity and compares current SID/CID/nickname with the registered identity. It then uses SessionManager's mark_ready with the supplied time. Missing, unrelated, stale, closing or already-ready models cannot complete. Success is exactly once for that model; typed rejection does not modify it. Completion is noexcept and allocation-free, including CID verification by fixed-size digest parsing rather than allocated text formatting.

The module has no byte counter: its completion method must be called only by the output owner after the actual ordered own-INF final LF is accepted by the transport. Calling it directly is a trusted integration operation, not independent proof of wire delivery. LoginHub owns and enforces this boundary:

1. Validate the incoming initial INF and reject existing nickname/CID collisions using the established early gates.
2. Explicitly approve the guest, then stage hub INF, admitted-user list and own INF in bounded FIFO output. The own INF is the welcome completion frame.
3. Register through UserLogin only after every required welcome enqueue succeeds. Welcome overflow cannot publish a public user. Relay the new public INF only after successful registration. Identified users awaiting their own welcome remain reserved/included, preserving simultaneous-login ordering.
4. Observe outgoing ADC messages only after each complete frame is sent. When the own-INF frame finishes, call UserLogin completion and return the first readiness signal. Partial output, ordinary updates and terminal output never signal readiness.

Registration errors use existing bounded ISTA/own-IQUI rejection. A completion invariant failure forces internal-error closure and never emits readiness. Existing queue caps, pinned TLS output storage, login/output deadlines and pending-admission accounting remain in their owners. Closing/closed registrations stay reserved until SessionManager removal. Disconnect finalizes/removes the model, public indexes and queue through the established cleanup path; reconnect gets a new lifetime and may reuse the public CID/name.

No new dependency, setting, SQL migration, API JSON field, WebAdmin feature, operator message or translation key is needed. Server development/default runtime language remains EN/US UTF-8, with existing languages/us and languages/pl catalogs. User Login documentation is mirrored under doc/us and doc/pl.

## Verification

`users.login` tests missing/unnegotiated/invalid-proof/unapproved identities, backwards admission time, single registration, indexed public projection, duplicate SID/CID/nick, unrelated and stale-nickname welcome completion, backwards completion time, exactly-once readiness, closing rejection and cleanup. Scoped C++ allocation failures cover projection and all managed index insertions with successful retry; an allocation budget of zero also proves completion does not allocate. A separate LoginHub allocation campaign exercises the guarded registration failure and confirms no public User/readiness, immediate close and complete queue/model cleanup. Earlier allocation points may propagate; this does not inject utf8proc C allocation or OS failures.

LoginHub tests verify an identified public registration before readiness, no readiness after hub-INF or partial own-INF, readiness on precisely the own-INF final LF, and no registration when required welcome output overflows. Existing lifecycle/terminal tests cover canceled welcomes and partial-output rejection.

`users.login.process` exercises actual ADC/ADCS in both mixed-transport directions: an unidentified SID cannot route, a failed Tiger proof causes no public join/quit, a fragmented valid identity registers, duplicate CID cannot replace it, a registered ready SID routes, fatal close cleans up and same-CID reconnect succeeds. Readiness/rejection event counts and absence of private data in logs are checked. Existing real ncdc ADC/ADCS tests reach client-side NORMAL and reconnect with Polish nicknames. See [TESTING.md](TESTING.md) for local regression results; these are not remote CI, OS-matrix, load or complete hub-functionality claims.
