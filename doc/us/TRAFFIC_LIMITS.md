# Traffic limits — alpha-0.9.4

Spam and excessive presence traffic. This increment caps ready-session INF updates before they are applied and fanned out. The branch starts from main `2daeb20`. The branch name is `alpha-0.9.4`.

Chat messages already have a token bucket. Search queries and results have one. Outgoing and incoming connection requests have one. A generic frame quota still applies. This increment does not replace those budgets and does not mix presence updates into the chat bucket.

## Ready-session INF

Each connection has one fixed `PresenceGuard`. It is not an INI key. The bucket starts full: 8 updates, then 4 updates per second. Login INF is still the identify state and is not charged. A nickname collision is rejected before this charge.

Every other INF that reaches the normal state is charged, including an unknown-only update and an update that later fails a field check. Denial does not apply the update, does not relay it, and does not disconnect. The sender receives at most one recoverable `ISTA 100` per second, `Presence update rate limit exceeded`, with the offending fourcc. A repeated denial in that same second is silent. The guard keeps no field text.

STA stays outside this bucket, so a peer can still report a connection result inside the existing frame quota. Keepalives stay on that frame quota. An ordering test that needs many markers uses STA rather than INF.

## Unchanged

Schema v9 is unchanged. No status counter, WebAdmin page, language string, INI key or Apache setting is added. Check status stays on GitHub-hosted Ubuntu 24.04. This is not a bandwidth cap, not DDoS protection, and it does not close residual threat T13. A client that passes admission can still spend CPU on TLS and password hashing. HTTP/2, OCSP, automatic renewal and a public authenticated gateway remain unimplemented.

`adc.presence` checks the burst, fractional refill, one notice per second, a backwards clock, and that one chat message still flows after presence denial. `adc.presence.process` checks the same cap on ADC and ADCS, including a silent second denial and an independent chat message.
