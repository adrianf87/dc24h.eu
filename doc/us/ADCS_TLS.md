# ADCS / TLS — alpha-0.1.4

ADCS wraps the ADC byte stream in TLS; it does not add a second ADC dialect or STARTTLS upgrade. References: [ADC extensions / ADCS](https://dc-protocols.github.io/ADC-EXT.html), [OpenSSL nonblocking I/O](https://docs.openssl.org/3.5/man3/SSL_get_error/) and [SSL_accept](https://docs.openssl.org/3.5/man3/SSL_accept/).

`adcs::Context` owns an OpenSSL server context. It loads a PEM full certificate chain and matching unencrypted PEM private key before opening ports. Invalid, missing, mismatched or password-encrypted keys fail startup without readiness logs or interactive password prompts. This checks loadability and key correspondence; clients are responsible for certificate trust, hostname and validity verification. No mutual TLS is requested.

The transport requires TLS 1.2 or newer, disables compression and renegotiation, and disables the server's in-memory session cache. Cipher selection follows the installed OpenSSL security policy. It does not provide application identity or password authentication. TLS 1.3 may use stateless session tickets.

```sh
# TLS only; no plaintext listener is implicitly retained.
./build/dc24h --adcs-listen 127.0.0.1:1512 \
  --tls-cert /etc/dc24h/tls/fullchain.pem \
  --tls-key /etc/dc24h/tls/privkey.pem
```

Add explicit `--listen` options for mixed ADC/ADCS service. All listeners share the connection limit. Each session advances `SSL_accept`/`SSL_read_ex` on epoll readiness, including WANT_WRITE from a read. The OpenSSL error queue is cleared before operations. Buffered decrypted bytes are scheduled even without new kernel readiness; per-client read budgets remain bounded. Failed peers close independently and SIGPIPE cannot terminate the executable.

A handshake deadline is `min(idle_timeout, 10 seconds)` from acceptance; partial handshakes do not refresh it. Successful handshake/data reads refresh the normal idle timer. Decrypted ADC bytes alone contribute to `received_bytes`. `handshakes` counts successful negotiations; `incomplete` counts accepted TLS sessions closed before negotiation, including timeout and stop. Neither counter proves ADC login.

For a disposable local test certificate (never use the generated key as a production credential):

```sh
umask 077
openssl req -x509 -newkey rsa:2048 -nodes -days 1 \
  -subj /CN=localhost -addext 'subjectAltName=DNS:localhost,IP:127.0.0.1' \
  -addext 'keyUsage=digitalSignature,keyEncipherment' \
  -addext 'extendedKeyUsage=serverAuth' \
  -keyout /tmp/dc24h-test-key.pem -out /tmp/dc24h-test-cert.pem
```

Keep private keys outside Git with permissions restricted to the service user. Restart after renewal: hot reload is not implemented. Stop closes sockets immediately without a TLS close_notify exchange. Bounded outgoing queues, graceful TLS shutdown, certificate reload, full ADC handshake and hub operation remain future work. [Tests](TESTING.md) include real ncdc ADCS negotiation.

TLS paths can now be configured in INI; relative paths use the INI directory. `--check-config` loads credentials without opening ports or a log. See [configuration](CONFIGURATION.md).
