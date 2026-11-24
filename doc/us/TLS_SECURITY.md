# TLS security — alpha-0.9.5

ADCS certificate and cipher policy. The branch starts from main `9d66f47`. The branch name is `alpha-0.9.5`.

The hub already required TLS 1.2 or newer, refused compression and renegotiation, and checked that the PEM chain matches the unencrypted private key. This increment adds a fixed leaf policy and an AEAD cipher list before any listener opens. It is not optional and it is not an INI key.

## Leaf certificate

Startup reads the system clock once. The leaf must already be valid: notBefore is at or before now, and notAfter is later than now. There is no grace period. A certificate that expires while the process is running stays in use until restart. This is not automatic renewal.

The signature must be RSA or ECDSA with SHA-256, SHA-384 or SHA-512, or Ed25519 or Ed448. The public key must be an RSA key of at least 2048 bits, NIST P-256, P-384 or P-521, or Ed25519 or Ed448. A smaller RSA key, another curve or a SHA-1 signature is refused.

The leaf must not be a CA certificate and malformed basic constraints are refused. If the leaf has a key-usage extension, it must allow digitalSignature: the configured TLS 1.2 ECDHE suites need a signing certificate. keyEncipherment or keyAgreement alone cannot satisfy this requirement. If it has an extended-key-usage extension, that extension must include serverAuth. A missing extension is not treated as proof of either property. The hub does not check the hostname, the trust anchor or revocation. Clients still do that. No client certificate is requested.

An expired, not-yet-valid, weak or mis-scoped leaf fails startup and `--check-config` before a port is opened. The diagnostic does not include the certificate or the private key. The US and PL catalogs carry the same sentences. The help text is unchanged.

## Ciphers

TLS 1.2 offers only ECDHE with AES-GCM or ChaCha20-Poly1305. TLS 1.3 offers AES-128-GCM, AES-256-GCM and ChaCha20-Poly1305. The hub prefers that order. Session tickets are off and the server session cache stays off, so a later connection cannot resume. CBC, anonymous and non-ECDHE suites are not offered.

## Unchanged

Schema v9 is unchanged. No status counter, WebAdmin page or Apache setting is added. The Apache examples keep their own cipher lists. Check status stays on GitHub-hosted Ubuntu 24.04. HTTP/2, OCSP and automatic certificate renewal remain unimplemented. This does not close residual threat T15. A public authenticated gateway remains separate work.

`adcs.tls_security` accepts a current RSA, P-256 and Ed25519 leaf, including a leaf with no key-usage extension. It refuses an expired leaf, a future leaf, SHA-1, RSA-1024, secp256k1, client-only extended key usage and a key usage that cannot serve TLS. `adcs.tls_security.process` checks the Polish diagnostic, refusal before listen, a current `--check-config`, rejection of TLS 1.2 CBC, acceptance of TLS 1.2 GCM and TLS 1.3, and that the next connection does not resume.
