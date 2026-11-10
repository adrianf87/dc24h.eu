# ADC guest login — alpha-0.2.8

**Current increment — alpha-0.3.3:** [User Core](USER_CORE.md) adds an owning public guest identity/profile to Session, with validated full-INF projection and lifecycle-guarded updates. It starts directly from main with Session Core; alpha-0.3.2 is a separate sibling branch. Earlier descriptions retain their historical scope.

**Increment delivered in alpha-0.3.1:** [Session Core](SESSION_CORE.md) provides the independent Users model and stable lifetime IDs, with live readiness/closure/timeline integration. Earlier stage descriptions below retain their original delivery scope.

**Increment delivered in alpha-0.2.10:** [Negotiation security](NEGOTIATION_SECURITY.md) adds pending admission caps, cumulative pre-readiness input budgets and shared pre-INF/Tiger attempt limits. Earlier release descriptions below retain their original scope.

**Increment delivered in alpha-0.2.9:** [STA/QUI](ADC_STATUS.md) adds semantic status/quit validation, source-checked B/D/E/F status routing, precise errors and bounded terminal ISTA/own-IQUI output. Historical descriptions below retain the behavior of their original releases.

The C++ hub now completes guest login through its actual nonblocking TCP and OpenSSL TLS listeners. `LoginHub` composes the existing SUP, SID, INF, Tiger and state modules; one reactor-owned instance spans every ADC/ADCS listener. It owns public presence, nickname/CID reservations and bounded ordered output. Registered accounts, password verification, bans/roles, chat, searches and transfers remain future stages.

This branch starts from main `b8978491fa8e146527eba818f9bc1ab0a31aa90d`, which already contains alpha-0.2.1–alpha-0.2.7. Main and historical branches are preserved; this stage is not merged automatically.

## Wire sequence and readiness

1. Client sends `HSUP ADBASE ADTIGR` (additional valid capabilities are allowed).
2. Hub sends `ISUP ADBASE ADTIGR`. Only its complete transport send permits SID reservation.
3. Hub sends its reserved `ISID xxxx`. Only completion of that LF-terminated frame moves the connection from PROTOCOL to IDENTIFY.
4. Client sends `BINF xxxx` with NI, ID and PD. Existing INF validation checks Tiger(decoded PID)==CID, assigned source, fields and kernel peer address. Public metadata never includes PD or ignored unknown fields.
5. The admission controller reserves unique nickname/CID values and approves a guest, entering the internal NORMAL state. This is an explicit guest policy; CID proof confers no account identity, operator role or permissions.
6. Hub queues IINF with hub CT32/name/description, the public INF of each previously admitted client, then the new client's own public INF last. Full send of that last frame marks application readiness and emits opt-in `login_ready`. A parsed INF or internal NORMAL transition alone does not mark readiness.
7. Existing admitted clients receive the newcomer's sanitized public INF. Valid NORMAL INF updates are echoed and relayed, with optional field cancellations preserved. Disconnect publishes IQUI and releases nickname, CID, output and SID ownership.

Client lists include admissions whose welcome is still being sent. FIFO order ensures that concurrent admissions see each other without putting a join before a recipient's own initial INF. Unknown fields and PID are never relayed. An update exports the retained public snapshot plus bare names for previously retained fields that were removed. Nicknames use exact, NFC UTF-8 byte equality, case sensitive; this is local alpha policy. CID uniqueness uses the canonical verified ID. CID/SID are public identifiers, not authentication secrets.

The hub rejects invalid sequencing, source spoofing, duplicate nicknames (222), duplicate CIDs (224), bad PID/CID pairs (227), missing BASE (245) and missing common hash (247), using a fixed sanitized fatal ISTA description. Other INF failures use the existing status mapping. No raw client parameter is inserted into status descriptions or operator logs. A nonfatal 125 status reports state-valid commands whose chat/search/transfer handlers are not implemented; no messages or searches are forwarded.

An input command cannot skip an unsent reply. Pipelined commands received before the required send completion fail their current state gate. Once a fatal rejection is queued, later input is ignored by admission; syntax/frame-budget counters may still count already-received frames. The rejected connection drains its bounded preexisting output plus status, then closes, unless a transport, quota or deadline failure requires immediate close. EOF cancels pending output. Fatal client HSTA also terminates and removes public presence. Reconnect creates a fresh session; disconnect is idempotent.

## Serializer and output ownership

`serializer.hpp` encodes decoded Message values with ADC space/newline/backslash escaping and a final LF. It validates header consistency, command/kind, parameters, feature selectors, UTF-8/NFC and BASE grammar using the shared parser. It rejects empty parameters, invalid outbound syntax and a body larger than 16384 encoded bytes. It returns either an owning string or fixed SerializeError. It does not authorize arbitrary messages; the login controller alone constructs its bounded output profile.

