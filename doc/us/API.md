# API

There is no HTTP/JSON/WebAdmin API yet.

`core::Server(Logger&, TcpServer* = nullptr)` uses non-owning dependencies. `run()` is blocking and single-use; `request_stop()` is thread safe and idempotent, including before startup; `state()` returns the mutex-protected lifecycle state. Network exceptions enter `stopped` before propagating.

`network::Options` includes plaintext/TLS endpoint vectors, TLS credential paths, maximum connections and idle timeout. `parse_endpoint` accepts numeric IPv4:port or bracketed IPv6:port. `TcpServer` validates limits in its constructor, then opens listeners in `run()`. Stop wakes eventfd without closing sockets from another thread. `statistics()` independently samples atomic active/accepted/rejected/received_bytes, ADC counters and ADCS handshake/incomplete counters; it is not a transactional snapshot.

`adc::parse_line`, `recognize_command`, `Message`, `ParseResult` and `StreamDecoder::feed/finish/buffered_bytes` are defined in `dc24h/adc/parser.hpp`. See [ADC_PROTOCOL.md](ADC_PROTOCOL.md) for ownership, limits and callback semantics.

`adcs::Context` owns the credential-bearing TLS context. `adcs::Connection` owns a session referencing that context and a non-owning fd. Both dependencies must outlive the connection. `handshake()` and `receive()` return `IoResult {status, bytes}` with complete/want_read/want_write/closed/error; `pending()` reports buffered decrypted bytes. Socket readiness and SIGPIPE disposition are the caller's responsibility. No send API exists yet.

All owners must stop and join callers before destroying dependencies. `Logger(Language = us)` and `i18n::parse_language/translate/help` provide built-in messages. Future HTTP endpoints need authentication, role checks, input validation and administrative audit records behind Apache2.
