# Auth Sessions — alpha-0.7.9

**alpha-0.8.1 Permissions:** [the seven-role permission model](PERMISSIONS.md) adds exact capability keys, explicit rank checks, constrained VIP delegation, Guest-aware kicks and transactional maintenance role assignment. Explicit migration v7 broadens stored account roles without changing earlier SQL. Mute, bans, audit browsing and configuration writes remain policy definitions with engines pending. Global limits still apply to Owner.

**alpha-0.7.12 Anti-Bruteforce:** [shared login attempt limits](ANTI_BRUTEFORCE.md) add bounded global/IP/account token buckets, automatic refill and generic terminal refusals across ADC/ADCS and reconnects. Success does not refund attempts; all roles are limited. Counters remain in memory and do not change account data.

**alpha-0.7.11 Account Management:** [trusted password change, disable and reset](ACCOUNT_MANAGEMENT.md) now compose atomic account/credential writes with fresh password proof, revision checks and ADC credential removal. Perform these operations with the hub stopped, then explicitly reenroll/reactivate as appropriate and restart. No live reload, new SQL migration or public management endpoint.

**alpha-0.7.10 Roles:** [validated account roles](ROLES.md) now bind to ready authenticated sessions, seed Search/Transfers grants and authorize staff revocation of lower-role sessions. Global limits and explicit connection restrictions still apply. Stored role changes require restart; no new SQL migration or public administration endpoint.

This stage starts directly from main `f39d552c174941effd4b9e18ff83f8515059b8f8`.
It adds connection-bound authenticated account sessions to the existing Users
module and GPA/PAS adapter. It does not merge the numbered branch into main.

## Authentication and readiness

A password challenge selects a pending account, without creating an authenticated
session. A correct one-shot PAS establishes a temporary verified account in the
private ADC peer. Only after the final LF of that connection's own BINF is
accepted by transport does `UserLogin::complete_welcome` atomically mark the
managed user ready and install `users::AuthSession`. The temporary proof is then
removed. Partial output, malformed/early/wrong PAS, welcome allocation failures
and disconnect before readiness create no authenticated session. Guests retain
ordinary ready sessions without an account association. Client INF fields cannot
supply or change an authenticated account or grant a role.

The [ADC specification](https://dc-protocols.github.io/ADC.html) describes the
protocol negotiation and GPA/PAS exchange. AuthSession is a private hub model,
not a new ADC command or a wire bearer token.

## Data and ownership

An `AuthSession` is an owning value with five fields (the role is added in alpha-0.7.10):

| Field | Meaning |
| --- | --- |
| `session_id` | Nonzero local connection lifetime ID, never an fd, ADC SID or persistent account ID |
| `account_id` | Checked positive signed-BIGINT `accounts::AccountId` |
| `transport` | Actual `adc` or `adcs` transport of that connection |
| `established_at` | Explicit steady-clock readiness time, equal to `Timeline::ready` |
| `role` | Validated startup account role; see [Roles](ROLES.md) for grants and hierarchy |

The single reactor-thread `SessionManager` owns the live association inside the
existing `Session`. It uses fixed-size optional storage; readiness, lookup and
removal allocate nothing. Capacity is bounded by the existing session/user limits.
No password, challenge, response, ciphertext, key, account name or public
profile is added to the value. This timestamp is not UTC database metadata and
never updates `last_login_at`.

`Session::authentication()`, `Snapshot::authentication`,
`SessionManager::authenticated(id)` and `LoginHub::authenticated_session(id)` expose
ready session data to trusted code. The existing `authenticated_account(id)`
accessor delegates to that managed record. Missing, zero, pending, guest, closing
and disconnected IDs yield no account session.

`authenticated_page(account, after, limit)` returns owning account-filtered
records in increasing lifetime-ID order, using an exclusive cursor and a limit
of 1–256. Scans are bounded by the configured registry capacity. Pages are
independent observations, not a frozen transaction. Allocation failure while
copying a page leaves live state unchanged. Saved snapshots remain historical
values after close; re-resolve the lifetime ID before every privileged operation.
Copies and numeric account IDs are not authentication evidence.

The manager's `mark_authenticated_ready` and the optional account input of
`UserLogin::complete_welcome` are trusted adapter operations, not public login
APIs. Their caller must prove credentials and enforce account policy first.
They reject missing IDs, wrong lifecycle phases and backwards times without
mutation. A ready guest cannot be upgraded, and an existing authenticated session
cannot be rebound to a different account. Authenticated nickname changes are
also rejected by the managed model before nickname-index mutation; profile
updates retaining the same identity remain possible.

## Closing, revocation and reconnect

Every successful begin-close, forced-close and direct finish-close clears the
live account association immediately, including protocol failure, EOF, idle
expiry, output failure and shutdown. Output draining cannot retain authentication.
Removal releases existing public identity reservations; the monotonic registry
watermark rejects reused lifetime IDs. Reconnect needs a fresh GPA/PAS exchange
and creates a new AuthSession only after its own readiness barrier. Existing
nickname/CID uniqueness still applies; this stage does not introduce concurrent
logins for one mapped identity.

Trusted owner-thread code may call `LoginHub::revoke_auth_session(id, now)`.
For a ready authenticated lifetime it atomically forces closing with the fixed
`authentication_revoked` cause, clears authentication and outstanding search
windows without allocation, and lets the reactor disconnect the socket on its
next pass. It sends no new payload and cannot resume the old connection.
Missing, guest, already-closed and backwards-time requests return false.
The opt-in connection log records only its fixed event and local lifetime ID.

This operation revokes that connection only. It does not disable an account,
change the immutable startup directory or block a fresh valid login. Persisted
account status, mapping and credential changes still require restart. Live
directory reload, account-wide bans, custom role policies, browser sessions,
JWT/cookies, a public revocation API, persistence of active sessions and a new
absolute session TTL are outside this stage. Existing network idle/login/close
timeouts remain active. No SQL migration is added; schema v6 and credential
storage are unchanged.

## Verification

`users.auth_sessions` covers lifecycle/time gates, checked account binding,
no-allocation readiness/cleanup, guest isolation, immutable identity, bounded
account-filtered pages, stale IDs and snapshot ownership. `adc.authentication`
adds final-LF publication, live model consistency, trusted revocation under
allocation failure, post-revocation traffic refusal and reconnect isolation.
`accounts.adc_authentication.process` adds repeated authenticated disconnect and
fresh login over TCP and TLS, alongside its real ncdc wrong/correct-password,
welcome and chat checks and disposable MariaDB credential tests.

Debug/Release and WebAdmin run on [GitHub-hosted Ubuntu 24.04](GITHUB_CI.md),
using `nproc` for build/test/browser workers. Remote CI is started without
waiting for completion; the changelog distinguishes local validation from CI.