`LoginHub::output(id)` borrows the immutable unconsumed front of a per-peer deque. `consume(id, bytes)` accepts only bytes actually sent and commits outgoing session events after the complete frame, including LF. Partial sends do not approve SUP/SID/readiness. Enqueueing more frames leaves the front address and remaining length unchanged. A failed/partial send closes the peer; no callback pretends it completed.

TCP uses nonblocking send with MSG_NOSIGNAL and exact partial-write offsets. ADCS uses SSL_write_ex, retains the identical front address/length across WANT_READ or WANT_WRITE, and completes that write before another SSL operation. Read retries that require write readiness likewise precede application writes. Each connection has stable read storage. epoll subscribes to the required direction, with EPOLLOUT only when needed. A turn performs at most eight output frames and eight 8192-byte reads; stop remains checked between operations. OpenSSL pending plaintext is rescheduled explicitly.

Queue wire-byte budgets are **256 KiB per connection and 16 MiB per hub**, including a partially consumed front until its completion. Limits are checked before insertion. These are compiled transport defaults, not INI options; the module exposes smaller limits for tests/embedding. Decoded Message objects, deque nodes, registry snapshots, input and OpenSSL buffers add bounded overhead beyond wire-byte accounting. On overflow, only that recipient is marked for immediate close; its pinned TLS front is not modified while a write is pending. A QUIT is queued for remaining admitted peers before old ownership is released; per-recipient FIFO prevents an old presence announcement from overtaking its QUIT and a later replacement. Global overload may evict multiple recipients until capacity recovers.

An initial user list must fit the bounded per-client/aggregate queue budget; a new login whose complete snapshot cannot fit is closed without publishing its identity. Snapshot streaming is not implemented.

Shutdown closes listeners and clients without waiting for output or close_notify. TLS close_notify/reload and production load/fuzz/fault-injection work remain separate follow-ups.

## Deadlines and configuration

INI `[security]` adds `login_timeout=30` and `output_timeout=10`, each accepting 1–3600 seconds. There are no new CLI switches; INI or library Options configure these settings. Existing TLS handshake, partial-frame, idle, rate and IP limits continue to apply.

The absolute login deadline starts at accept, includes TLS, and ends only when the own INF has been fully sent. Keepalives and partial identity traffic do not renew it. The output deadline starts when queued data awaits progress, renews after bytes are sent and clears when the queue empties. Reads cannot extend it. Reactor checks are periodic (about 250 ms) as well as before/after inbound I/O; scheduling can add delay.

New opt-in INFO events are `login_ready`, `login_rejected`, `login_timeout`, `output_timeout`, `output_limit` and `write_error`. Timeouts/queue overflows increment security_closed; ordinary protocol rejections do not. Event budgets remain bounded. English operator timeout messages have Polish translations. ADC wire descriptions remain English regardless of operator language. Administrative API v1 and WebAdmin schemas are unchanged: active_connections counts transport sockets and adc_frames counts parsed syntax, neither is a logged-in user count.

## Verification and remaining scope

`adc.login` checks serializer escapes/encoded limits, ordered partial sends, concurrent admissions, last-own-INF ordering, private-field removal, updates/cancellations, duplicates, spoofing, fatal cleanup, immutable front storage and per-peer/aggregate overflow accounting. `adc.login.process` runs the actual C++ TCP/OpenSSL listeners for mixed-transport presence, fragmented login, duplicate/Tiger rejection, reconnect, absolute deadline under keepalives and slow-reader isolation. `adcs.crypto` forces SSL write backpressure on a tiny socket buffer and verifies no lost/duplicated decrypted bytes across retries.

`adc.ncdc` and `adcs.ncdc` now use disposable ncdc PTYs, complete guest login and reconnect against C++. ncdc sends a normal chat attempt and displays the expected unimplemented-command status, proving it accepted its own initial INF. The ADCS relay is byte transparent; TLS terminates in C++ and no plaintext HSUP/BINF/probe appears on the recorded wire. Earlier SUP/SID/INF/identity fixture tests retain their historical narrower scope.

Password/account policy is not enabled: this release admits valid guests, does not read account rows and does not use the low-level password approval methods. Do not infer production hub completeness, registered-user authentication, implemented chat/search/transfer operations, measured scalability or a security audit from these tests. See [TESTING.md](TESTING.md) and [ROADMAP.md](ROADMAP.md).

Protocol basis: [ADC 1.0.4](https://adc.sourceforge.io/ADC.html), especially sections 3.5, 5.1, SUP/SID/INF/STA/QUI; transport retry contract: [OpenSSL SSL_write](https://docs.openssl.org/3.0/man3/SSL_write/).
