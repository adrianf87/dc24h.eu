# Password Security — alpha-0.7.7

Based directly on main `75c82eea4d5799460bebe93ed1b6accef175ce25`.
This increment retains Registration and Identity Mapping, adds a trusted
credential-check API and leaves SQL migrations 1–5 unchanged. It does not merge
itself into main or change a production database.

## Public contract and boundaries

Link `dc24h_password_security` and include
`dc24h/accounts/password_security.hpp`. `PasswordSecurityService` borrows a
Database Core connection on its owning thread. The constructor performs no I/O.
`verify(AccountId, password)` returns only one enum:

| Result | Meaning |
| --- | --- |
| `matched` | Supplied password matches the verifier snapshot read for that account ID. |
| `mismatch` | A valid stored verifier was checked with the fixed KDF and did not match. |
| `unavailable` | That account ID has no credential row, including an absent account or a metadata-only account. |

The return value contains no password, salt, verifier, database result or borrowed
view. The API is a trusted synchronous building block, not an authentication
endpoint. Callers authorize credential checks, bound concurrent KDF work and keep
them outside the Network reactor. There is no ADC password command, HTTP route,
WebAdmin action or secret-bearing CLI. Password bytes are borrowed for the call;
the caller owns their storage and erasure.

A match does not activate an account, grant a role, set last-login time, resolve
CID/nickname ownership or admit a session. Disabled accounts may match. Callers
must separately apply account status, authorization and session policy. A result
concerns the row snapshot read, not a guarantee that an administrator has not
changed credentials afterward. No lock or transaction is held during the KDF.

The existing v1 password policy is retained: valid UTF-8, at least 15 Unicode
scalar values, at most 1024 bytes, without NUL, C0 controls or DEL. Spaces, case
and Unicode composition are preserved exactly; there is no trimming,
normalization or truncation. Invalid input fails before SQL or expensive work.

## Strict verifier and cryptography

The Registration v1 envelope is unchanged:
`$dc24h-scrypt$v=1,ln=17,r=8,p=1$<64 lowercase salt hex>$<64 lowercase key hex>`.
The decoder requires its exact length, prefix, separators and lowercase hex.
It rejects trailing bytes, embedded NUL, unsupported versions/algorithms/costs,
alternate encodings and oversized records before invoking scrypt. Parameters
are constants, never parsed into a caller-selected KDF configuration.

Verification uses existing OpenSSL `EVP_PBE_scrypt` with **N=131072, r=8, p=1**,
32-byte salt and 32-byte output, and a 256 MiB provider memory ceiling. A call
needs roughly 128 MiB plus overhead; that ceiling is not a global concurrency
budget. Registration continues using fresh random salts and the same fixed
parameters. There is no weak fallback or exported provider/test injection.

The final 32-byte keys are compared with OpenSSL `CRYPTO_memcmp`. Its comparison
does not depend on the key contents; this is not a constant-time claim about
the complete operation. Database access, missing credentials, invalid inputs,
format checks, allocations and scrypt can take different amounts of time. A
future public authentication layer must define admission limits, transport,
rate limiting, account-enumeration behavior and concurrent KDF budgets.

Decoded salt, expected key and derived key use fixed noncopyable buffers wiped
by `OPENSSL_cleanse` on normal and exceptional exits. The bounded credential
cell owned by the service is borrowed during verification and wiped before its
result is destroyed, including format/provider failures. No verifier copy is
made by the service. Registration's existing cleansing remains intact.

These guarantees cover explicitly owned application buffers, not caller
plaintext, registers, allocator spare capacity, Connector/C or OpenSSL internals,
SQL logs, swap, core dumps or backups. Failed queries can leave data inside the
database layer before an owning result is returned. Use restricted process and
database access, protect backups and do not log arguments or SQL credentials.

## Database access and errors

The service requires explicit current clean migrations and compatible account
and credential storage. It reuses existing schema checks; ordinary verification
does not migrate, register, reconnect, retry writes or modify any table.
The credential query is a prepared `SELECT` bounded to one row, one column and
192 bytes. Stored verifier validation is stricter than the schema's size bound.

A runtime account needs SELECT on `dc24h_schema_migrations`, `dc24h_accounts`
and `dc24h_account_credentials`. It needs no INSERT, UPDATE, DELETE, DDL or
access to identity mappings. Missing visibility of credential storage fails
closed. Existing schema compatibility checks do not audit all CHECK expressions,
triggers, referential actions or concurrent administrator DDL.

`PasswordSecurityErrorKind` distinguishes `invalid_password`, `invalid_record`,
`crypto` and `schema`, with fixed EN/US text containing no input or SQL. A
provider failure throws an error rather than returning a match or falling back
to weaker work. Account-repository, migration and Database Core errors can
propagate with their existing redacted diagnostics. Caller transactions and
wrong-thread connections are refused.

## Verification and remaining work

`accounts.password_security` checks an independently generated scrypt fixture,
correct/wrong passwords, every malformed verifier byte, unsupported cost before
KDF work, exact whitespace/Unicode composition and maximum password length.
Executable-local wrappers check fixed production parameters, use of the
constant-time comparator, provider failure and cleansing. The normal checks
execute real production scrypt.

`accounts.password_security.process` uses private MariaDB with networking
disabled. It checks real registered credentials, absent/metadata-only accounts,
unchanged metadata, read-only and minimal SELECT privileges, denied credential
access, malformed records, foreign-key drift, wrong threads, nested transaction
refusal, provider errors, owned-record wiping and allocation-failure sweeps.
Only those sweeps replace expensive KDF work inside the test executable.
The product has no such configuration.

Release metadata validation now also checks the CLI/log/API and browser version
fixtures, preventing a version bump from leaving old test expectations.
See [Testing](TESTING.md), [Contabo CI](CONTABO_CI.md) and
[Changelog](../../CHANGELOG.md) for actual results. Remote CI runs Debug/Release,
ncdc ADC/ADCS and WebAdmin on the single Ubuntu 24.04 runner with six parallel
workers. Its completion is not awaited during this task.

Credential rotation/reset, adaptive rehashing, alternate formats, breached-password
screening, authenticated account activation/session admission and public
account endpoints remain future stages. No new SQL migration is needed.

References: [OpenSSL key comparison](https://docs.openssl.org/3.0/man3/CRYPTO_memcmp/),
[OpenSSL scrypt](https://docs.openssl.org/3.0/man7/EVP_KDF-SCRYPT/),
[OWASP password storage](https://cheatsheetseries.owasp.org/cheatsheets/Password_Storage_Cheat_Sheet.html).
