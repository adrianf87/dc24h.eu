# Account Management — alpha-0.7.11

**alpha-0.8.1 Permissions:** [the seven-role permission model](PERMISSIONS.md) adds exact capability keys, explicit rank checks, constrained VIP delegation, Guest-aware kicks and transactional maintenance role assignment. Explicit migration v7 broadens stored account roles without changing earlier SQL. Mute, bans, audit browsing and configuration writes remain policy definitions with engines pending. Global limits still apply to Owner.

**alpha-0.7.14 WebAdmin Accounts:** [local account maintenance](WEBADMIN_ACCOUNTS.md) adds paginated checked metadata, disabled registration, activation, disable/reset and password changes through a separate non-serving executable. Stop serving hubs before writes; fresh administrator proof and revision checks protect changes. Schema v6 and startup authentication snapshots remain. CI uses four workers per command and is not awaited.

This stage starts from main `c81c126b082c10833d4f85829c948fd2ee125184`.
It adds the trusted synchronous C++ `dc24h_account_management` library for account
password changes, administrative disabling and password resets. It does not merge
the branch automatically, add a SQL migration or expose public ADC/HTTP/WebAdmin
management commands. Existing schema v6 and fixed-cost scrypt remain in use.

## Operations and authorization

| Operation | Required proof and policy | Result |
| --- | --- | --- |
| `change_password(expected, current_password, new_password, changed_at)` | Exact fresh active account snapshot and current account-password proof; replacement must differ byte-for-byte | Replace salted scrypt verifier; preserve active status/role; remove ADC credential |
| `disable_account(actor, actor_password, expected, changed_at)` | Fresh active administrator snapshot and its password; target is registered/moderator, never self/administrator | Disable target; preserve its scrypt verifier; remove ADC credential |
| `reset_password(actor, actor_password, expected, new_password, changed_at)` | Same administrative policy; target may already be disabled or lack its scrypt row | Create/replace target verifier with a new random salt; leave target disabled; remove ADC credential |

The target name, role, ID, creation time, last-login history and identity mapping
are preserved. Successful operations increment the target revision exactly once
and update its explicit UTC timestamp. Separate administrator metadata is not
changed. Times must be within the existing account range and at least both
snapshots' update times. Disabled accounts cannot change their own password.
Administrator accounts can change their own password with proof; resetting or
disabling administrator accounts through the staff API is intentionally denied.
There is no anonymous administrator recovery mechanism in this stage.

`security::Permission::manage_accounts` is administrator-only;
`security::can_manage` also rejects administrator targets and unknown values.
A numeric account ID, supplied role or saved AuthSession is insufficient proof.
Both snapshots are re-read from MariaDB; forged/stale metadata, actor demotion,
disabling and concurrent password changes cannot reuse a pre-lock proof.
The trusted caller must still bind a request to its actual authenticated actor
and enforce transport security, request throttling and user-facing error policy
before exposing any future endpoint.

Passwords retain Registration's 15-Unicode-scalar minimum, 1024-byte UTF-8 maximum,
control rejection and exact byte semantics. No normalization or trimming is
introduced. Verification uses the existing strict fixed-cost scrypt parser and
constant-time derived-key comparison. Every replacement uses a fresh random
32-byte salt. There is no password-history rule or automatic secret generation.
The caller supplies and owns all plaintext; no secret appears in the returned
`StoredAccount` or fixed error messages.

