# ADCS transport: alpha-0.1.4

## Implementation

The C++20 `dc24h_adcs` library uses OpenSSL's TLS server API, with a separate context loaded at startup and a per-client TLS connection. The existing Linux `epoll` loop drives nonblocking `SSL_accept` and `SSL_read_ex`, enabling concurrent encrypted and plaintext listeners. Both listener types feed the same bounded ADC stream parser. TLS connections cannot downgrade to plaintext. At this stage the server does not generate ADC responses, assign SIDs, authenticate clients or complete the ADC handshake.

TLS 1.2 is the minimum negotiated protocol; TLS 1.3 is supported when provided by OpenSSL. OpenSSL default cipher and security-level policies apply. TLS compression and renegotiation are disabled. Session caching is disabled. Clients must validate the server's certificate against their configured trust store and expected hostname. The server does not request client certificates, and it does not authenticate user accounts. A TLS handshake is not proof of an authorized ADC session.

## Credentials and ports

Use a PEM certificate chain and matching PEM private key. The key must be readable by the hub process, protected by OS access controls and stored outside the repository. The private key must be available without interactive passphrase prompts at daemon startup. Missing or mismatched credentials cause startup failure before any listeners are announced ready. No secret contents are written to logs.

Example (replace paths and port with your deployment settings):

```sh
./build/dc24h --listen 127.0.0.1:1511 \\
  --adcs-listen 0.0.0.0:1512 \\
  --tls-cert /etc/dc24h/tls/fullchain.pem \\
  --tls-key /etc/dc24h/tls/privkey.pem
```

`--adcs-listen` is repeatable. A maximum of 16 TCP and ADCS listeners combined is supported. The default plaintext listener remains loopback-only; deploying ADCS does not enable plaintext access on external interfaces automatically. For public service, use a CA-signed certificate issued for the hub DNS name. Provision renewal and restart/reload procedures externally; live certificate rotation has not yet been implemented. Use permissions such as owner-only read access on private keys, not committed test credentials.

## Failure isolation and limitations

Handshake failures and encrypted peer I/O failures close only the corresponding client, not the listener. Incomplete TLS handshakes expire after at most 10 seconds or the configured shorter idle timeout. Regular encrypted sessions follow the existing idle timeout. Memory/connection limits apply across both listener types. The server does not implement TLS close_notify on local shutdown, outbound ADC messages, rate limiting per IP, mTLS, OCSP stapling or certificate hot reload. `ADCS stopped` logs aggregate successful and incomplete handshakes without storing client payloads.

TLS counters and cipher policy must be verified in each deployment. The release is experimental and is not production-ready.
