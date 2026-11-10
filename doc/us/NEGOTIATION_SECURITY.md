# Negotiation security — alpha-0.2.10

**Current increment — alpha-0.3.12:** [Limits](LIMITS.md) adds configurable session/user ceilings shared by ADC/ADCS, bounded by the connection cap. SessionManager checks capacity transactionally; LoginHub rejects a full user registry before welcome/list preparation with ISTA 211 and own IQUI. Closing reservations count until cleanup. Earlier descriptions retain their historical scope.

This increment adds cumulative initial-login input budgets, bounded pending admissions and shared pre-verification rate gates. Its base is main `a3ca70d65a0fcbb563147e8a44a1ce2669a0ec53`, following the alpha-0.2.9 STA/QUI merge. The numbered branch preserves history and is not merged automatically. The ADC protocol, guest admission model, API v1 and database schema are unchanged.

## Limits and configuration

All new settings are optional positive integers in INI `[security]`; defaults apply when omitted. No new CLI switches are introduced.

| Setting | Default | Accepted range | Scope |
| --- | --- | --- | --- |
| max_pending_logins | 256 | 1–65535 | All accepted but not yet ready TCP/ADCS sockets in this reactor |
| max_pending_per_ip | 64 | 1–65535 | Pending sockets for one canonical kernel source IP |
| max_login_bytes | 65536 | 1–16777216 | Cumulative plaintext input bytes per pending connection |
| max_login_frames | 64 | 1–65535 | Cumulative decoder results per pending connection |
| max_identity_checks_per_minute | 64 | 1–100000 | Initial INF attempts per source IP in a fixed minute |
| max_identity_checks_per_second | 128 | 1–100000 | Initial INF attempts across this hub in a fixed second |

Existing total/per-IP connection and accept limits, per-second frame budgets, invalid-frame caps, absolute TLS/login/partial-frame deadlines, output caps and terminal close deadlines continue to apply. Effective capacity is the minimum of the overlapping limits: a pending cap above max_connections does not expand overall capacity. Limits describe local resource policy, not ADC wire limits or production capacity guarantees.

## Admission and readiness ownership

PeerLimiter checks global and per-IP pending capacity before allocating TLS state or registering a client socket. A rejected admission closes the socket without protocol output or a TLS context; it increments the existing rejected/security_rejected counters and does not consume a new pending reservation. Existing per-IP accept accounting still counts successful application admissions only.

Every admitted TCP or TLS connection holds a pending slot, including incomplete TLS, PROTOCOL, IDENTIFY and terminal draining before readiness. Valid CID/PID or internal NORMAL alone does not release it. The slot is completed exactly once after the client's own final initial INF has been fully sent, at the same point as login_ready. Later disconnect of a ready socket releases active capacity without consuming another connection's pending slot. Failed initialization, watch registration, EOF, fatal reports, protocol rejection, timeout, failed welcome and shutdown release a pending slot during actual cleanup.

The existing PeerLimiter map remains bounded to 65536 source keys. Source identity uses trusted numeric kernel addresses; IPv4-mapped IPv6 normalization is shared across TCP/ADCS. Advertised I4/I6, nickname, CID and PID cannot choose a different quota bucket. Source keys stay in memory, never operator logs. Ready users retain active slots but no pending slot.

The library is single-owner/thread. complete_login is called once for each completed pending admission; release(peer, negotiating) is called exactly once with the connection's actual readiness history. Transport owns that history in its per-connection login flag. These methods are not an externally exposed API for clients to manipulate counts.

## Cumulative input through login

NegotiationBudget has independent byte/frame counters and a sticky failure flag, with overflow-safe arithmetic and no reset/window/clock. Bytes include terminating LF, partial frames, invalid syntax and oversize-discarded input. Each current stream segment is charged before decoder allocation/parsing. A segment exceeding the remainder is rejected without parsing that segment; bytes already read from the socket still count as received transport bytes.

Every delivered decoder result—valid message, invalid frame or keepalive—consumes a frame unit before admission handling. Fragmentation, coalescing, keepalives, waiting for the next rate window or moving from PROTOCOL to IDENTIFY cannot renew these lifetime budgets. The frame-rate limiter remains an independent first gate. Charges end only at completed readiness, not SUP/SID/identity approval. Once closing begins, input cannot advance negotiation; existing bounded terminal processing applies.

