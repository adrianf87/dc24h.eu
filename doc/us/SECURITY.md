# Security boundaries

This alpha is transport and syntax infrastructure, not a production-ready authenticated hub. The default listener is loopback; exposing a wildcard requires an explicit address. Use an unprivileged service user. TLS is optional and protects only explicitly configured ADCS listeners. Plaintext TCP remains unencrypted.

ADC input has independent bounded decoder state, UTF-8/NFC/header/escape validation and a parameter limit. It does not validate session identity, ownership of SID/IP, authorization or command semantics, and never dispatches commands. Limits on active clients and idle/handshake duration do not provide DDoS, per-IP rate or bandwidth protection. Frame bodies, PID and PAS hashes are not logged.

TLS requires loadable matching credentials and version >=1.2. Clients must validate trust/name/validity. No client certificates or user authentication are used. Encrypted PEM keys are rejected without terminal prompts. Keep keys and credentials outside Git with restricted permissions. The server follows OpenSSL's cipher policy; see [ADCS_TLS.md](ADCS_TLS.md) for shutdown and reload limitations.

SIGINT/SIGTERM are consumed synchronously, avoiding unsafe work in asynchronous handlers. SIGPIPE is ignored by main. Socket/event descriptors use close-on-exec. SIGHUP retains default system behavior. Network peer failures close the affected socket; infrastructure exhaustion or fatal reactor errors can still stop the process.

Future requirements include bounded response queues/backpressure, flood controls, session validation, authenticated roles, audited password hashing (planned Argon2id), parameterized SQL and secured API operations. Do not present planned controls as implemented.

## Configuration files and log sinks

INI reads are bounded and validate the descriptor type before reading. Final symlinks/devices/FIFOs are rejected. Log paths are checked against the active INI and TLS credentials, including existing aliases; regular append files must have one hard link. Opening a FIFO cannot block startup. Runtime writes are synchronous and best effort, without rotation. Trusted directories and unprivileged execution remain necessary. See [configuration](CONFIGURATION.md) and [Linux open(2)](https://man7.org/linux/man-pages/man2/open.2.html).

## Logging boundaries

Typed peer events omit addresses, nicknames, identifiers and password hashes. Generic message bodies are capped at 1024 bytes, made valid UTF-8 and stripped of record/terminal controls. This prevents additional log records from embedded CR/LF/Unicode separators but is not secret redaction. Per-peer logging is opt-in and rate bounded; synchronous I/O remains a storage-latency risk. See [LOGGING.md](LOGGING.md).
