# Roadmap

| Stage | Implemented scope |
| --- | --- |
| alpha-0.1.1 Core | Lifecycle, start/stop, synchronous system signals, logging |
| alpha-0.1.2 Network | Nonblocking TCP, IPv4/IPv6, multiple listeners and clients, resource limits |
| alpha-0.1.3 ADC | Bounded stream syntax parser, commands and data validation |
| alpha-0.1.4 ADCS | TLS credentials, nonblocking encrypted inbound transport, tests, US/PL documentation and operator messages |
| alpha-0.1.5 Configuration | INI, CLI overrides, path/limit validation and append-file logging |
| alpha-0.1.6 Logging | Startup summaries, WARN, typed connection events, bounded UTF-8 and rate budgets |
| alpha-0.1.7 Security | Bounded IP admission, frame quotas and absolute deadlines |
| alpha-0.1.8 Database | Optional local MariaDB connection, prepared queries and explicit migration metadata |
| alpha-0.1.9 API | Local authenticated HTTP/JSON status and health; read-only |
| alpha-0.1.10 WebAdmin | React/TypeScript/Vite read-only layout, EN/PL, local development adapter, static Apache example |
| alpha-0.1.11 Apache2 | Static HTTPS example, canonical redirect, TLS/security policy and disposable Apache tests |

| alpha-0.2.1 Project | Audit of alpha-0.1, bounded consistency fixes, strict CI integration gate and alpha-0.2 baseline |

## Proposed alpha-0.2 increments

These are planned acceptance gates, not implemented features. Define the final scope before starting each numbered stage.

| Proposed stage | Scope | Acceptance gate |
| --- | --- | --- |
| alpha-0.2.2 ADC output | Serializer, escaping, header validation and encoded byte limits | Byte-exact grammar fixtures; parse/serialize cases, Unicode/boundary/error coverage; no writes yet |
| alpha-0.2.3 Network output | Bounded per-client and aggregate queues, TCP/TLS partial writes, WANT_READ/WANT_WRITE and slow-reader policy | TCP/ADCS fragmented writes, queue limits, fairness under a stalled reader, peer failure and bounded shutdown; no unbounded allocation |
| alpha-0.2.4 Sessions | SUP/hash negotiation, SID allocator, handshake state machine and INF/identity checks | Legal/illegal state transitions, SID uniqueness/release, spoofed-source and duplicate identity rejection; no PID leakage |
| alpha-0.2.5 Client integration | Complete guest login over ADC and ADCS, lifecycle/disconnect behavior and readiness semantics | Real ncdc reaches normal logged-in state on both transports; reconnect, concurrent users, malformed negotiation and existing regressions pass |

Full chat, search and transfer coordination remain separate future stages after session correctness. Registered users require a reviewed authentication/permissions/password design and application SQL migrations. Production web administration requires an authenticated Apache gateway, browser sessions/roles, CSRF policy and audit before exposure. Logging isolation/rotation, TLS close_notify/reload, measured load/fuzz/fault-injection and the remaining OS matrix remain explicit follow-up work.

## Branches and completion

alpha-0.2.1 originates from main after the alpha-0.1.11 merge. alpha-0.2 is prepared at the same audit commit as a line marker; it is not a substitute for the requested main-based numbered branches. Merge a reviewed numbered stage to main only on a separate instruction. Keep historical branches and immutable SQL migrations.

Every stage must pass strict integration CTest, frontend checks when affected, metadata/documentation checks and an updated changelog. Green initial-HSUP ncdc tests are transport evidence only until the complete-login gate is implemented. See [the audit and prioritized findings](AUDIT_ALPHA_0.1.md).
