# Session Manager — alpha-0.3.4

**Increment delivered in alpha-0.3.13:** [Statistics](STATISTICS.md) adds constant-time lifecycle counts, online/peak users and completed-login totals, atomic transport statistics, authenticated API fields and bilingual WebAdmin metrics. Historical increments below retain their delivery scope.

**Increment delivered in alpha-0.3.12:** [Limits](LIMITS.md) adds configurable session/user ceilings shared by ADC/ADCS, bounded by the connection cap. SessionManager checks capacity transactionally; LoginHub rejects a full user registry before welcome/list preparation with ISTA 211 and own IQUI. Closing reservations count until cleanup. Earlier descriptions retain their historical scope.

**Increment delivered in alpha-0.3.5:** [User Core](USER_CORE.md) adds checked UTF-8/NFC Nickname values, shared ADC/User text validation, immutable rename candidates and explicit projection from validated ClientInfo/VerifiedIdentity. Users now links the neutral dc24h_text library using the existing utf8proc package. Manager ownership/uniqueness remains unchanged. Earlier descriptions retain their historical scope.

This increment starts from main `6162f8a648b4e7b720d0417d0e829c521ea0e55d`, after the ordered Session Manager/User Core merges. The central lifetime registry introduced in alpha-0.3.2 now owns the SID, CID and nickname indexes previously held by LoginHub. It integrates public User identity/profile with managed lifecycle mutations, rather than introducing another session store. Main and historical branches are preserved; this branch is not merged automatically.

## Ownership and registration

One noncopyable/nonmovable SessionManager owns the live Session models in dc24h_users, independently of sockets, ADC parsing, TLS, logging and SQL. Each Session owns its optional User. LoginHub peers borrow const models while retaining protocol state, SID leases and ordered output. There is one authoritative public identity index per key kind, owned by the manager. No mutable model pointer escapes add/find: registered model changes must use manager methods. Standalone Session remains usable without a manager.

Capacity is explicit, 1–65535 entries; LoginHub uses its existing connection limit. All registered transports count, including accepted ADCS before TLS success, negotiating clients, identified admissions awaiting full welcome, ready users and closing entries. Closing entries occupy capacity until removal. This is distinct from the public admitted-user count and application readiness.

`add(SessionId, Transport, accepted)` returns a borrowed const Session pointer or typed stale_id/capacity_reached. IDs are nonzero and strictly increase across the manager lifetime, including after removal. Capacity rejection commits neither an ID nor an entry. Allocation exceptions do not insert a partial model or advance the watermark. Network still supplies its nonreusing sequence and registration does not itself start negotiation. Failed protocol-peer construction finalizes/removes its model but keeps the registered ID consumed.

An ordered map owns unique_ptr models. Inserting/removing another ID keeps model addresses stable. Removal/destruction invalidates references to that model. Identity/profile subobject references also expire when replaced. All access, including const queries/snapshot copying, is restricted to the owner reactor thread; const does not provide synchronization. There are no cross-thread handles, callbacks or persistent UUIDs.

## Managed changes and identity reservations

The manager exposes start_negotiation, identify, update_user, rename, mark_ready, begin_close, force_close and finish_close with supplied monotonic times. MutationResult distinguishes RegistryError from SessionError; missing IDs are explicit errors. Phase/time guards, first/forced/final causes and deadline idempotence retain Session Core behavior. There are no hidden clock reads or scheduling.

`identify(id, User, time)` requires negotiating, valid bounded public data and an unused SID, CID and nickname. Duplicate keys produce duplicate_sid, duplicate_cid or duplicate_nickname. Allocation of all three index nodes precedes the nonthrowing model commit; if staging allocation throws, previously inserted keys are erased and the model stays unchanged. No allocation occurs after this commit. Model/state/time rejection creates no reservation. No arbitrary allocation-fault campaign is claimed by the tests.

`update_user` requires identified/ready, unchanged SID/CID and nondecreasing time. A changed nickname is checked and allocated before replacing the full User; the old index is removed only after the model commit. Duplicate/invalid/backwards updates preserve both identity and profile. An unchanged nickname refreshes the profile without adding an index entry. `rename` follows the same path while preserving the profile. Public keys are exact and case-sensitive, matching the previous ADC registry policy; no new Unicode normalization or case folding is introduced.

