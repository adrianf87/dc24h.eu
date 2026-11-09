# Architecture

The executable composes independent C++20 libraries: `dc24h_core`, `dc24h_network`, `dc24h_adc`, `dc24h_adcs`, `dc24h_logging` and `dc24h_i18n`. Public headers live in `include/dc24h/`; implementations live under the corresponding `src/` directory. Network depends on ADC and ADCS. The parser has no dependency on Core or sockets. OpenSSL handles remain inside ADCS.

Core has a single-use `created → running → stopping → stopped` lifecycle. Without transport, `run()` waits on a condition variable. With transport, it runs the blocking reactor on the calling thread. `request_stop()` is thread safe, idempotent and retained before startup. Calling `run()` again throws. The owner must join all callers before destruction; the logger and transport must outlive Core.

Before creating threads, main blocks SIGINT/SIGTERM with `pthread_sigmask`. A `std::jthread` uses `sigtimedwait` to request stop synchronously. Its cancellation poll is 100 ms. The mask remains blocked through process exit to protect cleanup from repeated signals. SIGPIPE is ignored by the executable so broken TLS writes become peer errors. Library embedders must arrange equivalent SIGPIPE handling.

Network owns nonblocking sockets, epoll and eventfd using RAII. All client operations run on one reactor thread; only stop flags, counters and eventfd wakeups cross threads. Client event tokens never reuse socket descriptor numbers. TLS contexts outlive sessions, and sessions are destroyed before their sockets. Partial startup failures close resources before propagating to Core, which enters `stopped`. Readiness is advertised only after every listener starts.

[Network](NETWORK.md), [ADC](ADC_PROTOCOL.md), [ADCS](ADCS_TLS.md) and [API](API.md) define module contracts. Users, Chat, Search, Transfers, Security/authentication, Database, HTTP API and file Configuration remain planned. WebAdmin will be a separate React/TypeScript/Vite application.
