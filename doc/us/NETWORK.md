# Network

`network::TcpServer` uses level-triggered epoll on a single calling thread with nonblocking IPv4/IPv6 TCP sockets. There is no thread per client. Defaults: `127.0.0.1:1511`, 1024 active connections, 120 seconds idle timeout. Explicit listeners replace the default; ADCS-only configuration has no plaintext socket. Configure IPv4 and IPv6 separately: IPv6 uses IPV6_V6ONLY. Only numeric addresses are accepted; DNS and IPv6 zone identifiers are unsupported.

There are at most 16 listeners, shared across TCP and TLS. Port 0 requests an ephemeral port, reported by `Network listening on ...` or `ADCS listening on ...`. All ports must bind before readiness is logged; `Core running` alone does not prove readiness. Wildcard addresses require explicit options.

RAII owns all sockets; SOCK_NONBLOCK and SOCK_CLOEXEC are used. Excess clients are closed once accepted by the application; the kernel TCP handshake may have completed earlier. Counters track accepted/rejected connections and plaintext application bytes. TLS negotiation bytes are excluded. Peer FIN, reset and read errors release only that connection. Idle checks run about every 250 ms; scheduling may add delay. TLS handshake limits are in [ADCS_TLS.md](ADCS_TLS.md).

Each accept batch handles at most 64 clients, each client read turn at most 8 × 8192 bytes. Every ADC decoder buffers at most 16 KiB of unfinished frame body. OpenSSL has additional per-session buffers. Buffered decrypted data is explicitly rescheduled. These budgets preserve stop/peer progress during sustained traffic; they are not DDoS protection or bandwidth limits.

Transient accept errors retry. Descriptor/memory/watch pressure suspends listener watches for one second while existing clients and stop continue. Fatal reactor/listener errors propagate and produce exit code 1 after cleanup. Application limits do not change RLIMIT_NOFILE. Eventfd wakes epoll on stop; listeners close before clients, without waiting for peers or draining application messages. No output queues exist yet.

SIGINT/SIGTERM request idempotent graceful process cleanup and exit 0. SIGPIPE is ignored in main. SIGHUP keeps its default behavior and does not reload configuration. SIGKILL/SIGSTOP cannot be intercepted. ADC parsing produces counters, not responses or routing.

## Configuration source

Since alpha-0.1.5, listener/limit settings can come from INI and CLI. A CLI list replaces its transport while preserving the other explicit list. See [configuration](CONFIGURATION.md). Transport defaults and shutdown behavior are unchanged.

## Logging integration

With opt-in INFO logging, accepted/TLS-ready/close events include only an internal token and transport. Ordinary idle expiry and unfinished TLS handshake expiry have distinct events/categories. Invalid frames, limits, TLS failure and peer I/O warnings use independent five-second budgets; no frame contents or peer addresses enter logs. Existing counters remain unchanged. See [LOGGING.md](LOGGING.md).

## Security — alpha-0.1.7

The Network options include `security::Limits`. A single bounded `PeerLimiter` spans every plaintext/TLS listener, using canonical kernel source addresses kept only in memory. Admission is checked before TLS allocation/watch registration, and active slots are rolled back/released on setup failure or disconnect. Every client has independent frame-rate, invalid-count and absolute partial/TLS timing state. Deadlines are checked before/after I/O and per decoded segment, in addition to periodic expiry scans. INFO shutdown and `statistics()` add security_rejected/security_closed without changing existing counters. See [SECURITY.md](SECURITY.md) for defaults, fixed windows, counter meaning and limitations.

## Administrative socket — alpha-0.1.9

The optional local HTTP API uses a separate private Unix-domain stream socket and worker, not an ADC/ADCS listener. Its active-client/deadline policies are independent of TCP/TLS security counters. No HTTP port is opened and Apache2 is not involved in transport tests. See [API.md](API.md).

## State integration boundary — alpha-0.2.3

The new [ADC State library](ADC_STATE.md) is transport-independent and not yet attached to Client objects. ADC/ADCS wire behavior and syntax counters are unchanged. Attachment requires bounded ordered output, committed-send events, protocol/identity validation and a unique SID allocator; otherwise an inbound-only handshake would reject valid clients while being unable to reply. Existing frame/TLS deadlines remain in Network; a future session deadline must bound complete login independently of keepalives.

## SUP adapter — alpha-0.2.4

The [SUP/state adapter](ADC_SUP.md) completes feature/hash selection as a standalone C++ module. Network still uses the parser alone and sends no SUP response. The new ncdc SUP fixture connects real ncdc to Python-owned temporary sockets/TLS and the C++ module driver; existing ADC/ADCS tests continue to cover the actual C++ reactor. These tests demonstrate different integration boundaries. Attach ProtocolConnection only with bounded output, completed-send events, unique SID allocation, identity/hash computation and login deadlines.
