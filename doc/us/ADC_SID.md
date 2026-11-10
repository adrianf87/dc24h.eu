# SID — alpha-0.2.5

**Current integration — alpha-0.2.8:** [Guest login](ADC_LOGIN.md) attaches the state/SUP/SID/INF/Tiger stack to the actual C++ TCP/TLS reactor with bounded ordered output, nickname/CID admission, public presence and complete ncdc NORMAL/reconnect tests. Historical stage descriptions below retain their original scope; statements that transport attachment/output/login were still planned describe those earlier versions. Registered-user authentication, chat, search and transfer dispatch remain planned.

Since alpha-0.2.7, [CID/PID verification](ADC_IDENTITY.md) is a dedicated shared module with TigerCid/VerifiedIdentity values, direct cleansed output buffers and a constant-time final digest comparison. INF snapshots retain only a public verified result. Basic verification introduced in alpha-0.2.6 remains compatible; live transport attachment and account/registry policy remain separate.

Since alpha-0.2.6, [ADC INF](ADC_INF.md) supplies ClientSession, combining this lower-level stack with intrinsic INF validation and Tiger PID/CID verification. It requires validated metadata before external identity admission and never retains/exports PD. Registry uniqueness and actual Network attachment remain planned; older APIs/fixtures retain their original scope.

## Scope and protocol values

`adc/sid.hpp` supplies Sid, SidPool and SidLease. `adc/sid_session.hpp` supplies SidSession, composing a shared hub pool with [SUP](ADC_SUP.md) and [ADC State](ADC_STATE.md). These are independent C++20 components in dc24h_adc, without sockets, logging, database, Core or new dependencies. Production Network still uses only the parser; this stage does not enable a live handshake.