Sensitive changes require fresh password proof, following the general principle
of [OWASP reauthentication guidance](https://cheatsheetseries.owasp.org/cheatsheets/Authentication_Cheat_Sheet.html#require-re-authentication-for-sensitive-features).
The administrative reset is a trusted maintenance operation, rather than an
email/token recovery flow; such flows remain separate work (see [OWASP recovery guidance](https://cheatsheetseries.owasp.org/cheatsheets/Forgot_Password_Cheat_Sheet.html)).

## Transactions and failure behavior

Each call validates current/clean account, scrypt and ADC storage before writing
and refuses caller-owned transactions or the wrong connection thread. It derives
and checks credentials before holding row locks. Inside one owned transaction it:

1. Locks actor and target account rows in increasing account-ID order and compares
   complete expected snapshots/revisions.
2. Locks and rechecks the exact captured administrator/owner proof record,
   including verifier and credential creation time. A credential-only change that
   did not increment account revision still invalidates the earlier proof.
3. Updates the target metadata/revision, replaces its verifier when needed and
   deletes its encrypted ADC credential.
4. Commits once, returning a prepared owning metadata value without a post-commit
   allocation. Failures before commit roll back all writes.

ADC enrollment also captures the exact account/password snapshot used for its
proof and rechecks both under account/credential row locks before inserting an
envelope. An enrollment started with an old password cannot restore ADC access
after a password change/reset/disable or a credential-only concurrent write.

The private AccountRepository composition methods are available only to trusted
service friends; ordinary public repository writes still own their transaction.
Credential tables must pass the existing engine, column, key and foreign-key
checks so the composed operation remains transactional. Keep schema migrations
and maintenance writes serialized; the service performs no DDL.

`ManagementError` contains fixed categories: invalid input/password, unauthorized,
stale, invalid record, crypto and schema. Database errors retain the existing
`outcome_unknown` flag. No automatic reconnect or write retry occurs. After a lost
commit acknowledgment, inspect current state from a new maintenance connection
before deciding what happened. Map internal categories to a suitable generic
response in any future public adapter.

Application-owned captured/locked verifier cells, encoded replacement buffers
and bound verifier bytes are wiped on normal and exceptional exits. Caller
buffers, database connector/provider internals, OS dumps and backups remain
outside this guarantee.

## Required maintenance workflow and ADC credentials

**Stop the hub before performing these operations.** Existing network instances
keep immutable startup account/role/credential directories; a database write does
not itself change that directory, cancel a pending GPA or disconnect live sockets.
Per-session revocation alone cannot stop fresh login against an old directory.
This stage does not implement hot reload or a background management command queue.

1. Stop the serving hub and use a separate maintenance connection with only the
   required database privileges. Read fresh account snapshots.
2. Perform the selected operation. Account ownership passwords and ADC passwords
   are separate: all three operations deliberately delete the old ADC envelope.
3. For a password change, enroll a fresh distinct ADC password with
   `AdcCredentialService::enroll` using the new account-password proof and the
   existing external key. For a reset, first explicitly reactivate the account
   through trusted AccountRepository administration if intended, then enroll
   using the reset password. Disabling alone needs no reenrollment or activation.
4. Restart the hub to load the new policy and secrets. Missing ADC credentials and
   disabled/mismatched mapped accounts remain reserved and fail authentication;
   they do not fall back to guest access. Clients use the newly enrolled ADC
   password, never the account ownership password.

The API borrows passwords and returns metadata only:

```cpp
#include "dc24h/accounts/management.hpp"
// connection, current snapshot, borrowed secrets and UTC time are trusted inputs.
auto updated = dc24h::accounts::AccountManagementService(connection)
    .change_password(current, old_secret, new_secret, utc_changed_at);
// Re-enrollment and restart are explicit maintenance steps described above.
```

Management needs SELECT on migration metadata/accounts/credentials, UPDATE on
accounts, INSERT/UPDATE on scrypt credentials for change/reset, and DELETE on ADC
credentials. Disable does not write the scrypt table. It needs no decryption key
or ADC plaintext. Reenrollment separately requires the existing private key and
its own documented privileges. Startup runtime readers need no new write grants.
No passwords are accepted by a new CLI option or public endpoint, emailed or
logged. Reset never automatically activates a user or grants a role.

## Verification

`accounts.management.process` uses a disposable MariaDB and real scrypt/AES-GCM
to cover proof/role/time/input failures, exact revision changes, last-login and
mapping preservation, replacement salts, old-password refusal, disabling, reset
of disabled/missing-credential accounts, ADC invalidation and explicit reenrollment.
It verifies transaction rollback when DELETE privilege is missing after earlier
writes, nested-transaction refusal, malformed schema, actor demotion and
credential-only proof races, and two concurrent reset requests with exactly one
successful revision update.

An allocation-failure sweep uses a test-only fake KDF to visit C++ allocation
failures through success, checking unchanged account/verifier/ADC rows after
rollback and sensitive-buffer erasure. Production has no reduced-cost KDF mode.
The final reset account is explicitly reactivated and enrolled with a fresh ADC
password; real ncdc then proves wrong-password refusal and correct ADC/ADCS login,
reconnect, welcome and chat. Existing authentication, roles, repository and
registration suites remain enabled.

[GitHub CI](GITHUB_CI.md) runs hosted Ubuntu 24.04 Debug/Release and WebAdmin with
`nproc` parallelism. Remote completion is not awaited. See [Testing](TESTING.md)
and [Changelog](../../CHANGELOG.md) for actual local results and remaining limits.
