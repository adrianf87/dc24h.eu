# Roadmap

**Current increment — alpha-0.3.12:** [Limits](LIMITS.md) adds configurable session/user ceilings shared by ADC/ADCS, bounded by the connection cap. SessionManager checks capacity transactionally; LoginHub rejects a full user registry before welcome/list preparation with ISTA 211 and own IQUI. Closing reservations count until cleanup. Earlier descriptions retain their historical scope.

**Increment delivered in alpha-0.3.5:** [User Core](USER_CORE.md) adds checked UTF-8/NFC Nickname values, shared ADC/User text validation, immutable rename candidates and explicit projection from validated ClientInfo/VerifiedIdentity. Users now links the neutral dc24h_text library using the existing utf8proc package. Manager ownership/uniqueness remains unchanged. Earlier descriptions retain their historical scope.

**Increment delivered in alpha-0.3.4:** [Session Manager](SESSION_MANAGER.md) owns SID/CID/nickname indexes together with live models. Const lookups and managed lifecycle/User mutations preserve uniqueness through admission, rename and cleanup. LoginHub retains protocol and output ownership. Earlier descriptions retain their historical delivery scope.

**Increment delivered in alpha-0.3.3:** [User Core](USER_CORE.md) adds an owning public guest identity/profile to Session, with validated full-INF projection and lifecycle-guarded updates. It was developed directly from main with Session Core; main now combines it with the previously merged alpha-0.3.2 Session Manager. Earlier descriptions retain their historical scope.

**Increment delivered in alpha-0.3.2:** [Session Manager](SESSION_MANAGER.md) centrally owns all registered lifecycle models, with bounded capacity, stable lookup, phase counts and ordered snapshot pages. Protocol peers borrow models; their queues and public routing indexes remain in ADC. Earlier descriptions retain their historical scope.

**Increment delivered in alpha-0.3.1:** [Session Core](SESSION_CORE.md) provides the independent Users model and stable lifetime IDs, with live readiness/closure/timeline integration. Earlier stage descriptions below retain their original delivery scope.

**Increment delivered in alpha-0.2.10:** [Negotiation security](NEGOTIATION_SECURITY.md) adds pending admission caps, cumulative pre-readiness input budgets and shared pre-INF/Tiger attempt limits. Earlier release descriptions below retain their original scope.

**Increment delivered in alpha-0.2.9:** [STA/QUI](ADC_STATUS.md) adds semantic status/quit validation, source-checked B/D/E/F status routing, precise errors and bounded terminal ISTA/own-IQUI output. Historical descriptions below retain the behavior of their original releases.


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
| alpha-0.2.2 ADC Parser | BASE argument syntax, named fields, scalar helpers and TCP/TLS recovery |
| alpha-0.2.3 ADC State | Independent hub state machine, explicit approvals and command/direction/source gates |
| alpha-0.2.4 SUP | Feature AD/RM, BASE/common TIGR, bounded support and state composition |
| alpha-0.2.5 SID | 20-bit codec, bounded shared pool, RAII leases and managed SID sessions |
| alpha-0.2.6 INF | Bounded metadata, Tiger identity verification, peer-IP checks and public PD-free snapshots |
| alpha-0.2.7 CID/PID | Dedicated Tiger verifier, public identity result, cleansed buffers and provider-failure coverage |
| alpha-0.2.8 Login | Live guest NORMAL, ordered bounded output, nickname/CID registry, public presence and ncdc reconnect |
| alpha-0.2.9 STA/QUI | Semantic errors, status routing, final own QUI and bounded terminal drain |
| alpha-0.2.10 Security | Pending admission, cumulative negotiation budgets and pre-verification quotas |
| alpha-0.3.1 Session Core | Users model, stable lifetime identity, snapshots and actual connection lifecycle |
| alpha-0.3.2 Session Manager | Central ownership, bounded registry, stable lookup, phase counts and snapshot pages |
| alpha-0.3.3 User Core | Public guest identity/profile, lifecycle ownership and validated INF projection/updates (integrated after Session Manager) |
| alpha-0.3.4 Session Manager | Central public identity indexes and managed lifecycle/profile mutations |
| alpha-0.3.5 User Core | Checked public names/profile text, immutable rename values and verified ADC projection |
| alpha-0.3.6 Nick Manager | Bounded nickname ownership, validation, transactional rename and lifecycle cleanup |
| alpha-0.3.7 User Registry | Bounded SID/CID ownership, read-only User lookup and transactional admission/cleanup |
| alpha-0.3.8 User Login | Verified guest registration, typed failures and identity-bound final-welcome readiness |
| alpha-0.3.9 User List | Deterministic initial synchronization, bounded owned welcome and FIFO presence updates |
| alpha-0.3.10 INF Updates | Transactional metadata/profile/nickname updates and sanitized incremental fanout |
| alpha-0.3.11 User Disconnect | Finalization, resource cleanup and bounded public QUI after release |
| alpha-0.3.12 Limits | Configurable connection/session/user ceilings and lifecycle-safe admission/reuse |

