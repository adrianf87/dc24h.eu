# Beta 0.9.x — focused ADC/ADCS, client, TLS and permission fix pass

Development branch: `beta-0.9.x-stage-full-fix-pass`, based on the
`beta-0.9.x-stage-full` head `eaafa082713e8d45de3125d36b31aa3b8d436525`.
This is a focused additional review requested by the Owner, **not** an
automatic acceptance of any of the five formal Fix Passes in issue #242.
No change to `main`, production grants, DB schema, listener defaults or
account-revocation architecture is authorized by this document.

## Corrections

1. **ADCS server-leaf role:** reject an X.509 CA certificate installed as the
   hub's end-entity TLS leaf, including a CA cert that also declares serverAuth
   and digitalSignature. Reject invalid basic-constraints data. This is a
   startup/`--check-config` refusal, before opening the ADCS listener.
2. **TLS 1.2 signing permission:** for the fixed ECDHE-only TLS 1.2 cipher
   suites, a leaf with an explicit key-usage extension must permit
   digitalSignature. keyEncipherment-only and keyAgreement-only certificates
   are not sufficient; modern RSA/P-256/Ed25519 signing leaves remain supported.
   Existing ad hoc `openssl req -x509` output may default to `CA:TRUE`;
   disposable ADCS/VM fixtures now explicitly set `CA:FALSE`. Operators
   should inspect and replace such server certificates *before* upgrading;
   this fix deliberately refuses the legacy CA-leaf configuration.
3. **ADC/ADCS client and authorization regression:** exercise a synthetic
   DC++-shaped `APDC++`/`VE`/`LO`/`CT1`/`SU` login over both plaintext ADC and
   certificate-validated ADCS. Public fields are forwarded without the secret
   `PD` or unknown `XX` field. A subsequent `CT4` operator-privilege claim
   is refused and the session is disconnected, even though it announces a
   DC++-style client identifier.

## Proof boundary

- The C++ TLS policy test compiles independently against OpenSSL 3 and checks
  acceptance/refusal of valid/non-CA, expired, weak, wrong-EKU, CA-leaf and
  non-signing-usage test certificates. The process tests additionally assert
  negative `--check-config` results for CA and non-signing leaves.
- The ADC/ADCS test **simulates client wire metadata**. It is not execution of
  the real DC++ or AirDC++ binaries. Neither client's compatibility is hereby
  certified. The existing real, pinned ncdc evidence remains independent.
- The permission regression is about **rejecting public privilege claims**;
  no new role, VIP delegation, moderation grant or runtime live-revocation
  mechanism has been introduced. Role authority continues to derive from the
  server's authenticated session/account snapshot.
- The existing verified-hostname/SNI/CA tests still bind trust decisions to
  the client. Server-side SNI hostname allowlisting is not claimed.

## Release boundary

The commit can be proposed for hosted GitHub Ubuntu 22.04, 24.04 and 26.04
CI, with the exact pushed SHA recorded separately. A passing isolated test or
pending workflow is not the complete exact-head release gate. Additional
real-client certification, the formal fix-pass ledger, Owner acceptance and
explicit protected-main merge authority remain separate work.
