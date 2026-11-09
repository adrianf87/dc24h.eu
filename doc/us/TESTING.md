# Testing

Run `ctest --test-dir build --output-on-failure` and the equivalent for `build-release`. Tests use only loopback, ephemeral ports and temporary data; they never use the user's ncdc configuration or shares. Each wait/process has a timeout and cleanup.

| CTest suite | Coverage |
| --- | --- |
| `core.lifecycle` | States, single-use run, early/idempotent/concurrent stop, signal cancellation |
| `core.process` | Version, CLI/help, US/PL selection and messages, SIGINT/SIGTERM, repeated/startup signals |
| `network.unit` | Numeric endpoint parser, option limits, early stop and descriptor rollback |
| `network.process` | 48 concurrent peers, multiple listeners, IPv6, limits/reuse, resets/FIN, idle timeout, flood shutdown, descriptor pressure |
| `adc.parser` | Headers/commands, escaping, UTF-8/NFC, bounds, fragmentation, truncation, deterministic random corpus |
| `adc.process` | Real stream integration, recovery, per-client state and no sensitive logging |
| `adcs.crypto` | OpenSSL nonblocking negotiation/decryption with disposable credentials and preexisting error queue entries |
| `adcs.process` | Verified certificate/name, explicit TLS 1.2 and 1.3, TLS-only/mixed listeners, fragmentation, legacy rejection, handshake timeout, key mismatch/encryption/missing file, resets during TLS writes, simultaneous peers and large streams, active shutdown |
| `adc.ncdc` | Real ncdc initial HSUP through plaintext recording relay |
| `adcs.ncdc` | Real ncdc negotiates TLS directly with C++ through transparent raw relay; one decrypted HSUP, no visible HSUP on wire |

ncdc tests run in disposable PTY sessions. The ADCS relay forwards bytes unchanged, so TLS terminates in C++, not Python. The server's idle close synchronizes the counter check. TLS socket tests trust only the generated local test certificate and verify the hostname. Tests account for missing server close_notify, which is not implemented. ncdc smoke tests validate transport/initial parsing, not full login or NORMAL state.

Local result: 10/10 CTest suites passed in Debug, Release and ASan/UBSan on Debian 13 with GCC 14.2, CMake 3.31.6, OpenSSL 3.5.7 and ncdc 1.23.1. Details are recorded in the [changelog](../../CHANGELOG.md). The CI definition targets Ubuntu 22.04/24.04 in Debug/Release; its existence does not establish a successful remote run. Debian 12 and Ubuntu 26.04 require separate verification. These tests are not a performance benchmark, long-running fuzz campaign, DDoS test or security audit. Kernel/watch allocation failures and all out-of-memory paths are not covered by fault injection.