A live cumulative-limit breach triggers one sanitized fatal ISTA 240 (Negotiation resource limit exceeded), followed by own IQUI when a SID is assigned. Existing immutable-front/queue-byte accounting, no-data-after-own-QUI and close_timeout policy remain. Pending capacity is held through that terminal drain. An unusable/overfull transport may close without delivered notification. No per-byte or per-frame warning flood is added.

## Gate before initial identity work

LoginHub exposes a transport-independent needs_identity_check predicate: live, nonclosing IDENTIFY with no retained identity and an incoming BINF. The reactor must acquire per-IP minute and hub second credit before invoking ClientSession/INF/Tiger validation for such a candidate. The predicate does not validate that candidate or grant identity approval.

Credits count attempted initial INF submissions, including malformed identities, bad source/field claims, mismatched Tiger pairs and later nickname/CID rejection; they are not a count of successful hashes or registered-user passwords. The gate intentionally occurs before those semantic checks. Normal INF metadata updates require no new identity credit because their PID is forbidden and CID immutable. Lower-level standalone ADC modules remain reusable validators without a network quota policy; embedders must enforce their own admission policy.

The minute window belongs to the source key, not the socket. It survives failed login, successful readiness, disconnect and reconnect. Pruning retains inactive source entries until both accept and identity windows have expired; expiring an earlier accept window cannot erase a newer identity window. Active entries are retained. The hub second window spans every source and transport. A per-IP denial does not consume global credit; a global denial does not consume a per-IP attempt or start/reset its minute window. The minute starts at the first charged attempt. Denials do not extend an already running window.

These are fixed windows, so traffic near a boundary can burst across two windows. NAT/shared-IP users share their minute quota; tune it deliberately for the deployment. No persistent ban, account/password lockout, identity ownership proof beyond existing CID/PID verification, distributed limiter or external reputation service is introduced.

A denied attempt never reaches INF/Tiger work. It triggers one fixed fatal ISTA 240 (Identity verification rate limit exceeded), own IQUI where assigned, and existing bounded cleanup. No PID, claimed identity, source address or response hash enters the message or logs; there is no automatic retry/ban/redirect advice.

## Observability and verification

New fixed INFO events are negotiation_limit and identity_limit, under existing opt-in event budgets. Separate five-second warning categories report cumulative and pre-verification limits, with US/PL operator translations. Wire descriptions remain English. Startup summaries include all six settings without secrets. security_closed counts an accepted connection once, even if multiple security/output policies coincide. API/WebAdmin counters and JSON schema remain unchanged; neither exposes source buckets or claims a ready-user count.

security.negotiation checks exact byte/frame boundaries, sticky failure, fragmentation and size_t overflow, pending/active ownership, hub/per-IP capacities, no allocation on denied keys, independent second/minute edges and reconnect/pruning behavior using a fake clock. security.negotiation.process tests actual TCP/OpenSSL pending admission (including uncompleted TLS), ready-slot release, cumulative bytes/frames across time, exact successful login, normal traffic after readiness, invalid INI, cross-transport identity quotas, different source IP isolation and pre-Tiger denial (240 instead of the rejected pair's 227).

adc.security.ncdc and adcs.security.ncdc use disposable real ncdc PTYs: one failed proof consumes credit, ncdc uses the remaining credit to reach NORMAL, its normal command remains available, and reconnect receives the expected rate-limit reason without a new login_ready. Previous real-client protocol, forced blocked-write/terminal timeout, MariaDB, Apache and WebAdmin checks remain in the strict gate. Historical transport fixtures now keep pre-login traffic within the cumulative budget; large TLS throughput fixtures explicitly log in before sending their normal keepalive stream. The public-field privacy assertion checks field boundaries rather than coincidental XX characters inside valid base32 CIDs.

The full strict configuration registers 44 CTest suites. These deterministic/local tests are not a measured load benchmark, DDoS guarantee, comprehensive allocator/TLS fault campaign, side-channel audit or verification of the remaining OS matrix. Account security, roles/bans, chat/search/transfer policy, TLS close_notify/reload and production administrative authentication remain separate work.

References: [ADC 1.0.4](https://adc.sourceforge.io/ADC.html), especially session setup, identity and state rules; [STA/QUI](ADC_STATUS.md) and [guest login](ADC_LOGIN.md) define the existing send-completion and shutdown contracts.
