# WebAdmin — secure API integration plan

Status: **DRAFT / PARTIALLY IMPLEMENTED (read-only phase)**. Source baseline: protected `main` after Stage IV PR #29 (`bbf240d4f5303209c52614b62a877d4c915ad9aa`). This document is the integration and review checkpoint for `feature/webadmin-secure-api-integration`.

## First implementation checkpoint — authenticated read-only APIs

One precise public read path is added **on the feature branch only**: `GET /stage4/v1/identity-summary`. Node invokes the new C++ `GET /api/v1/stage4/identity-summary` via an exact-path UID-checked Unix bridge. The C++ Stage IV account service returns only aggregate account counts and verified email count from MariaDB; the Node adapter validates and projects the bounded DTO. The public gateway requires a verified Stage IV password + email OTP session, rechecks the authoritative role/revision after the asynchronous read, rate-limits requests and permits only active Owner or Administrator accounts. Registered users and unauthenticated requests are rejected. Headers, methods and routes remain on finite Apache/Node allowlists. React renders the counts inside the existing Stage IV login UI; no grant, service token or OTP is exposed to React.

**Source boundary:** The Stage IV C++ account service is not the live ADC/ADCS hub. Its `/api/v1/status` is synthetic account-process status and `/api/v1/users` is unavailable there. Therefore this change deliberately does **not** publish hub telemetry or a connected-user list. Those require a separately reviewed, read-only hub bridge with strong authentication and data minimization. The existing local private React `/admin-api` remains private.

This phase does **not** implement privileged account/ban/audit write operations, an authorization-delegation mechanism for administrative C++ grants, public configuration changes or service controls. No deployment/activation authorization is conveyed by code review or CI.

## Goal and non-goals

Integrate the existing C++ administrative API capabilities with the authenticated WebAdmin through explicitly authorized, typed, route-specific adapters. Cover all WebAdmin feature areas that have an implemented backend contract; clearly label unavailable operations. Do **not** bulk-publish every internal C++ endpoint. Do not expose `/remote.html`, the localhost `/admin-api`, the maintenance controller, database, Unix sockets, bearer tokens or generic forwarding to Internet clients.

The Stage IV password plus mandatory eight-digit email OTP design, server-side authoritative role/ACL checks, sessions, CSRF, rate limits and audit remain in force. Passkey/WebAuthn public login is outside the approved scope. Any new public operation, especially one that changes configuration or service state, requires threat-model review and explicit OWNER authorization before exposure.

## Initial API inventory (verify method, schema and implementation before coding)

| WebAdmin area | Existing internal contract or UI area | Proposed public integration boundary | State |
| --- | --- | --- | --- |
| Stage IV account overview | New `/api/v1/stage4/identity-summary` on account service | Authenticated, aggregate count-only DTO | Implemented in branch; integration CI pending |
| Status, dashboard and health | `/api/v1/health`, `/api/v1/ready`, `/api/v1/status`; Dashboard | Live hub needs separate scoped bridge; do not substitute account-process status | Not yet integrated |
| Connected users and sessions | `/api/v1/users`, HubUsers, accounts | Live hub needs separate scoped bridge and session/SID redaction | Not yet integrated |
| Accounts and password | `/api/v1/accounts`, `/api/v1/admin`, Stage IV account flows | Existing verified-email registration, own-password and session rules; privileged account operations require backend ACL | Inventory needed |
| Roles and permissions | `/api/v1/admin/access`, `/api/v1/management`, Roles | Per-operation reauthorization; no client-provided role grants | Inventory needed |
| Bans and moderation | `/api/v1/bans`, Bans, ModerationRules, Kick/Mute views | Explicit allowlist and audit; unavailable actions remain disabled | Inventory needed |
| Chat, search and transfers | Dashboard, ChatStatistics, SearchMonitoring, TransferMonitoring, TransferStatistics | Bounded read-only metrics where backend support exists | Not yet integrated |
| Audit and diagnostics | `/api/v1/audit`, AuditLog, metrics/status | Owner/admin-only sanitized, paginated reads; no raw logs or secrets | Inventory needed |
| Configuration | `/api/v1/management/configuration`, `/write`, `/restore`, ConfigurationEditor | Private by default; no public writes before separate OWNER decision and recovery review | OWNER gate |
| Hub/service management | `/api/v1/management/authorize`, `/api/v1/management/hub/*`, HubService | Private by default; never proxy arbitrary start/stop/restart actions | OWNER gate |

These are route **families**, not an assertion that each listed operation is fully implemented. Derive an exact `METHOD + PATH + request/response schema + required permission + side effect` matrix from `src/api/server.cpp`, `src/api/accounts.cpp`, the WebAdmin adapters and tests. Existing documented 501/not-implemented behavior must remain visible and must not become a false PASS.

## Implementation checklist

- [x] Implement a count-only C++ account API, a rate-limited and role-gated Node adapter, and a React identity overview with negative-path tests; **not all APIs**.
- [ ] Enumerate every actual C++ route and local WebAdmin adapter; reconcile documentation, runtime route table and UI pages. Classify supported/read-only/write/unimplemented.
- [ ] Define a finite public route and method allowlist, per-role permission map, redacted response DTOs, bounded payloads/timeouts and fail-closed errors.
- [ ] Enforce session, OTP-authenticated identity, CSRF on writes, current C++ role/status and revocation at the backend for **every** operation; never trust browser role flags.
- [ ] Implement explicitly named Node-to-C++ bridge handlers without a generic user-controlled path or headers, public bearer token, database connection or maintenance proxy.
- [ ] Connect WebAdmin pages to typed adapters with loading, denied, unavailable and stale-session states; keep en-US and Polish translations aligned.
- [ ] Test positive/negative roles (Owner, Administrator, Registered, etc.), forbidden routes/methods, header and Origin spoofing, session expiry, logout, revoke, CSRF, rate limits, replay and audit.
- [ ] Run GitHub-hosted Ubuntu 22.04, 24.04 and 26.04 CI on the exact PR head, with C++/Node/TypeScript and Playwright coverage; keep any real-VPS tests separately OWNER-approved and non-disruptive.
- [ ] Collect exact-SHA evidence and review outstanding security risks. Obtain separate OWNER decision for exposing each high-impact configuration/service endpoint and explicit OWNER MERGE GO.

## Acceptance boundary

This Draft PR implements one public read-only account-summary endpoint **only in reviewable source**, not on the VPS. Stage IV's previous scoped acceptance remains historical; it is not an approval to widen the deployed public API. Do not merge, deploy, change host firewall/DNS or issue RELEASE/PRODUCTION GO on the basis of this branch without fresh OWNER security acceptance.
