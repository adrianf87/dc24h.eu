# Configuration

alpha-0.1.4 accepts CLI configuration only. No configuration file or environment-variable loader is implemented.

| Option | Default | Meaning |
| --- | --- | --- |
| `--listen IPv4:port` / `--listen [IPv6]:port` | `127.0.0.1:1511` | Plaintext TCP listener; repeatable |
| `--adcs-listen ADDRESS:port` | None | TLS listener; repeatable |
| `--tls-cert PATH` | None | PEM certificate chain; required with ADCS |
| `--tls-key PATH` | None | Matching unencrypted PEM private key; required with ADCS |
| `--max-connections N` | 1024 | Shared active connection limit, 1..65535 |
| `--idle-timeout SECONDS` | 120 | 1..86400; handshake deadline is capped at 10 seconds |
| `--language us\|pl` | `us` | Built-in UTF-8 help and lifecycle/transport messages |
| `--help` | — | Help without starting Core |
| `--version` | — | Project name and alpha version |

The first explicit TCP or TLS listener removes the implicit default. Up to 16 total listeners are accepted. Port 0 selects an ephemeral port. Wildcards are never added implicitly. Repeated scalar options use the last value. TLS paths require at least one ADCS listener; each ADCS configuration requires both paths. Password-encrypted private keys are unsupported and fail without prompting.

Help/version may be combined with `--language` but not operational options. Exit codes: 0 for normal stop/help/version, 2 for invalid arguments, 1 for runtime/startup failure. Untrusted option values are not echoed in errors. Built-in catalogs reside in `languages/us` and `languages/pl`; system/library diagnostic text without a translation falls back to English. Protocol commands, counters and client data are not translated.

Logs use UTC `YYYY-MM-DDTHH:MM:SSZ [INFO|ERROR] message` on stderr with synchronized best-effort writes. There is no file sink or rotation. Frame length (16384 bytes) and parameter count (128) are fixed parser policies. No per-frame parameters or password material are logged. SIGHUP does not reload settings or certificates.
