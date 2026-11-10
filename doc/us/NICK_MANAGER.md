# Nick Manager — alpha-0.3.6

**Current increment — alpha-0.3.8:** [User Login](USER_LOGIN.md) bridges a validated, guest-approved ADC ClientSession to SessionManager registration. It projects verified public identity, returns typed failures and checks identity/lifecycle again before welcome completion. LoginHub retains bounded output ownership and marks readiness only after the own-INF final LF is sent. Earlier module descriptions retain their historical scope.

This increment starts directly from main `cb49cdb9e1e714f3b548be6a842063c58890eaf6`, which already includes alpha-0.3.1–alpha-0.3.5. Historical branches and main are preserved. The new alpha-0.3.6 branch is not merged automatically.

## Ownership and validation

`users::NickManager` is a noncopyable, nonmovable, single-reactor registry owned privately by SessionManager. It replaces that manager's raw nickname map; SID/CID indexes and Session/User ownership stay in SessionManager. No second live nickname registry exists in LoginHub. A standalone NickManager can be tested without sockets, TLS, ADC, a database or a clock.

Each reservation binds one checked name to one nonzero local SessionId. A reverse owner index enforces one name per session and supports cleanup by lifetime ID. Capacity is explicit, 1–65535, and live integration uses the session limit. Registered sessions can exist without a nickname until admission; pending names do not represent authenticated accounts.

The string overloads of `reserve` and `rename` call the existing `Nickname::create`. Checked-value overloads reuse its validation. Names must be nonempty, at most 256 UTF-8 bytes, already NFC, without ASCII space, controls, format characters or line/paragraph separators. Names are not normalized or case-folded; `Name` and `name` are distinct. This preserves the established hub policy. Non-ASCII spacing follows the existing Unicode category policy. A moved-from empty Nickname is rejected.

ADC retains its stricter 64-byte NI limit. The stream parser drops invalid UTF-8/non-NFC frames before INF processing; a valid subsequent frame can recover. INF rejects additional prohibited characters and empty/canceled required names, and validates all other changed fields before public User projection. The generic name factory does not replace these transport/protocol checks.

The [ADC specification, INF/NI and STA](https://adc.sourceforge.io/ADC.html#_inf) defines nickname uniqueness and permits hubs to restrict the accepted character set. This increment retains existing exact comparison, wire error codes and character policy.

## Registry contract

| Operation | Behavior |
| --- | --- |
| `reserve(id, name)` | Validate and reserve a new owner/name pair; duplicate owner, duplicate name and capacity are typed errors. Repeating reserve for an existing owner returns owner_exists. |
| `rename(id, name)` | Require an existing owner; self-rename succeeds, even at capacity. Duplicate/invalid names preserve the old name. |
| `owner(name)` | Optional SessionId from an exact lookup, without temporary allocation. Empty/NUL/oversize queries return no owner. This is lookup, not an independent full validation API. |
| `release(id)` | Remove only that owner's pair, returning false if absent. Repeating cleanup cannot release a name subsequently reserved by a different lifetime. |
| `size()` / `capacity()` | Current reservations and configured upper bound. |

`NickResult` is optional NickError: invalid_nickname, duplicate_nickname, owner_exists, missing or capacity_reached. Invalid constructor capacity throws invalid_argument. Allocation failure propagates bad_alloc and leaves both indexes unchanged. Raw string validation precedes registry checks; no general precedence across simultaneous errors is promised. No borrowed string or map iterator escapes.

Reserve allocates the name entry first and the reverse-owner node second, rolling back the first if the second fails. Rename allocates the new name before changing the owner's iterator and erasing the old entry. Those final operations do not allocate or throw. Operations are O(log n), with bounded strings and at most one name plus two map nodes per reservation. The owner index stores an iterator, not another name copy. Capacity is an entry limit, not a precise memory budget. Concurrent access requires external ownership; const methods do not provide synchronization. Standalone callers must not reuse a local lifetime ID while it still identifies an old connection.

## Session and live protocol integration

SessionManager still checks lifecycle, monotonic time, valid User and immutable SID/CID before nickname changes. Admission stages SID/CID and NickManager entries before its nonthrowing Session commit; exceptions roll back all reserved keys. Profile-only updates avoid registry changes. A changed name commits through NickManager, followed by the already-checked nonthrowing User move. Public `SessionManager::rename` builds a candidate preserving the profile and uses the same path. No mutable registry reference escapes the session manager.

Reservations survive identified, ready, closing and closed states until `remove` finalizes cleanup. A new login cannot steal a draining session's name. After successful rename the old name is immediately available; after disconnect/remove the final name is available to a new lifetime. SID/CID stay unchanged during rename. Snapshot copies retain their original identity/profile.

LoginHub's existing prospective NI collision gate queries SessionManager and thus NickManager before ADC state commits. All successful validated INF updates use the same registry. Existing ISTA 222 rejects a taken nickname; invalid nickname checks keep their existing STA/QUI mapping. Successful full public updates reach the sender and other admitted peers; rejected fields are never relayed. Terminal output and cleanup remain bounded by existing policies.

No new dependency, configuration setting, SQL migration, API route, runtime message or translation key is needed. EN/US remains the default and existing languages/us and languages/pl catalogs remain applicable. Persistent nick ownership, registered accounts, reserved operator names, confusable detection, case-insensitive policy and administrative rename endpoints remain future work.

## Verification

`users.nicknames` covers invalid capacities, UTF-8/NFC/category/boundary checks, Polish names, case distinction, duplicate owners/names, capacity, self/other rename, stale cleanup, reuse and 1000 lifetimes. A scoped allocation budget exercises every C++ allocation point in reserve, rename and SessionManager admission, checking rollback and retry. It does not inject failures inside utf8proc's C allocator or claim an exhaustive system-wide fault campaign.

`users.nicknames.process` exercises actual ADC and ADCS, mixed transports, public rename/profile updates, self-rename, the 64-byte ADC boundary, old-name reuse before owner disconnect, collision rejection, absence of rejected-field broadcasts, cleanup/reconnect and parser recovery after a non-NFC frame. Existing users.registry covers lifecycle/time gates and reservation retention through closed state. Real ncdc login/reconnect tests change the Polish name from dc24hŻółw to dc24hŁoś and reach client-side NORMAL again over both ADC and ADCS. ncdc applies `/nick` on reconnect; live NI changes are exercised by the wire-level clients, not claimed as ncdc behavior.

Build and regression commands/results are recorded in [TESTING.md](TESTING.md). Local results do not establish production load capacity, remote CI status or validation on every listed operating system.
