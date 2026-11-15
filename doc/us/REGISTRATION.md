# Registration — alpha-0.7.5

Based directly on main `493e146af8192e0a87288261a9c9548ec2ee66da`, retaining Database Core, Migrations, Account Model and Account Repository. Branch `alpha-0.7.5` is not merged automatically.

## Contract and scope

Link CMake target `dc24h_registration` and include `dc24h/accounts/registration.hpp`. `RegistrationService` borrows a Database Core connection and exposes `create_account(name, password, created_at)`. It returns an owning `StoredAccount` after one successful commit. The constructor performs no I/O. The connection must outlive the service, remain on its owning thread and have no active caller transaction.

This is a trusted synchronous C++ application service. Callers supply authorization and run it outside the Network reactor; it is not exposed through ADC, HTTP, WebAdmin or a password-bearing CLI. Registration does not change guest login, perform password authentication or grant a session permissions. Each new account is **registered/disabled**, with revision 1, the supplied UTC second for both creation/update, and absent last login. There is no role/status input, account ID input or existing-account credential attachment. Metadata accounts from v3 remain intact without invented credentials.

| Input | Validation |
| --- | --- |
| Name | Existing `AccountName` policy: already-NFC UTF-8, 1–64 bytes, without controls/format/separators. Exact name bytes and case determine uniqueness; no trimming/case folding. |
| Password | Valid UTF-8, at least 15 Unicode scalar values and at most 1024 bytes. Reject NUL, C0 controls and DEL. Preserve spaces, case and Unicode composition exactly; no normalization, trimming, truncation or composition rules. Bounds do not establish password entropy. |
| Creation time | Explicit UTC Unix seconds in 0–253402300799. No implicit clock/timezone conversion; caller supplies its trusted time policy. |

Arguments borrow memory for the entire call. The caller owns and clears its plaintext; the service never stores a plaintext copy or includes passwords/verifiers in the account snapshot, errors, logs or public API. Do not pass secrets through process arguments or log request structures. Account names are ordinary metadata and are stored in the account table.

## Password storage

The existing OpenSSL dependency supplies `RAND_bytes` and `EVP_PBE_scrypt`. Fixed production parameters are **N=131072, r=8, p=1**, with a fresh 32-byte random salt and 32-byte derived key. The KDF uses roughly 128 MiB plus overhead, with a 256 MiB provider memory ceiling. No caller-selected cost, test mode or weak fallback is exported. Randomness/KDF failure returns a fixed `crypto` error and creates no account. The KDF executes before starting the write transaction or taking row locks.

Stored format: `$dc24h-scrypt$v=1,ln=17,r=8,p=1$<64 lowercase salt hex characters>$<64 lowercase key hex characters>`. It is an application-defined, versioned envelope, not a promise of compatibility with another library's PHC parser. Future verification must allowlist the format and bound its parameters before derivation. Credential rotation, rehashing, password reset and verification belong to later increments.

Temporary salt/key/envelope buffers are fixed, noncopyable and cleansed on scope exit. The one application-owned heap copy of the envelope used as a binary prepared parameter is cleansed before release. This does not promise erasure of caller memory, registers, OpenSSL internal allocations, Connector/C copies, SQL buffers, backups or external observability. Keep credential tables and backups access-controlled. Neither credentials nor the result belong in logs.

The selected costs follow the documented scrypt alternative when Argon2id is unavailable in the existing cross-platform dependency set. No new package or vendored cryptographic implementation is introduced. References: [OWASP password storage](https://cheatsheetseries.owasp.org/cheatsheets/Password_Storage_Cheat_Sheet.html), [OpenSSL scrypt](https://docs.openssl.org/1.1.1/man7/scrypt/), [OpenSSL randomness](https://docs.openssl.org/1.1.1/man3/RAND_bytes/).

## Persistence and errors

Explicit migration v4 creates InnoDB `dc24h_account_credentials`: `account_id BIGINT` primary/foreign key to `dc24h_accounts.id`, `verifier VARBINARY(192)` and explicit UTC `created_at BIGINT`. The foreign key restricts updates/deletes of referenced identities. Existing SQL migrations 1–3 are byte-for-byte unchanged. Fresh databases apply 4 migrations, upgrades from v1 apply 3, v2 apply 2 and v3 apply 1; latest status is `version=4 latest=4 pending=0 dirty=no`. Ordinary startup never migrates. Back up before explicit operator migration; v4 uses the existing durable applying/applied journal and refuses blind replay after partial DDL.

Before hashing, registration requires a current clean migration history, compatible account storage and compatible credential engine, columns, primary key and account foreign-key mapping. This is a bounded compatibility check, not an audit of all defaults, constraints, triggers, external writes or concurrent administrator DDL. Referential action rules are defined by the shipped migration; read-only metadata visibility must not require broad DDL privileges. Use separate migration and runtime identities. Runtime registration needs SELECT on migration/account/credential metadata and INSERT on account/credential tables; it needs no DDL, DELETE, UPDATE or blanket administrative grant. Repository updates have their own UPDATE requirement.

One private composition point in Account Repository inserts metadata inside the registration-owned RAII transaction. Credentials are inserted before the same commit. A duplicate, privilege error, allocation failure or other known pre-commit failure rolls back both. Existing accounts and their credentials are never overwritten by a duplicate registration. Competing exact names yield one committed account/credential pair. IDs may have gaps after rollback. Public repository methods continue owning their transaction and reject callers' transaction scopes.

`RegistrationErrorKind` is `invalid_name`, `invalid_password`, `invalid_time`, `crypto` or `schema`, with fixed EN/US text. Existing repository duplicate/schema errors and Database Core/Migration errors propagate. A database error preserves `outcome_unknown`: after a lost execution/commit acknowledgement, inspect through a new connection and reconcile before any operator-directed retry. There is no automatic reconnect or write replay; atomicity does not imply certainty about an unacknowledged commit.

## Verification and next increments

`accounts.registration` checks UTF-8/character/byte boundaries, exact whitespace behavior, an independently generated scrypt fixture, deterministic random/KDF failures and buffer cleansing. `accounts.registration.process` runs on disposable MariaDB, checks default role/status/time, distinct random salts, independent Python `hashlib.scrypt` verification of stored records, duplicate preservation, racing registrations, restricted privileges, schema drift, rollback and allocation-failure sweeps through successful writes. Only the allocation sweep replaces KDF work with an executable-local wrapper; normal unit/integration checks execute the fixed real production KDF. No provider injection is part of the shipped library.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DDC24H_REQUIRE_INTEGRATION_TESTS=ON
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure --parallel 4
ctest --test-dir build -R '^accounts.registration' --output-on-failure
```

Existing real ncdc ADC/ADCS, MariaDB and Apache checks remain required. The pinned Proxmox workflow from `github-runner-v0.1` tests Ubuntu 24.04/22.04; that branch is reused without merging. See [CI](CI_DEBIAN13.md) and [Testing](TESTING.md) for actual coverage.

Public registration/admission budgets, breached-password screening, authenticated activation, password verification/reset, account/session binding, role grants and account UI/API remain future work. A future public endpoint must bound concurrent KDF workers and protect its transport before accepting secrets. No production hub, host database or user account is deployed by these tests.