## Historical acceptance plan before alpha-0.2.8

alpha-0.2.2 delivers the requested fuller parser. The earlier serializer proposal remains planned; subsequent stage numbers will be assigned when their scope is requested.

These are planned acceptance gates, not implemented features. Define the final scope before starting each numbered stage.

| Proposed stage | Scope | Acceptance gate |
| --- | --- | --- |
| Delivered in alpha-0.2.8: ADC output | Serializer, escaping, header validation and encoded byte limits | Byte-exact grammar fixtures; parse/serialize cases, Unicode/boundary/error coverage; no writes yet |
| Delivered in alpha-0.2.8: Network output | Bounded per-client and aggregate queues, TCP/TLS partial writes, WANT_READ/WANT_WRITE and slow-reader policy | TCP/ADCS fragmented writes, queue limits, fairness under a stalled reader, peer failure and bounded shutdown; no unbounded allocation |
| Delivered in alpha-0.2.8: guest sessions | Wire integration of state/SUP/SID/INF modules, user registry, nickname/CID uniqueness and admission | Legal/illegal state transitions, SID uniqueness/release, spoofed-source and duplicate identity rejection; no PID leakage |
| Delivered in alpha-0.2.8: guest client integration | Complete guest login over ADC and ADCS, lifecycle/disconnect behavior and readiness semantics | Real ncdc reaches normal logged-in state on both transports; reconnect, concurrent users, malformed negotiation and existing regressions pass |

Full chat, search and transfer coordination remain separate future stages after session correctness. Registered users require a reviewed authentication/permissions/password design and application SQL migrations. Production web administration requires an authenticated Apache gateway, browser sessions/roles, CSRF policy and audit before exposure. Logging isolation/rotation, TLS close_notify/reload, measured load/fuzz/fault-injection and the remaining OS matrix remain explicit follow-up work.

## Branches and completion

alpha-0.2.2 starts from main at `ca464e02e714c1e81ed4c37279496168a6c47e23`, after both audit branches were merged. It does not change main or move the alpha-0.2 marker automatically.

alpha-0.2.1 originates from main after the alpha-0.1.11 merge. alpha-0.2 is prepared at the same audit commit as a line marker; it is not a substitute for the requested main-based numbered branches. Merge a reviewed numbered stage to main only on a separate instruction. Keep historical branches and immutable SQL migrations.

Every stage must pass strict integration CTest, frontend checks when affected, metadata/documentation checks and an updated changelog. Green initial-HSUP ncdc tests are transport evidence only until the complete-login gate is implemented. See [the audit and prioritized findings](AUDIT_ALPHA_0.1.md).

alpha-0.2.3 starts from main at `4c98e10` after the alpha-0.2.2 merge. Its [state module](ADC_STATE.md) completes the standalone sequencing gate; outbound transport, feature/identity/password validation, SID uniqueness and complete ncdc login remain planned. This stage does not merge itself into main.

alpha-0.2.4 starts from main at `1dd0b9f1a94626789d1ac9addad991a6eae78910`, after the explicit alpha-0.2.3 merge. The [SUP module](ADC_SUP.md) implements feature/hash selection and composes it with state approval. Generic output, live transport attachment, hash computation and complete login remain planned. This branch does not merge itself into main.

alpha-0.2.5 starts from main at `350a43f69b5134bd049923739cd606f798f846ec` after the SUP merge. [SID](ADC_SID.md) completes allocator/lease ownership and composition; live output/session attachment and user identity remain planned. This branch does not merge itself into main.

alpha-0.2.6 starts from main at `85524067756ac5fcbd177bb47f5d2cfe33fad063`. [INF](ADC_INF.md) completes intrinsic field/PID-CID validation and stack composition. Global registry/admission, registered-user passwords, live output and complete login remain planned. This branch does not merge automatically.

alpha-0.2.7 starts from main at `9d3443a4d889e745b6490a139435a8f44297cfba`. [CID/PID](ADC_IDENTITY.md) extracts and hardens the basic check introduced in INF; it does not add live login, global CID uniqueness, account authentication or file-tree hashing. This branch is not merged automatically.


## alpha-0.2.8 Login

Delivered: ordered output/serializer, guest sessions and registry, live TCP/TLS guest NORMAL and ncdc reconnect. Remaining: registered-user credentials/permissions, chat/search/transfer handlers, TLS close_notify/reload, production load/fuzz/fault-injection and remaining OS validation. Base: main b8978491fa8e146527eba818f9bc1ab0a31aa90d. The numbered branch is not merged automatically.
