# Logging — alpha-0.1.6

**Increment delivered in alpha-0.3.12:** [Limits](LIMITS.md) adds configurable session/user ceilings shared by ADC/ADCS, bounded by the connection cap. SessionManager checks capacity transactionally; LoginHub rejects a full user registry before welcome/list preparation with ISTA 211 and own IQUI. Closing reservations count until cleanup. Earlier descriptions retain their historical scope.

**Increment delivered in alpha-0.2.10:** [Negotiation security](NEGOTIATION_SECURITY.md) adds pending admission caps, cumulative pre-readiness input budgets and shared pre-INF/Tiger attempt limits. Earlier release descriptions below retain their original scope.

**Increment delivered in alpha-0.2.9:** [STA/QUI](ADC_STATUS.md) adds semantic status/quit validation, source-checked B/D/E/F status routing, precise errors and bounded terminal ISTA/own-IQUI output. Historical descriptions below retain the behavior of their original releases.

`dc24h_logging` provides synchronized, best-effort UTC records to stderr or a regular append file. The logger must outlive its callers; stop/join all callers before destruction. Operator messages use US by default, or PL through the compiled catalogs. Protocol data and event keys are not translated.

## Levels and configuration

| Setting | Emitted levels |
| --- | --- |
| `info` (default) | INFO, WARN, ERROR |
| `warning` | WARN, ERROR |
| `error` | ERROR |
| `off` | None |

INFO reports version/startup, non-secret settings counts/limits, listener readiness, Core lifecycle, signals and shutdown counters. WARN reports invalid/truncated ADC frames, excess connections, idle expiry, TLS failure/deadline and peer I/O problems. ERROR reports startup/fatal infrastructure errors and resource pressure. Check-only and argument diagnostics go directly to stdout/stderr before constructing a logger; `off` does not hide those diagnostics.

```ini
[logging]
level = info
file = stderr
connections = off
```

```sh
./build/dc24h --config config/dc24h.ini.example \
  --log-level info --log-file /var/log/dc24h/hub.log --log-connections on
```

Use `--log-file -` for stderr. File permissions, aliases, relative paths and INI/CLI precedence follow [CONFIGURATION.md](CONFIGURATION.md). A new file is mode `0600`. An existing file is appended only when it is already mode `0600`, owned by this user, and has one link. See [Logging security](LOGGING_SECURITY.md). A configured file is opened/validated even at `off`. No directory is created automatically.

## Connection events and privacy

Per-peer INFO records require `connections=on` or `--log-connections on`, and `level=info`. They are disabled by default. Each record has only an internal reactor token, transport and a fixed event name:

```text
2026-11-09T00:00:00Z [INFO] Connection id=2 transport=ADCS event=tls_ready
```

Public `ConnectionEvent` values are accepted, peer_closed, tls_ready, tls_handshake_failed, tls_closed, tls_read_error, tcp_read_error, socket_error, idle_timeout, handshake_timeout, shutdown, security_quota and partial_frame_timeout. The API takes the enum, numeric ID and secure flag, not an arbitrary event string. IDs are temporary and process-local, not authenticated user identities. A successful TLS negotiation is not an ADC login. A TLS close_notify received from the peer is distinguished from TLS read failure; sending server close_notify remains future work.

No remote addresses, ports, nicknames, CID/PID, PAS hashes or ADC frame bodies are passed to these records. Listener readiness still reports the explicitly configured local bind address. Startup settings report listener counts, connection/timeout limits and whether connection logging is enabled, without configuration/log/key paths. Generic info/warning/error callers must also avoid secrets: sanitization does not redact arbitrary sensitive text.

## Bounds and flood behavior

A logger emits at most 64 connection lifecycle records in each one-second steady-clock window shared by all its peers and transports. The window starts with the first event and rolls over on demand; it is not a rolling-window guarantee, so adjacent windows can burst at their boundary. Excess records are counted without allocating a queue. One WARN summary reports the suppression count at the next event after rollover, or during logger destruction if no next event arrives. The count saturates at uint64 max. Lifecycle records can therefore be incomplete and should not serve as an audit trail. Severity remains immutable for the logger lifetime.

The Network reactor separately emits at most one warning every five seconds per category: invalid frames (including truncated EOF), connection limits, ordinary idle expiry, TLS initialization/handshake failure, handshake deadline and peer I/O. Categories are independent, so a limit warning does not hide an idle timeout. Suppressed warning occurrences are not individually queued/reported; aggregate transport/parser counters remain available at INFO shutdown. Accept/watch resource errors retain the existing one-second listener cooldown. Direct Logger warning/error calls are not globally rate limited.

The message body is at most 1024 bytes including `...[truncated]` when needed, plus the fixed timestamp/severity prefix and final LF. Input is clipped before localization so large input views do not cause unbounded copying. utf8proc decodes UTF-8 and classifies controls: invalid bytes, Unicode Cc/Cf controls (including ESC, CR/LF/NUL and bidi formatting), line and paragraph separators become `?`. Other valid text, including Polish and emoji, remains UTF-8. Truncation preserves codepoint boundaries, not necessarily whole grapheme clusters. See [utf8proc](https://juliastrings.github.io/utf8proc/doc/utf8proc_8h.html).

## Operational limits

Writes are synchronous on the caller thread. Regular-file I/O and a blocked stderr pipe can still delay the reactor; rate budgets bound routine peer-driven log volume, not storage latency. Partial writes/EINTR are handled, runtime sink errors are best effort, and log exceptions do not escape. File-open failures are startup errors. There is no async queue, rotation, reopen, dynamic level change, SIGHUP reload or remote collector. Manage file growth and use trusted directories. These controls do not replace network rate limits, authentication or DDoS protection. Tests and local verification are described in [TESTING.md](TESTING.md).

## Security — alpha-0.1.7

Admission quotas, frame quotas and partial-frame expiry each have their own five-second warning category. Existing ordinary-idle and TLS-deadline categories remain independent. The new typed close events are security_quota and partial_frame_timeout; TLS expiry retains handshake_timeout. Shutdown adds Security stopped; rejected=N closed=N, with exact meanings in [SECURITY.md](SECURITY.md). Source IPs remain absent from logs, even though admission state uses them in memory.


## alpha-0.2.8

`login_ready`, `login_rejected`, `login_timeout`, `output_timeout`, `output_limit`, `write_error`: new fixed opt-in INFO events, without nicknames, addresses or payloads. Login/output timeout warnings have US/PL translations and separate five-second budgets. Wire status descriptions remain English. See [ADC_LOGIN.md](ADC_LOGIN.md).
