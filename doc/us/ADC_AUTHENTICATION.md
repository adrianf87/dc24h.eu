# ADC Authentication — alpha-0.7.8

**alpha-0.7.9 Auth Sessions:** [connection-bound account sessions](AUTH_SESSIONS.md) add atomic readiness binding, trusted account-filtered lookups and per-connection revocation. Closing clears authentication immediately. No new SQL migration, automatic role grants or browser sessions are added. CI uses [GitHub-hosted runners](GITHUB_CI.md) and is not awaited.

Account-backed ADC GPA/PAS is implemented on the existing TCP/TLS reactor. The branch starts from main `a20abc38e731e9cb94b3fbf73aa080e2ccaf273d`; merging requires a separate instruction. Existing guest login remains available for identities absent from the account directory.

## Separate credentials and key

The account password retains its existing fixed-cost scrypt verifier, unchanged. ADC's [GPA/PAS protocol](https://dc-protocols.github.io/ADC.html) requires the original password bytes to compute Tiger(password || challenge). A scrypt verifier cannot produce this response. Following the explicit project decision, a separate ADC password is therefore stored encrypted, never as plaintext, in MariaDB schema v6 (`dc24h_account_adc_credentials`). Use a distinct ADC password.

The envelope is `DC24HADC1` (9 bytes), a random 12-byte nonce, AES-256-GCM ciphertext and a 16-byte tag. Authenticated additional data binds the version, account ID and creation timestamp. Tampering, reassignment or a wrong key fails closed. The password policy matches Registration: at least 15 Unicode scalars, at most 1024 UTF-8 bytes, no C0/DEL; bytes, spaces and composition remain exact.

Generate the external 32-byte key as the hub's service user, in an administrator-provisioned private directory:

```sh
umask 077
openssl rand -hex 32 > /etc/dc24h/adc.key
chmod 600 /etc/dc24h/adc.key
```

The file contains exactly 64 lowercase hex characters and optionally one final newline. It must be a regular, single-hardlink file owned by the effective process UID, with no group/other access and no final symlink. Keep it outside the database and repository and back it up separately. It cannot alias the configuration, database password, API token or TLS files. Log/API targets cannot alias it. Losing the key makes existing ADC credentials unusable; copying it with the database permits decryption. Automatic rotation/recovery/reset is not implemented. The service-owned plaintext/key/challenge buffers are wiped; caller buffers, provider internals, OS dumps and backups remain outside that guarantee.

## Provisioning and startup

1. Explicitly run `dc24h --config /etc/dc24h/dc24h.ini --migrate-database` with migration privileges. SQL 1–5 is unchanged; v6 adds only the encrypted ADC credential table. Startup never migrates automatically.
2. Use the existing trusted C++ Registration, AccountRepository and IdentityRepository APIs to create an account, activate it explicitly and bind its exact ADC nickname/Tiger CID. Keep the client's PID/CID stable. Account name and ADC nickname may differ.
3. Load `AdcSecretKey::load(path)` and call `AdcCredentialService(connection, *key).enroll(account_id, account_password, adc_password, created_at)`. The caller authorizes this administrative operation; the service also proves the original scrypt password. It requires an existing mapping, validates input/time and owns the insert transaction. It refuses existing credentials and never overwrites/reset them. The account may be disabled during provisioning; it must be active to log in. Caller plaintext is borrowed and must be erased by the caller. Enrollment may block on SQL/scrypt and must stay outside the reactor.
4. Enable the configuration below and restart. The schema must be current/clean. The bounded immutable startup directory validates encrypted records before listeners open; invalid storage, key or credentials aborts startup with redacted errors. `--check-config` checks key safety/encoding and TLS files without connecting to MariaDB; it cannot verify database ciphertext. Database status/migration commands do not load account secrets.

```ini
[authentication]
enabled = on
key_file = /etc/dc24h/adc.key
max_accounts = 1024
require_tls = on
```

Database settings and working ADCS listeners/certificates must also be configured (see [Configuration](CONFIGURATION.md)). Authentication defaults off; the key has no default. Relative key paths resolve from the INI directory. `max_accounts` accepts 1–4096 and counts mapped accounts, including disabled/unprovisioned ones; exceeding it aborts startup rather than omitting reserved identities. Runtime loading needs SELECT on migration metadata, accounts, mappings and ADC credentials; it does not need the original scrypt table. Enrollment additionally needs its existing credential-verification and write privileges. No password is accepted through CLI arguments, HTTP or WebAdmin; trusted C++ provisioning is the administrative integration surface in this stage.

**Account status, mappings and credential changes are startup snapshots. Restart the hub after any change or revocation to replace the directory and disconnect old sessions.** There is no hot reload, automatic key rotation or live revocation. SQL and scrypt never run in the network reactor; it decrypts a bounded record only while preparing a challenge, immediately wipes the plaintext and retains only the expected digest until PAS completes.

## Wire behavior

After SUP/SID and validated BINF identity, the exact active account nickname/CID pair receives IGPA with 24 random bytes in canonical unpadded base32. After its final LF has been sent, one HPAS is accepted: canonical base32 of Tiger-192(password UTF-8 bytes || decoded challenge). Comparison uses CRYPTO_memcmp. Challenges are fresh per connection and consumed on every attempt; early/malformed/wrong/replayed PAS fails. Unknown nickname and CID together retain guest behavior. If either is reserved, a mismatched pair, disabled account or missing ADC credential is refused without guest fallback or user-list publication. Authentication refuses plain ADC by default. `require_tls=off` is an explicit operator opt-in for trusted isolated ADC tests; it permits captured fast-Tiger responses to be attacked offline.

Only successful PAS admits a registered session. Successful PAS precedes public user-list admission. The existing own-BINF final-LF barrier still precedes session readiness and chat/search/transfers; welcome follows the own BINF in the output queue. Account association is visible through the trusted LoginHub accessor only after readiness. Roles are not automatically granted and last-login metadata is not updated. Authenticated sessions cannot rename away from their mapped nickname/CID; guests cannot rename into reserved identities. Wire refusal uses generic `ISTA 223 Authentication failed`, with existing bounded output and timeouts. Logs do not include passwords, GPA/PAS payloads or ciphertext.

## Verification

`adc.authentication` covers independent Tiger/base32 fixtures, output-order barriers, malformed/replayed PAS, cryptographic/allocation failures, erasure and lifecycle admission. `accounts.adc_authentication.process` uses a disposable MariaDB, real AES-GCM/scrypt, a runtime user without scrypt-table access, encryption tampering/wrong-key/duplicate checks, real TCP/TLS traffic and ncdc over ADC and ADCS. ncdc proves wrong-password refusal, reconnect with a new challenge, correct-password admission, welcome and chat. Migration tests cover v5→v6 preservation and dirty-history refusal. Existing guest protocol tests remain. Full Debug/Release and WebAdmin CI uses the single Ubuntu 24.04 Contabo runner with six workers; the remote run is started without waiting for completion.
