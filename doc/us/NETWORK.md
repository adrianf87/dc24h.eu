# Network

`network::TcpServer` uses level-triggered epoll on a single calling thread with nonblocking IPv4/IPv6 TCP sockets. There is no thread per client. Defaults: `127.0.0.1:1511`, 1024 active connections, 120 seconds idle timeout. Explicit listeners replace the default; ADCS-only configuration has no plaintext socket. Configure IPv4 and IPv6 separately: IPv6 uses IPV6_V6ONLY. Only numeric addresses are accepted; DNS and IPv6 zone identifiers are unsupported.

There are at most 16 listeners, shared across TCP and TLS. Port 0 requests an ephemeral port, reported by `Network listening on ...` or `ADCS listening on ...`. All ports must bind before readiness is logged; `Core running` alone does not prove readiness. Wildcard addresses require explicit options.

RAII owns all sockets; SOCK_NONBLOCK and SOCK_CLOEXEC are used. Excess clients are closed once accepted by the application; the kernel TCP handshake may have completed earlier. Counters track accepted/rejected connections and plaintext application bytes. TLS negotiation bytes are excluded. Peer FIN, reset and read errors release only that connection. Idle checks run about every 250 ms; scheduling may add delay. TLS handshake limits are in [ADCS_TLS.md](ADCS_TLS.md).

Each accept batch handles at most 64 clients, each client read turn at most 8 × 8192 bytes. Every ADC decoder buffers at most 16 KiB of unfinished frame body. OpenSSL has additional per-session buffers. Buffered decrypted data is explicitly rescheduled. These budgets preserve stop/peer progress during sustained traffic; they are not DDoS protection or bandwidth limits.

Transient accept errors retry. Descriptor/memory/watch pressure suspends listener watches for one second while existing clients and stop continue. Fatal reactor/listener errors propagate and produce exit code 1 after cleanup. Application limits do not change RLIMIT_NOFILE. Eventfd wakes epoll on stop; listeners close before clients, without waiting for peers or draining application messages. No output queues exist yet.

SIGINT/SIGTERM request idempotent graceful process cleanup and exit 0. SIGPIPE is ignored in main. SIGHUP keeps its default behavior and does not reload configuration. SIGKILL/SIGSTOP cannot be intercepted. ADC parsing produces counters, not responses or routing.

## Configuration source

Since alpha-0.1.5, listener/limit settings can come from INI and CLI. A CLI list replaces its transport while preserving the other explicit list. See [configuration](CONFIGURATION.md). Transport defaults and shutdown behavior are unchanged.
