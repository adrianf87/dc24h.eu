# ADCS / TLS — alpha-0.1.4

**Managed renewal:** The Ubuntu deployment supports reloading the certificate and key on the running hub through its private Unix API. New connections use the validated replacement, and active connections retain their established TLS state. [Automatic TLS renewal](TLS_AUTO_RENEWAL.md)

**alpha-0.9.5 TLS security:** The hub checks the leaf before listen and limits the listener to TLS 1.2 and 1.3 AEAD. Clients still check the name and the trust chain. [ADCS certificate and cipher policy](TLS_SECURITY.md)

**Public Stage I client-trust clarification:** CA/hostname verification and explicitly trusted ADC KEYP SHA-256 certificate pinning are distinct client authentication modes. In particular, ncdc with a trusted `?kp=SHA256/...` may accept a certificate without a SAN matching the connection URL; the hub cannot infer whether a client checked the name. See the [Debian 13/ncdc acceptance matrix](PUBLIC_STAGE_I_NCDC.md). Public use still requires a separately approved certificate identity and deployment policy.

**Increment delivered in alpha-0.3.12:** [Limits](LIMITS.md) adds configurable session/user ceilings shared by ADC/ADCS, bounded by the connection cap. SessionManager checks capacity transactionally; LoginHub rejects a full user registry before welcome/list preparation with ISTA 211 and own IQUI. Closing reservations count until cleanup. Earlier descriptions retain their historical scope.

**Increment delivered in alpha-0.2.10:** [Negotiation security](NEGOTIATION_SECURITY.md) adds pending admission caps, cumulative pre-readiness input budgets and shared pre-INF/Tiger attempt limits. Earlier release descriptions below retain their original scope.

**Increment delivered in alpha-0.2.9:** [STA/QUI](ADC_STATUS.md) adds semantic status/quit validation, source-checked B/D/E/F status routing, precise errors and bounded terminal ISTA/own-IQUI output. Historical descriptions below retain the behavior of their original releases.

**Integration delivered in alpha-0.2.8:** [Guest login](ADC_LOGIN.md) attaches the state/SUP/SID/INF/Tiger stack to the actual C++ TCP/TLS reactor with bounded ordered output, nickname/CID admission, public presence and complete ncdc NORMAL/reconnect tests. Historical stage descriptions below retain their original scope; statements that transport attachment/output/login were still planned describe those earlier versions. Registered-user authentication, chat, search and transfer dispatch remain planned.

ADCS wraps the ADC byte stream in TLS; it does not add a second ADC dialect or STARTTLS upgrade. References: [ADC extensions / ADCS](https://dc-protocols.github.io/ADC-EXT.html), [OpenSSL nonblocking I/O](https://docs.openssl.org/3.5/man3/SSL_get_error/) and [SSL_accept](https://docs.openssl.org/3.5/man3/SSL_accept/).

`adcs::Context` owns an OpenSSL server context. It loads a PEM full certificate chain and matching unencrypted PEM private key before opening ports. Invalid, missing, mismatched or password-encrypted keys fail startup without readiness logs or interactive password prompts. This checks loadability and key correspondence; clients are responsible for certificate trust, hostname and validity verification. No mutual TLS is requested.

The transport requires TLS 1.2 or newer, disables compression and renegotiation, and disables the server's in-memory session cache **and TLS session tickets** (including TLS 1.3). The listener offers only the configured AEAD suites. A CA certificate cannot be used as the listener leaf; an explicit key-usage extension must permit digitalSignature for the configured ECDHE suites. Clients still independently verify the certificate chain, expected host and trust anchor. TLS alone does not provide ADC account authentication or permission grants.

```sh
# TLS only; no plaintext listener is implicitly retained.
./build/dc24h --adcs-listen 127.0.0.1:1512 \
  --tls-cert /etc/dc24h/tls/fullchain.pem \
  --tls-key /etc/dc24h/tls/privkey.pem
```

Add explicit `--listen` options for mixed ADC/ADCS service. All listeners share the connection limit. Each session advances `SSL_accept`/`SSL_read_ex` on epoll readiness, including WANT_WRITE from a read. The OpenSSL error queue is cleared before operations. Buffered decrypted bytes are scheduled even without new kernel readiness; per-client read budgets remain bounded. Failed peers close independently and SIGPIPE cannot terminate the executable.

A handshake deadline is `min(idle_timeout, tls_handshake_timeout)` (TLS setting defaults to 10 seconds) from acceptance; partial handshakes do not refresh it. Successful handshake/data reads refresh the normal idle timer. Decrypted ADC bytes alone contribute to `received_bytes`. `handshakes` counts successful negotiations; `incomplete` counts accepted TLS sessions closed before negotiation, including timeout and stop. Neither counter proves ADC login.

For a disposable local test certificate (never use the generated key as a production credential):

```sh
umask 077
openssl req -x509 -newkey rsa:2048 -nodes -days 1 \
  -subj /CN=localhost -addext 'subjectAltName=DNS:localhost,IP:127.0.0.1' \
  -addext 'basicConstraints=critical,CA:FALSE' \
  -addext 'keyUsage=digitalSignature,keyEncipherment' \
  -addext 'extendedKeyUsage=serverAuth' \
  -keyout /tmp/dc24h-test-key.pem -out /tmp/dc24h-test-cert.pem
```

Keep private keys outside Git with permissions restricted to the service user. For an unmanaged listener, schedule a service restart to apply a replacement. Stop closes sockets immediately without a TLS close_notify exchange. The test suite includes real ncdc ADCS negotiation. [Tests](TESTING.md)

TLS paths can now be configured in INI; relative paths use the INI directory. `--check-config` loads credentials without opening ports or a log. See [configuration](CONFIGURATION.md).

Since alpha-0.1.7, shared per-IP quotas apply before TLS-session allocation. Absolute deadlines are checked before a handshake may complete. See [SECURITY.md](SECURITY.md).