Storage factories and registry uniqueness do not prove CID ownership or account identity. The existing ADC SID/CID/Tiger/NFC/INF checks run before projection and remain authoritative. The generic manager works with bounded public opaque keys; no PID, PAS/password, peer IP, privilege or account/role data is indexed. Client profile declarations remain unverified metadata.

Identity reservations survive closing and closed until remove, preventing reuse while terminal output drains. Lookups can therefore return a non-ready session. `finish_close` records the final model state but does not free keys. `remove` requires closed and erases exactly that owner's SID/CID/nickname entries and model. Missing and not_closed remain explicit errors. After removal, the public identity may be reused by a new greater local SessionId, while lookup by the removed local ID remains absent. There are no tombstones or retained closed-session histories.

## Lookup, snapshots and cost

`find(id)`, `by_sid(key)`, `by_cid(key)` and `by_nickname(key)` return borrowed const Session pointers or null. Public queries reject empty/NUL/oversize keys before search, using existing model storage limits of 64/128/256 bytes. The three namespaces are independent. Check ready() for application routing; finding a reserved public identity does not authorize delivery or authentication. Exact lookup/insertion/removal uses ordered maps with O(log n) behavior and bounded key lengths, without allocating temporary lookup strings.

`identified_users()` reports the number of public reservations, including identified/ready/closing and any finalized entry awaiting removal. `counts()` now returns total and all six phase gauges in O(1), maintained at successful model transitions; tests reconcile them with owned models. See [Statistics](STATISTICS.md). The hub removes finalized entries during the same cleanup call.

`snapshot(id)` returns an owning copy or nullopt. `page(after, limit)` returns copies in ascending SessionId order strictly after an exclusive cursor, default 64, limit 1–256. Invalid limits throw before copying; gaps/deleted cursors are supported and UINT64_MAX never wraps. An empty page marks the current end. Use the last returned ID as the next cursor. Pages are independent owner-thread reads, not a frozen transaction. Copying includes bounded public identity/profile and lifecycle data; snapshot values survive source update/removal. No unlimited full-registry snapshot API is introduced.

Memory is bounded by capacity, model/profile limits and at most three bounded index keys per admitted model; it is not a configurable exact byte budget. Public index key copies support lookup and do not constitute a second User model. Registration, lookup and removal are O(log n); pages are O(log n + limit) excluding bounded copying. No performance/load benchmark is inferred.

## Live ADC/ADCS integration

LoginHub no longer owns nickname/CID/SID maps. Prospective nickname checks and validated CID admission checks query SessionManager, with the same 222/224 statuses. Successful bounded welcome construction precedes manager identification. Full own-INF delivery marks ready through the manager exactly once. INF updates replace the full User and its nickname index; failed validation and duplicate nick attempts preserve the prior public model.

Direct/echo STA resolves its SID target through the manager and checks readiness before using the existing protocol/output peer. Relays, welcome order, quota policy and ADC serialization remain unchanged. Cleanup announces IQUI where appropriate, cancels output/disconnects protocol state, finalizes the managed model, destroys the peer and removes the model/indexes. Repeated hub disconnect remains harmless. A closing reservation cannot route normal application output and cannot be stolen by another admission.

`LoginHub::sessions()` remains a const internal query surface, not HTTP/WebAdmin access. No new configuration, runtime messages/translations, dependency, SQL migration, credential or API JSON field is required. Existing US/PL runtime catalogs remain applicable. Accounts, roles, bans, persistent/distributed registries, synchronized publication and remote mutation remain separate work.

## Verification

`users.registry` checks typed missing/phase/time errors, all three duplicate keys without partial reservation, immutable keys, rename/profile rollback, exact case and lookup bounds, closing/closed reservations, removal isolation, identity reuse with a new local ID and 1000 repeated admission/removal lifetimes. Compile-time checks require const borrowed models. Updated users.manager exercises the managed mutation API while retaining capacity, stable-address, pagination, maximum-ID and protocol-setup rollback coverage.

users.core also checks that actual LoginHub public lookups resolve the same model, old nicknames disappear after rename, closing peers retain reservations and disconnect removes them. Existing process tests cover mixed TCP/TLS duplicate rejection, updates/reconnect, pending readiness, terminal output and actual ncdc ADC/ADCS. See [testing](TESTING.md), [Session Core](SESSION_CORE.md), [User Core](USER_CORE.md) and [guest login](ADC_LOGIN.md). Local tests do not imply other OS, remote CI, production load or a security audit.
