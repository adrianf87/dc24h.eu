# Network security — alpha-0.9.3

TCP connection protection, timeouts and buffer limits. This increment caps the kernel listen queue and, on each accepted socket the hub keeps, the kernel send and receive buffers and the TCP user timeout. The branch starts from main `95abd7e`. The branch name is `alpha-0.9.3`.

Application idle, handshake, partial-frame, login, output and close timeouts already exist. So do per-address admission, frame and login budgets, the 16384-byte parser frame cap, the 8192-byte application read and the output queues. This increment does not replace them. It is not a bandwidth cap, not DDoS protection, and it does not close residual threat T13.

## Listen backlog

`listen` uses `listen_backlog` (128) instead of `SOMAXCONN`. The kernel may still apply a smaller `somaxconn`. The existing process test opens 48 peers at once, and that burst fits in this queue. The listener socket itself does not receive the per-connection buffer cap. A small listener buffer would not reliably cap the accepted sockets and could shrink the accept queue.

## Accepted sockets

`protect_tcp_socket` runs only after a session id is acquired and before TLS, and only for a socket the hub will keep. Excess connections, bans and admission failures still close the socket before this call.

The call sets:

- `SO_RCVBUF` and `SO_SNDBUF` to `socket_buffer_bytes` (65536). Linux reports a value from the request up to twice the request, because it accounts for bookkeeping overhead. The unit test accepts that closed range.
- `TCP_USER_TIMEOUT` to the configured idle timeout. The option value is milliseconds. The idle timeout is `std::chrono::seconds` and is already limited to 1–86400 seconds, so the user timeout is 1000–86400000 ms.

`TCP_USER_TIMEOUT` closes a connection whose transmitted data stays unacknowledged for that long. It does not close a silent connection that has no unacknowledged data. Quiet peers are still closed by the existing application idle timer. Local idle tests with a one-second idle timeout stay valid because loopback acknowledgements complete immediately.

A bad descriptor, a timeout outside 1–86400000 ms, or a failed `setsockopt` returns false. An earlier option in the same call may already have been applied. The caller then releases the peer admission, counts one ordinary rejection, emits the rate-limited warning `TCP socket protection failed; peer rejected`, and closes the socket. The warning shares the existing five-second budget. The reactor continues. The session id is released with the accept-loop handle. No new close cause is added.

The application read buffer is the existing 8192 bytes, now named `read_buffer_bytes`. Each accept batch still handles at most 64 clients. Each client read turn is still at most 8 × 8192 bytes. `TCP_NODELAY` is not set. The private Unix API socket is not a TCP socket and is unchanged.

## Unchanged

Schema v9 is unchanged. No status counter, WebAdmin page, language string, INI key or Apache setting is added. Check status stays on GitHub-hosted Ubuntu 24.04. HTTP/2, OCSP, automatic renewal and a public authenticated gateway remain unimplemented. A client that passes admission can still spend CPU on TLS and password hashing. Kernel buffer caps are not a byte-bandwidth cap.

The existing `network.unit` test checks the three constants, rejects a bad descriptor and a zero timeout, and checks both buffer options and `TCP_USER_TIMEOUT` on one loopback TCP socket. No new CTest is added.

The close-deadline process fixture still forces a 1024-byte server send buffer after this cap and sets `TCP_USER_TIMEOUT` to 120 seconds. That fixture checks the application drain deadline. It does not change the hub, where the user timeout equals the idle timeout.
