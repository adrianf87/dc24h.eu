# User Registry — alpha-0.3.7

**Increment delivered in alpha-0.3.12:** [Limits](LIMITS.md) adds configurable session/user ceilings shared by ADC/ADCS, bounded by the connection cap. SessionManager checks capacity transactionally; LoginHub rejects a full user registry before welcome/list preparation with ISTA 211 and own IQUI. Closing reservations count until cleanup. Earlier descriptions retain their historical scope.

This increment starts directly from main `ca75d6c53d9b228b978dc3b3b8fdd2fcb0f33852`, after the alpha-0.3.6 Nick Manager merge. Earlier requested stages alpha-0.3.1–alpha-0.3.6 are already present. The alpha-0.3.7 branch preserves main and historical branches; it is not merged automatically.

## Ownership and key meaning

`users::UserRegistry` is a bounded, noncopyable, nonmovable, single-reactor index of admitted public users by SID and CID. It belongs privately to SessionManager and replaces that manager's raw SID/CID maps. It stores public key strings and local SessionId ownership, not another User/profile model or pointers to caller values. SessionManager owns Session objects; each Session owns its optional User. NickManager independently enforces nickname ownership under the same manager's admission/cleanup transaction.

The [ADC specification, client identification](https://adc.sourceforge.io/ADC.html#_client_identification) distinguishes the hub-assigned SID, public CID and private PID. SID leases remain the ADC SidPool's responsibility. An accepted/negotiating connection can hold a protocol SID without having an admitted public UserRegistry entry. CID registration does not authenticate an account. PID, password/PAS, IP addresses, roles and profile text are not stored in these indexes.

The neutral Users model uses bounded opaque SID/CID strings: nonempty, NUL-free, at most 64/128 bytes. `add` requires a valid User supplied by its validating adapter; these checks do not prove ADC syntax, SID lease ownership, canonical base32, or hash(PID)==CID. The live ADC adapter retains all existing four-character SID, Tiger CID/PID, source binding, INF and Unicode validation before admission. Generic registry keys are exact and case-sensitive; there is no case folding or silent normalization. The SID and CID namespaces are independent.

## Registry contract

| Operation | Behavior |
| --- | --- |
| `add(session_id, user)` | Bind one SID and CID to one session owner. Never replaces an existing registration. Input is borrowed only for this call; modifying or destroying it later cannot change the keys. |
| `by_sid(sid)` / `by_cid(cid)` | Return optional SessionId by exact key; no temporary allocation. Empty, NUL-containing or oversize queries return no owner. |
| `contains(session_id)` | Report whether that lifetime owns a complete public registration. |
| `remove(session_id)` | Remove both keys for that owner, returning false when missing. Repeated cleanup cannot delete a later lifetime's reused key. |
| `size()` / `capacity()` | Current public registrations and the explicit capacity, 1–65535. |

`UserRegistryResult` is optional UserRegistryError: invalid_user, owner_exists, duplicate_sid, duplicate_cid or capacity_reached. A repeated add for the same owner returns owner_exists, even when the supplied keys match. SID/CID cannot be changed by adding the same owner again. Rebinding requires removal/new admission through the lifecycle owner; SessionManager additionally rejects SID/CID changes on a live User update. Invalid constructor capacity throws invalid_argument. Allocation failure propagates bad_alloc.

The registry maintains ordered SID and CID maps plus a reverse owner map holding their iterators. Each public key is stored once in its own map; the owner map does not duplicate strings. All three allocating insertions precede publication of a complete registration. Failure at any allocation erases only successfully inserted entries from that call. Typed rejections and exceptions leave existing users intact. Removal uses the stored iterators and does not allocate. Copy/move are disabled because the reverse index depends on these map nodes.

Capacity counts public reservations, not every accepted connection. Memory is bounded by capacity, two bounded key strings and three map nodes per user, with a bounded temporary during insertion. It is not an exact byte quota or a throughput guarantee. Lookup/add/remove are O(log n). No iterator, mutable registry reference, database handle, internal clock or cross-thread access escapes. Standalone callers must supply distinct lifetime IDs; there is no permanent tombstone history after removal.

## SessionManager and model lookups

SessionManager privately owns UserRegistry with its session capacity. Admission first checks phase, monotonic time, User validity and SID/CID/nickname collisions. It stages UserRegistry and NickManager entries before the nonthrowing Session/User commit. An exception rolls back both registries and leaves the negotiating model unchanged. Existing RegistryError and SessionError results remain the public managed mutation contract. Internal registry failures after these checks indicate a logic invariant violation, not another exposed mutation path.

`SessionManager::by_sid` and `by_cid` resolve the registered SessionId to a borrowed const Session. New `user_by_sid` and `user_by_cid` return a borrowed `const User*` to that exact session's current User, or null. They do not allocate or copy profiles. Both keys resolve to the same model; nickname/profile updates are immediately visible through a fresh lookup without reindexing immutable SID/CID. User subobject references should not be retained across User mutation or removal. Updates/removal of other sessions preserve the owning Session's address. Owning snapshots remain the existing `snapshot`/`page` API and retain their copied values.

Registration includes identified, ready, closing and closed models until `remove`; lookup alone does not authorize routing. Check the indexed Session's ready() state before application delivery. `identified_users()` now reports UserRegistry's reservation count. `begin_close`, `force_close` and `finish_close` retain both SID/CID and nickname reservations. Final `remove` requires a closed model, releases all public keys by its local SessionId, and deletes that model. Reconnection may reuse the CID/name with a new lifetime, while a removed lifetime remains absent.

## Live ADC/ADCS behavior

LoginHub's existing CID collision check now queries UserRegistry through SessionManager; duplicate CID still returns ISTA 224 with bounded terminal QUI/close. Nickname admission remains ISTA 222. Direct/echo status routing queries the registered SID and requires readiness. Rejected guests never enter public user lists or steal another user's keys. Rename/profile updates leave SID/CID bindings intact. Cleanup, TLS failure and shutdown remove registrations through the same SessionManager path.

No new external dependency, configuration, SQL migration, API JSON field, WebAdmin feature, operator message or translation key is required. EN/US remains the development/default runtime language; existing languages/us and languages/pl catalogs remain applicable. Account registration/authentication, persistent identities, role policy, multi-hub/distributed registries, chat/search/transfer dispatch and administrative mutation endpoints remain future work.

## Verification

`users.user_registry` checks capacities, empty lookups, const API, independent/exact namespaces, duplicate owner/SID/CID rejection, input ownership, maximum key/local-ID boundaries, allocation-free queries/removal, isolated cleanup, stale-owner safety, key reuse and 1000 lifetimes. Managed tests confirm both keys return the same actual User, show current nickname/profile, reject identity rebinding, preserve closing/closed reservations and remove stale entries. Scoped C++ allocation failures cover every allocation point in standalone registration and combined UserRegistry/NickManager/Session admission, with an unrelated registered sentinel and successful retry after each failure. This is not a full system allocator fault campaign.

`users.user_registry.process` exercises the real mixed TCP/TLS reactor in both transport directions: public CID/SID user lists, repeated duplicate-CID rejection, routing to the original owner, rename without key changes, disconnect/reconnect with the same CID and a new SID, missing old/rejected SID routing, rejected CID mutation and subsequent identity reuse. Private identity/profile/status payloads are absent from operator logs. Existing session, nickname, source-binding, terminal-output and provider tests remain applicable.

Real ncdc completes guest login and reconnect over ADC and ADCS while changing its Polish nick. Plaintext capture additionally checks that reconnect uses a different ADC SID and the same public CID. ADCS checks use the real OpenSSL listener and client-side NORMAL; encrypted wire contents are not claimed as plaintext inspection. See [TESTING.md](TESTING.md) for local build/regression results and their limits.