Reference: [ADC 1.0.4](https://adc.sourceforge.io/ADC.html), sections 3.2, 3.5.1, 5.1 and 5.3.3. A SID is a 20-bit value represented by four base32 characters. The codec uses the alphabet ABCDEFGHIJKLMNOPQRSTUVWXYZ234567, most significant group first. Examples: 0 → AAAA, 1 → AAAB, 31 → AAA7, 32 → AABA, maximum 1048575 → 7777. Parser and state now share this codec without changing their accepted SID syntax.

`Sid::from_value(uint32_t)` accepts 0..1048575; `Sid::parse(string_view)` accepts exactly four alphabet characters and rejects lowercase, padding, spaces, controls, NUL, Unicode and wrong length. Both return optional Sid without allocating. `value()` returns the number; `encode()` returns four owning characters without a terminator; `str()` returns an owning string. Sid copies are public routing values, not ownership handles, credentials or proof of identity.

The pool reserves zero/AAAA as **local policy**, not a protocol syntax restriction. The codec/parser continue to accept AAAA. Client allocation uses values 1..capacity; default capacity is 1048575. A smaller capacity selects a smaller namespace for an embedding policy or deterministic tests; no INI option or new Network limit is introduced.

## Pool algorithm and ownership

Create exactly one shared SidPool per hub and use it for every future ADC/ADCS session on the hub's reactor thread. Different hub instances have separate namespaces and may issue the same SID. Construction rejects capacity zero or greater than the 20-bit maximum with fixed-text invalid_argument, then allocates shared state and a bitmap. At full capacity the bitmap is 16384 uint64_t words, exactly 128 KiB, plus fixed metadata/container overhead. Each live lease stores only a shared state handle and optional Sid; the bitmap does not contain users or payloads.

`acquire() noexcept` returns a nonempty SidLease or an empty lease on exhaustion. Allocation starts at a rotating cursor, checks free bits word by word, wraps once and never chooses an occupied slot or an unused tail bit. It is deterministic and does not use random retry loops. Full-pool detection is constant time; a nonfull search visits at most the bitmap's words plus its split boundary word. There is no dynamic allocation in acquire/reset after successful pool construction. No randomness, unpredictability or performance benchmark is claimed.

`size()`, `capacity()` and `in_use(Sid)` report reservation state on the owning thread. Reserved-but-not-yet-sent SIDs count as occupied. in_use returns false for zero or outside this pool's range; it is not a user lookup. These fields are not atomic HTTP telemetry. Copying/moving a pool is disabled so an accidental copy cannot create a second apparent owner. Share the same pool explicitly with shared_ptr.

SidLease is default-empty, noncopyable and movable. Its private constructor prevents arbitrary reservations, and `sid()` returns a copied optional value. Move transfers unique release responsibility and empties the source. Move assignment first releases its destination; self-move is harmless. `reset()` and destruction release once; repeated reset is a no-op. There is intentionally no release-by-SID API: a stale copied identifier or moved-from lease cannot release a new owner's reservation. Shared bitmap state survives destruction of the originating pool while any lease remains, avoiding a dangling pointer during cleanup.

All pool/session mutation, lease reset/destruction and final shared-owner destruction must occur on one owning thread. Shared_ptr lifetime management does not make bitmap operations thread safe. A future multi-reactor implementation needs an explicit synchronization/ownership design.

## Managed SID session

SidSession requires a non-null shared_ptr<SidPool> and optional SupportPolicy. It retains allocator lifetime, is noncopyable/nonmovable and exposes only const protocol support. It does not expose mutable lower-level protocol state or a releasable lease. Session destruction releases any reservation, including an ISID that was prepared but never sent.

| Step | Effect |
| --- | --- |
| Accept valid inbound HSUP | Negotiate support; remain PROTOCOL, no SID reserved |
| Observe completed generated ISUP | Mark local reply complete; remain PROTOCOL |
| `prepare_sid()` | Reserve one pool value; remain PROTOCOL with empty bound sid() |
| `sid_reply()` / `sid_reply_line()` | Return owning ISID Message / LF line for that reservation; no transition |
| Observe completed matching ISID | Bind the reserved SID and enter IDENTIFY |
| Identity/password application decisions | Preserve reservation through IDENTIFY/VERIFY/NORMAL |
| Own-SID IQUI, peer/hub fatal STA, explicit disconnect or destruction | Release reservation exactly once |

`prepare_sid() noexcept` returns optional SidError. Calling before the completed ISUP, or with an existing lease, is invalid_sequence; outside PROTOCOL it is invalid_state. Exhaustion returns exhausted with unchanged state/pool/session. The owner may close with a bounded hub-full response or retry only under its own bounded policy. Another session's release may make a slot available. No automatic wait queue or retry loop exists.

`reserved_sid()` exposes the current optional value; `sid()` exposes the protocol's bound string, empty until the ISID send completes. The terminal lower-level state may retain its historical bound string after release, but every subsequent protocol event is rejected. A new connection requires a new session. SID replies are copies; allocating their strings/messages can throw without losing the owned reservation. The initial ISID profile is exactly I/SID, one reserved positional SID and no routing/features/extra parameters. Its wire encoding is exactly ten bytes including LF. Returned copies remain available while the lease exists, but a repeated send is rejected after binding.

`observe` returns optional `variant<StateError, SupportError, SidError>`. Wrong/unreserved ISID cannot change state or consume another slot. Incoming spoofed B/D/E/F sources are still rejected by the state gate. Other-user IQUI and routed status do not release this session. `disconnect()` is terminal and idempotent. `sid_status_code()` suggests fatal 211 for exhausted and 240 for other SID errors; it sends no bytes and includes no peer input. Identity/password validation remains an explicit external service, not a side effect of owning a SID.

## Transport integration still required

to_client events mean completed ordered sends, never queue insertion. Future transport integration must send/commit ISUP, prepare a SID, send/commit exactly that ISID and preserve the lease through pending output. Startup rollback, failed/partial sends, EOF and deadlines must disconnect/destroy the session. Before releasing and reusing a SID, cancel/drain old-session queued output, remove registry references and define ordering of user-list/QUIT notifications. A move-only lease prevents erroneous release; it does not make stale queued wire messages safe. SID values may be reused and carry no generation number on the wire.

The actual receive-only Network is not attached to SidSession yet. Bounded output and preflight, user registry/dispatch, Tiger computation, PID/CID/nickname validation, login deadlines and complete ncdc login remain planned. No API schema, HTTP routes, SQL migrations, TLS policy or operator catalogs change. SIDs are public and predictable; do not use them as authentication secrets or permissions.

## Verification

`adc.sid` uses release-active checks across every one of the 1048576 codec values and every one of the 1048575 allocatable pool values. It checks actual full exhaustion, 7777/tail masking, wrap/reuse, small capacities around 64-bit boundaries, deterministic churn, invalid input, move/reset/self-move, pool/lease lifetime, separate hubs, shared session uniqueness, exhausted pending sessions, stale disconnect, exact ISID matching, spoofing, VERIFY/NORMAL retention, own/other QUI, fatal status and pending-send destruction.

`adc.sid.ncdc` reuses the existing isolated ncdc fixture in --sid mode. Python sends the C++ module's ISUP, reports completion, receives/sends an allocator-generated ISID, reports that completion and forwards real ncdc BINF with the assigned source. The C++ driver stays in unapproved IDENTIFY and verifies reservation cleanup after disconnect. Both loopback TCP and Python-owned TLS are covered; no fixed SID is used by the SID driver. The older SUP fixture still uses its historical constant SID. Existing adc.ncdc/adcs.ncdc separately test actual C++ Network/OpenSSL. These are distinct boundaries, not live hub login. See [TESTING.md](TESTING.md).
