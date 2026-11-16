# Anti-Bruteforce — alpha-0.7.12

This stage starts from main `420715822970698018dd093d16b2f5486b676da1`.
It adds bounded authentication-attempt token buckets to the existing single
ADC/ADCS LoginHub, preserving one-shot GPA/PAS, TLS policy, roles and account
management. The numbered branch is not merged automatically. No SQL migration,
password format or public management endpoint is added.

## Three shared budgets

When an authentication source is configured, every policy-approved initial BINF
passes a global and source-IP admission check before the authentication source
prepares its decision/challenge. This includes guest and denied-identity decisions
on an account-enabled hub. A valid password-required decision additionally charges
a bucket keyed by the trusted account ID before any IGPA is queued. This account
budget is shared across IPs and ADC/ADCS listeners, including reconnects.

| INI key in `[authentication]` | Default | Range | Meaning |
| --- | --- | --- | --- |
| `attempts_per_ip` | 20 | 1–100000 | Source-IP initial burst and average attempts per configured window |
| `attempts_per_account` | 10 | 1–100000 | Known account's shared burst and average attempts per window |
| `attempts_per_second` | 128 | 1–100000 | Hub-wide initial burst and average attempts per second |
| `attempt_window_seconds` | 60 | 1–3600 | Refill/retention horizon for IP and account buckets |
| `max_tracked_attempt_ips` | 4096 | 1–65536 | Maximum source-address entries |
| `max_tracked_attempt_accounts` | 4096 | 1–65536 | Maximum trusted account-ID entries |

These are token buckets, not fixed or strict rolling windows. They begin full
and refill continuously at the configured average rate, capped at the initial
burst. With defaults, an exhausted IP recovers one token every three seconds,
and an exhausted account one every six seconds. Correct PAS, final readiness,
disconnect, SUP/INF profile updates and new sockets do not refund or reset credit.
All roles, including administrator, use the same admission gates.

```ini
[authentication]
# Existing enabled/key_file/max_accounts/require_tls settings still apply.
attempts_per_ip = 20
attempts_per_account = 10
attempts_per_second = 128
attempt_window_seconds = 60
max_tracked_attempt_ips = 4096
max_tracked_attempt_accounts = 4096
```

Zero/unknown/duplicate/out-of-range settings fail validation, also when account
authentication is disabled. Limits activate when a trusted authentication source
is installed. Pure guest hubs without one retain their existing behavior.
Tune budgets for legitimate reconnects and shared NATs: guest joins on an
account-enabled hub share the source-IP/global budget with password attempts.
Earlier connection/pending/identity/frame/byte limits still apply; raising a new
attempt limit does not bypass them.

## Refusal, recovery and accounting

Admission charges attempts before the outcome is known. Wrong/malformed/early
PAS, abandoned challenges and password successes therefore cannot be used to
reset failed-attempt state. Once a challenge has been admitted, its one-shot
response may complete normally even if another connection later exhausts a
shared bucket. Invalid pre-identity protocol input remains governed by existing
negotiation and identity quotas.

A depleted bucket or full tracking table fails closed with the same generic
`ISTA 223 Authentication failed` and own IQUI as ordinary password refusal.
Throttled attempts receive no GPA, user list, ready account binding or role.
Account-throttled attempts may have prepared a bounded challenge internally;
it is canceled and its expected digest erased before any IGPA is emitted.
No sleep, SQL, password KDF or blocking timer is added to the reactor.

Recovery requires another connection after sufficient token refill; the old
connection stays terminal. Successful users do not clear the budget. Account
status, role, credentials and database revisions are never modified by throttling.
Already-ready connections continue operating. This is temporary admission
throttling, not a persistent account ban or a password reset.

Normal quota refusals contribute to the existing aggregate network
`security_closed` counter. Opt-in connection logging uses only the fixed
`authentication_limit` event and local lifetime ID; the warning is emitted once
per reactor run. Neither path logs source keys, account/nickname/CID, challenges,
responses, passwords or remaining account budget. No new per-account data is
published through HTTP or WebAdmin.

## Bounded state and ownership

`security::AuthenticationLimiter` owns separate IP/account maps in one reactor
thread. Numeric addresses are parsed once into fixed-size binary keys: IPv6
spelling variants compare equal, and IPv4-mapped IPv6 uses the same key as IPv4.
Ports and client INF/address claims cannot choose a bucket. No DNS or proxy-header
identity is used. Accounts are numeric IDs from the trusted password preparation,
not arbitrary client nicknames or guessed numeric fields.

Each map is independently capped. Global denials create no IP entries; invalid
account IDs create no account entries. Existing-key checks allocate nothing.
An allocation failure does not debit an uncommitted bucket; the hub fails the
connection closed, without granting access or exposing a challenge. Earlier-stage
IP/global debits remain conservatively charged if a later stage fails.

Entries become eligible for removal once their last admitted attempt is a full
configured window old, when their credit is fully recoverable. Denied retries do
not extend that retention age. Pruning scans bounded maps at most once per second
of limiter activity; an idle hub performs no background sweep. Exhausted entries
are not evicted early merely to admit new keys. Full maps temporarily deny new
keys until eligible entries are reclaimed. Backwards supplied time is clamped;
refill arithmetic saturates safely across the supported clock horizon.

State is process-local and intentionally survives connection cleanup, but not a
hub restart or a new LoginHub instance. Cluster-wide counters, persistent bans,
IP-prefix aggregation, adaptive delays, MFA/CAPTCHA, external proxy attribution
and throttling of future public management APIs are outside this stage. Shared
account throttling can delay legitimate login under a targeted distributed
attack; it never disables the stored account. Deployment limits should reflect
expected traffic and may need external admission protection.

Login throttling and lockout tradeoffs are discussed in the primary
[OWASP Authentication guidance](https://cheatsheetseries.owasp.org/cheatsheets/Authentication_Cheat_Sheet.html#login-throttling).
This implementation applies the concrete token-bucket and bounded-state policy
above, without claiming to eliminate offline attacks against captured ADC hashes.

## Verification

`security.authentication_limits` covers validation, canonical address aliases,
per-IP/account/global independence, exact fractional refill boundaries, shared
ADC/ADCS/reconnect/account/administrator budgets, success without refunds,
backwards/extreme time, cache caps/pruning and allocation failures without lost
uncommitted credit. LoginHub checks verify generic terminal refusal with no
challenge/readiness and recovery after refill.

`accounts.adc_authentication.process` adds real MariaDB-backed TCP/TLS attempts
from multiple loopback IPs, shared IP/account throttling, unaffected other accounts,
recovery with a fresh challenge, configuration bounds and private log checks.
Existing real ncdc wrong/correct-password, reconnect, welcome and chat checks
remain enabled. [GitHub CI](GITHUB_CI.md) runs hosted Ubuntu 24.04 Debug/Release
and WebAdmin with all available CPU workers via `nproc`; remote completion is not
awaited. See [Testing](TESTING.md) and [Changelog](../../CHANGELOG.md) for results.
