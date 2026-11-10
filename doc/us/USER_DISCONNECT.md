# User Disconnect — alpha-0.3.11

This increment starts directly from main `49b57749b346cb1638eae856c4b778a371616c47`, after the alpha-0.3.10 INF Updates merge. The alpha-0.3.11 branch preserves earlier history and is not merged automatically.

## Finalization and ownership

`adc::UserDisconnect` borrows SessionManager on the single reactor thread. It owns no socket, TLS context, queue, database, timer or duplicate user registry. Its two operations allocate no memory and are noexcept:

1. `begin(id, cause, now)` finds the model, copies its typed public SID and historical readiness, then calls managed finish_close. The returned Departure contains values, not references into the source. Accepted/negotiating connections have no public departure. Identified users awaiting welcome do have public presence and require QUI, even if they never became ready.
2. The owner cancels its output charge, disconnects ClientSession (releasing its RAII SID lease and metadata), and destroys the Peer containing borrowed Session references.
3. `release(id)` removes the closed Session through SessionManager, releasing SID/CID UserRegistry and NickManager reservations. Only after this succeeds does LoginHub announce the saved SID to remaining public recipients.

The order fixes an earlier failure mode: allocating/broadcasting QUI before cleanup could throw with the source still retained. It also returns the source's global output allowance before charging survivors for QUI. A disconnected source cannot be routed to, and the next connection can reuse CID/nickname and an available SID without inheriting the previous lifetime's local SessionId.

begin preserves the first close cause, original close timestamp, readiness history and monotonic lifecycle rules. Missing IDs and backwards time return typed RegistryError/SessionError without mutation. Repeating begin on a closed model returns first=false without another public SID; repeating release returns missing. Repeated LoginHub::disconnect on an absent Peer does nothing. Release of a live model returns not_closed. A malformed generic User SID is reported as invalid_public_sid, but cannot prevent model cleanup; actual ADC admission already requires a valid, nonzero SID.

The caller must drop all borrowed Session references before release. The module cannot inspect external references and does not make arbitrary adapters safe automatically. In the live server, Network remains responsible for the terminal event, removing the epoll watch before closing the fd, releasing pending/per-IP quotas using historical readiness, and destroying the socket/TLS/decoder connection. The unique transport token is never reused as another session's ID. No network ownership is moved into UserDisconnect.

## QUI, queues and failure handling

The [ADC QUI specification](https://adc.sourceforge.io/ADC.html#_qui) defines IQUI with the departing SID. Public notifications here are exactly `IQUI <SID>\n`, without a client-provided reason, PID or private metadata. Identified/ready recipients receive it once after their existing queued frames. Anonymous, closing and immediately closed recipients are excluded. No new frame can be placed after a recipient's own terminal QUI. An identified recipient's own-INF welcome delimiter remains ahead of later presence events.

Existing immutable output buffers and full-frame byte accounting remain. Source cleanup does not alter another recipient's partially consumed or TLS-pinned front pointer/view. Per-client and global output limits remain enforced: a recipient that cannot queue the notice is forced closed with output_limit. Its own resources are released by its normal reactor cleanup.

After source release, LoginHub catches bad_alloc while constructing the public QUI or enqueueing it. If notice construction fails, all remaining public recipients are forced to immediate internal_error without another allocation. If a single enqueue fails, that recipient is forced closed and fanout continues to others. This prevents a client from continuing with a usable stale user list. Reservations for these recipients remain held until their individual cleanup. Fatal cleanup paths cannot promise network delivery to an already broken connection; no whole-system OOM recovery or atomic delivery to all clients is claimed.

The existing terminal protocol remains: bounded own ISTA → own IQUI → close for applicable rejections, with partial-front preservation and the existing drain deadline. Once terminal input or transport failure ends the source, its public departure follows the sequence above. Ordinary EOF, reset, TLS failure, protocol/status rejection, timeout and resource-limit paths use the same final cleanup. Server shutdown explicitly disables public announcements while closing all connections, avoiding useless QUI storms. No implicit TLS close_notify exchange or unbounded graceful drain is introduced.

No dependency, configuration setting, SQL/API contract, WebAdmin function or runtime translation key is added. EN/US UTF-8 remains the default, with existing languages/us and languages/pl catalogs. Account persistence, chat, search and transfers remain future work.

## Verification

`users.disconnect` checks all six lifecycle phases, no-allocation finalization/release, first-cause/time retention, backwards time, missing/repeated operations, invalid generic SID cleanup, reservation retention until release and actual one-slot SID lease/CID/nickname reuse. LoginHub fixtures cover queued welcome cancellation before readiness, anonymous ADCS cleanup, exactly-once bare QUI, FIFO order, global queue reclamation before fanout and an unchanged pinned recipient front.

A scoped C++ allocation campaign proves source resources are gone even when public notification fails. Persistent and one-shot faults exercise global notice failure, per-recipient isolation and continued healthy delivery. This injects C++ allocation failures in the exercised paths, not C allocator, kernel or unrelated protocol operations.

`users.disconnect.process` repeats EOF, RST, client fatal HSTA and invalid-INF terminal closure over ADC and ADCS, with an observer on the other transport. Each cycle checks one bare public QUI, same-CID/nickname reconnect, release of the sole pending-login allowance after failed anonymous TLS, no anonymous public departure, unique transport IDs and exactly one terminal log event per accepted connection. Shutdown covers ready users and an incomplete TLS connection without public application output or private client payload in logs.

The actual ncdc ADC/ADCS tests also require an unavailable-recipient status when routing to the departed ncdc SID. Earlier terminal ordering, timeout, output-limit, user-list, update, security, TLS and privacy tests remain in the full regression gate. See [TESTING.md](TESTING.md) for local results and commands.
