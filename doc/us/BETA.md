# Beta 0.3 Stage III — beta-0.3.8 through beta-0.3.10

The existing branch `beta-0.3.1-12-full` now reports **`beta-0.3.10`**. Stages I–III are delivered together without another branch, deployment or merge to `main`.

## beta-0.3.8 — local HTTPS trust boundary

Harden the **existing** `127.0.0.1:8443` Apache TLS vhost: retain TLS 1.2/1.3, restrict TLS 1.2 to forward-secret AEAD cipher suites, disable TLS compression and resumption tickets, and add cross-origin isolation/privacy response headers. Keep exact Host/Origin/route/method/body checks and fixed five local read-only gateway routes. No public listener, reverse proxy or new credential entry point.

## beta-0.3.9 — dedicated system service isolation

Expand the opt-in systemd example: private devices, kernel, cgroup, process and namespace restrictions, SUID/SGID and realtime limits, native syscall architecture and locked personality. Preserve owner-only MFA state, read-only system paths, the local C++ API Unix socket and IPAddressDeny/Allow loopback policy. Do **not** set `PrivateNetwork` (it would isolate the Apache/Node loopback), or `MemoryDenyWriteExecute` (unsafe for the Node/V8 JIT) without specific compatibility proof. The example is never auto-installed.

## beta-0.3.10 — optional private read-only pilot

Document an **operator-initiated** SSH **local** port-forward that carries `localhost:8443` from the workstation to the server's localhost-only Apache TLS vhost. SSH host-key authentication is independent of browser certificate verification; the client must validate both. Add a Python preflight using verified TLS and five fixed unauthenticated GETs; no password, TOTP, cookie, API token, remote host parameter, redirect following or write route. Tests enforce these limits. The pilot changes no listener or public firewall policy, grants no remote writes and is not an Internet-facing/WebAuthn-ready deployment. See [optional pilot](REMOTE_PILOT.md) and [gateway](REMOTE_GATEWAY.md).

Stages beta-0.3.11–beta-0.3.12 remain planned. CMake/npm package tuple `0.1.0`, schema v11, ADC/ADCS and existing public Apache vhost remain unchanged.

## Previous stage — beta-0.3.4 through beta-0.3.7

# Beta 0.3 Stage II — beta-0.3.4 through beta-0.3.7

On completing Stage II, the existing branch `beta-0.3.1-12-full` reported **`beta-0.3.7`** (Stage I plus Stage II). No separate patch branches, tags or deployment were created; Stage III above supersedes the runtime identifier.

## beta-0.3.4 — MFA threat model and contract

The separate remote read-only gateway requires the active administrator/Owner password proof, current live management permission and a server-verified six-digit TOTP code. The login accepts exactly `actor_id`, `password` and `otp`; successful session JSON includes `mfa: true`. No new route or browser-visible seed, enrollment API, privilege or public endpoint is introduced. TOTP is a development second factor, not phishing-resistant WebAuthn or an Internet-access authorization.

## beta-0.3.5 — private TOTP verification

An explicitly provisioned JSON enrollment file maps numeric account IDs to unique random Base32 TOTP secrets (at least 160 bits); it must be owned by the gateway UID and mode 0600 inside an owner-only 0700 directory. Verification uses RFC 6238 HMAC-SHA1, 30-second steps, six digits and a ±1-step clock window. A private append-only JSONL ledger records consumed steps with `fsync`, so a code cannot be reused for that account, including after gateway restart. Invalid/missing credentials fail closed; filesystem and ledger failures deny login. No seed or code is logged.

## beta-0.3.6 — session and browser isolation

The browser discards entered password and OTP on submit, requires `mfa: true` on session JSON, and never persists either credential. Only a successful password, role and TOTP verification creates an opaque read-only session. Secure, HttpOnly, SameSite=Strict cookies, CSRF, loopback HTTPS, four-session cap, two-minute idle and ten-minute absolute deadlines, live permission rechecks and revocation races remain enforced. Unit and browser regressions cover MFA failures and session behavior.

## beta-0.3.7 — structured event audit and safety boundary

The dedicated Node process writes bounded JSON event records (`component`, fixed `event`, ISO timestamp) to systemd-journald: login accepted/refused, MFA accepted/refused, logout, session revocation, refused route and unavailable API. No account ID, IP, password, OTP, API token, cookie, grant or response body is included. Journal persistence, access restrictions, rotation, alerting and independent forwarding remain the operator's responsibility; the sink is not an immutable compliance audit. The service template provisions private MFA storage and remains loopback-only. Tests cover file permissions, replay after restart and generic denials. See [Remote gateway](REMOTE_GATEWAY.md).

Nothing is merged to `main` or exposed to the Internet by this stage. WebAuthn/passkeys, recovery and enrollment administration, phishing-resistant MFA, anti-DoS across gateway restarts and public-edge hardening remain future work. MariaDB schema v11, CMake/npm package tuple `0.1.0`, ADC/ADCS protocol and existing public Apache vhost remain unchanged.

## Previous stage — beta-0.3.1 through beta-0.3.3

# Beta 0.3 Stage I — beta-0.3.1 through beta-0.3.3

Branch `beta-0.3.1-12-full` started from merged `main` after beta-0.2.12. Stage I originally delivered three patches and the runtime version **`beta-0.3.3`**; Stage II above supersedes its runtime identifier. Later beta-0.3.8–beta-0.3.12 work is not included.

## beta-0.3.1 — CI and release baseline

Update the C++ hub/API/service-controller, WebAdmin and live test fixtures. Remove the hardcoded `nproc == 4` assertion, retain Ubuntu 22.04/24.04/26.04 and Debug/Release/WebAdmin coverage, preserve the existing required check name, and trigger CI on the new branch. The CMake/npm package tuple stays `0.1.0` and database schema stays v11.

## beta-0.3.2 — local adapter API safety

Local and remote Node adapters use the same exact JSON content-type policy. Private C++ API response buffers are wiped after success, denial, malformed data, oversize and disconnect. Browser clients receive allowlisted data or generic errors. Socket-based tests cover invalid media-type prefixes and valid UTF-8 JSON. There are no new API routes or privileges.

## beta-0.3.3 — automated security checks

Required GitHub CI adds AddressSanitizer and UndefinedBehaviorSanitizer builds for a subset of C++ security tests, plus a high-severity production npm dependency advisory check. Existing lockfile and shared-library checks remain. No MFA, public access, remote writes, schema migration, ADC/ADCS protocol change or deployment is introduced. CI completion and merge remain separate decisions.

## Previous stage — beta-0.2.7 through beta-0.2.12

# Combined hardening release — beta-0.2.7 to beta-0.2.12

One branch, `beta-0.2.7-beta-0.2.12-finish`, starts at merged `main` `f91ab49` after beta-0.2.6. The **final version string is `beta-0.2.12`**; intermediate patch names describe workstreams, not separately created branches, tags or builds. The visible project name is **dc24h.eu High Starter**; the stable C++/API identity `server.name = "dc24h.eu"` and existing wire contracts remain unchanged.

## beta-0.2.7 — release documentation and CI coherence

Update the previously stale `webadmin/README.md`, the main release headings and the CI docs. The project checker now includes the WebAdmin README in current-version, UTF-8 and local-link checks. Fix both C++/WebAdmin repository guards to recognize the renamed `adrianf87/dc24h.eu-high-starter` slug, and test that guard. The existing Ubuntu 22.04/24.04/26.04 Debug/Release/WebAdmin matrix runs for the one combined branch; public host and package tuple `0.1.0` do not change.

## beta-0.2.8 — authentication and session regressions

Add a deterministic regression that an outstanding status read from the oldest browser session cannot return metrics after a fifth login evicts it. The four-session bound, two-minute idle and ten-minute absolute deadlines, single-use challenges, CSRF and grant revalidation stay as before. No new login scope, persistent identity store or MFA is introduced.

## beta-0.2.9 — private API response hardening

Enforce exact `application/json` media type (allowing parameters such as UTF-8), rather than any prefix. Add a four-second **absolute** request deadline in addition to the idle socket timeout: a trickling peer cannot keep a private read alive indefinitely. Scrub buffered private response bytes after normal completion and on truncated/aborted/oversized responses. Tests cover a forged JSON media-type prefix and recovery.

## beta-0.2.10 — Apache/HTTPS trust boundary

The local-only TLS vhost now blocks `Sec-Fetch-Site: cross-site` and `same-site` for the five gateway routes before proxying, while allowing `same-origin`, `none` and missing Fetch Metadata for compatible local clients. The Node gateway already independently rejects cross-site Fetch Metadata. Add disposable Apache integration negatives; retain exact Host/Origin/method/query/framing and static-file restrictions.

## beta-0.2.11 — concurrent failure and recovery

Add socket-free concurrent status reads with mid-flight permission revocation. All pending requests must return generic 403 without metrics, old browser sessions stay invalid, and a new authorized login can recover. Normal private API failures return generic 503 without cached metrics. This is deterministic regression coverage, not a production load-capacity claim.

## beta-0.2.12 — final verification boundary

Pin hub, API, service controller, WebAdmin and fixtures to `beta-0.2.12`; keep the package tuple `0.1.0`, MariaDB schema v11, ADC/ADCS behavior and five fixed local HTTPS read-only routes. Run release metadata, documentation, static Apache, Node and the existing C++ Debug/Release, WebAdmin, Chromium and real API integration checks on CI. Merge remains an explicit separate decision after verification; no production deployment or public access/MFA is claimed.

## Earlier stage

# Minimal status projection and API failures — beta-0.2.6

Branch `beta-0.2.6` begins from merged `main` `f2aec7d` following beta-0.2.5. The separate local-only read-only gateway now parses only approved public aggregate fields from the private C++ API; it constructs a fresh object for server version/running/uptime, connections, users-online/peak, chat totals and optional search/transfer counters. Unknown fields (identities, IP/CID, audit rows, tokens, paths) and unrelated status sections (configuration, dashboard and monitoring) are ignored and never serialized, even when their structure changes. Required counters must be finite nonnegative safe integers, the private API schema and version must be valid, and malformed optional **public** counters fail closed. Broken API responses or outages return generic 503 with no cached data. Deterministic tests cover extra secrets, partial data, unsafe numbers, changed upstream formats and recovery. The five fixed HTTPS loopback routes, administrator/Owner checks, bounded sessions, CSRF, CMake/npm package tuple `0.1.0`, MariaDB schema v11 and ADC/ADCS are unchanged. No public access without MFA, remote writes, migration, production deployment or automatic merge. See [Remote gateway](REMOTE_GATEWAY.md).

# Apache2, HTTPS and loopback boundary — beta-0.2.5

Branch `beta-0.2.5` starts from merged `main` `56a73f7` after beta-0.2.4. The separate development-only Apache TLS vhost remains bound to `127.0.0.1:8443` and now rejects an unexpected Host header, query strings on remote API routes, disallowed HTTP methods and mismatched Origin headers before reaching the Node gateway; login and logout require the exact `https://localhost:8443` Origin. It denies the regular `index.html` and `local-*` JS/CSS bundles, allowing only the remote portal. Apache bounds POST request bodies to 2048 bytes using explicit pre-proxy Content-Length and Transfer-Encoding checks in addition to `LimitRequestBody`, after the latter alone failed a real-Apache integration test. A disposable HTTPS Apache regression suite checks refusals and verifies that disallowed requests never reach the mock backend; companion static tests check configuration invariants. No existing public Apache configuration is changed or deployed. The hub/API, service controller and WebAdmin report `beta-0.2.5`; five read-only routes, Secure/HttpOnly/SameSite=Strict cookies, CMake/npm package tuple `0.1.0`, MariaDB v11 and ADC/ADCS remain unchanged. No MFA, public access, administrative writes, migration or production release. See [Remote gateway](REMOTE_GATEWAY.md).

# Session lifetime, rotation and CSRF — beta-0.2.4

Branch `beta-0.2.4` starts from merged `main` `b30d833` after beta-0.2.3 and its follow-up fixes. Two reproduced session-lifecycle regressions are fixed: re-authentication now invalidates the browser's previously presented session cookie, and a session's absolute/idle timestamps are established **after** asynchronous password proof and permission revalidation, rather than at request arrival. Deterministic no-socket tests cover 10-minute absolute expiry, 2-minute idle expiry, four concurrent sessions, independent browser cookies and CSRF tokens, logout and cookie replay, mismatched Origin, single-use challenges under concurrency, in-flight status reads and gateway restart. Playwright checks that the UI removes previously displayed metrics when the session expires. The hub, API, controller and WebAdmin report `beta-0.2.4`. The five read-only routes, localhost HTTPS boundary, Secure/HttpOnly/SameSite=Strict cookie scoped to `/remote/v1`, package tuple `0.1.0`, schema v11 and ADC/ADCS remain unchanged. No public access, MFA, migration, service control or deployment. This is not a production release. See [Remote gateway](REMOTE_GATEWAY.md).

Follow-up review adds guards for simultaneous re-logins and logout racing a pending login: a previously valid cookie must still map to the same server-side session when re-authentication completes. The UI rejects older HTTP responses after an authentication transition, hides data as soon as logout starts, and clears the password even if fetching a login challenge fails. Socket-free and browser regressions cover the races.

# Authentication and live authorization — beta-0.2.3

Branch `beta-0.2.3` starts from merged `main` `070dd44` (beta-0.2.2). The gateway now rejects a password-proof response that identifies a different account from the one requested, even if the response has an otherwise valid administrator/Owner role. Concurrent private status responses cannot complete after another request has revoked that browser session. Deterministic socket-free tests cover approved logins, inactive or unauthorized accounts, bad or unknown credentials, revoked roles/grants, API/DB faults, status-read races and per-actor login throttling. The hub, API, controller and WebAdmin report `beta-0.2.3`. The five routes, CMake/npm package tuple `0.1.0`, MariaDB schema v11 and ADC/ADCS behavior remain unchanged. No public access, MFA, remote mutation, migration or deployment is included. This is not a production release. See [Remote gateway](REMOTE_GATEWAY.md).

# HTTP request allowlist hardening — beta-0.2.2

Branch `beta-0.2-code` starts from `main` at `fbb7b4e`, after the merge of beta-0.2.1. The gateway still exposes only the five fixed remote paths with exact methods. It now rejects *all* browser-supplied `X-Forwarded-*` variants and other untrusted proxy-identity/rewrite headers before allocating challenges or contacting the C++ API. Negative request tests cover path variants, methods, framing, invalid JSON, size limits and secret-free refusals; the Apache example strips common spoofing headers. The hub, API, service controller and WebAdmin report `beta-0.2.2`; package tuple `0.1.0` and schema v11 stay unchanged. No public access, remote mutation, MFA, ADC/ADCS changes or deployment is included. See [Remote gateway](REMOTE_GATEWAY.md). This is not a production release.

# Maintenance baseline — beta-0.2.1

Branch `beta-0.2.1` starts from protected `main` after merging beta-0.2 (`fbd5ab3`). The hub, local API, service controller and WebAdmin report `beta-0.2.1`; the CMake and npm package tuple stays `0.1.0`. The beta-0.2 gateway remains authenticated, local-only and strictly read-only. No MFA or Internet-facing mode, remote writes, systemd controls, migration, ADC/ADCS protocol change or production deployment is introduced. This is a development baseline, not a production release. See [Remote gateway](REMOTE_GATEWAY.md).

## Previous stage

# Remote WebAdmin read-only gateway — beta-0.2

Branch beta-0.2 is based on the latest protected main with the beta-0.1.8 baseline. The hub, API, service controller and WebAdmin report beta-0.2. A separate HTTPS portal at /remote.html uses active administrator/Owner password proof, short server-side sessions, CSRF and a fixed status projection. The gateway is local-only because MFA is not implemented; it offers no public network listener, configuration mutation, role write or systemd control. Schema v11 and package tuple 0.1.0 are unchanged. This is not a production release. See [Remote gateway](REMOTE_GATEWAY.md).

## Previous stage

# Development baseline — beta-0.1.8

Branch `beta-0.1.8` starts from protected main `cebcd12`. The hub, maintenance API, independent service controller and WebAdmin identify as `beta-0.1.8`, while the package tuple stays `0.1.0`. This baseline keeps the local Owner-only WebAdmin service controls, confirmation and audit behavior from beta-0.1.7, along with fixes merged into main. No new feature, schema migration, public proxy route, ADC/ADCS protocol change or production deployment is included. This is not a production release.

## Previous stage

# Hub service controls in WebAdmin — beta-0.1.7

Branch `beta-0.1.7` starts from protected main `a314fe0`. The local Owner-only WebAdmin panel reads independent systemd status, confirms Start/Stop/Restart, shows recent separately observed online-user counts and links to proof-gated service audit. No new schema, public proxy route, arbitrary command or production deployment. See [Hub WebAdmin](HUB_WEBADMIN.md). Hub, API and WebAdmin report `beta-0.1.7`; package tuple remains `0.1.0`. This is not a production release.

## Previous stage

# Owner-only service operations — beta-0.1.6

Branch `beta-0.1.6` starts from protected main `463e56e`. The separate maintenance API checks the current Owner role, grant, session, CSRF and explicit confirmation before calling the fixed `dc24h.service` controller. Only Start, Stop and Restart are permitted. Schema v11 records a durable `started` intention before action and a final verified `succeeded` or `failed` state; `outcome_unknown` is never automatically retried. No browser UI or public proxy is added. Read [Service operations](SERVICE_OPERATIONS.md). This is not a production release.

# Read-only service controller — beta-0.1.5

Branch `beta-0.1.5` starts from main `e8d00ae`. Hub, API and WebAdmin report `beta-0.1.5`. The independent C++20 `dc24h-service-controller` queries the fixed `dc24h.service` unit via a local authenticated Unix socket, even when the hub is stopped. Only status reads are implemented. No HTTP service route, web controls, Start/Stop/Restart or schema change. See [Service controller](SERVICE_CONTROLLER.md). Schema v10 is unchanged. The application `hub.manage` permission remains non-executable.

## Previous stage

# WebAdmin configuration editor — beta-0.1.4

Branch `beta-0.1.4` starts from main `3f7f03d`. Hub, API and WebAdmin report `beta-0.1.4`. A restricted editor on the Configuration page loads the saved INI from the separate maintenance process, displays its four allowlisted nonsecret settings and the maintenance startup values, validates and previews differences, then requires explicit confirmation for atomic write or backup restore. Apache and the Node adapter forward only exact loopback, same-origin paths under grant, session, CSRF and server-side role checks. A controlled hub restart is still required; no live reload or service controller is included. Schema v10 is unchanged. See [Configuration editor](CONFIGURATION_EDITOR.md).

## Previous stage

# Atomic configuration write — beta-0.1.3

Branch `beta-0.1.3` starts from main `83b224c`. Hub, API and WebAdmin report `beta-0.1.3`. The isolated maintenance API supports atomic INI write/restore using the fixed, reviewed allowlist and a content revision. Migration v10 records configuration audit intentions and results; v9 history is preserved. See [Configuration write](CONFIGURATION_WRITE.md). No WebAdmin editor, live hub reload or service controller is included.

## Previous stage

# Configuration review — beta-0.1.2

The branch name is `beta-0.1.2`. Hub, API and WebAdmin report `beta-0.1.2`. The maintenance API can read four non-secret settings, distinguish saved from maintenance-process active values and validate proposed values against a content revision without writing. The running hub's active configuration is not available from maintenance. See [Configuration review](CONFIGURATION_REVIEW.md). Full configuration management and hub management remain unimplemented. Schema v9 is unchanged.

## Previous stage

# Management foundation — beta-0.1.1

The release channel is Beta. beta-0.1.1 starts from main `075081f`. The branch name is `beta-0.1.1`.

The hub, the API and WebAdmin report `beta-0.1.1`. The CMake project number and the WebAdmin package number stay `0.1.0`. Those numbers are the package tuple. Operators see `beta-0.1.1`. The status parser accepts `beta-N.N`, `beta-N.N.N`, and the existing `alpha-N.N.N` form. It still rejects any other version text.

Hub behavior, ADC bytes, recipient sets and the security decisions delivered through alpha-0.9.17 stay. Schema v9 is unchanged. This is a developmental Beta. This is not a production release. No residual threat is closed.

`configuration.manage` and `hub.manage` stay unimplemented. The catalog and the authorize contract are specified in [Management foundation](MANAGEMENT_FOUNDATION.md). This increment does not read or write configuration and does not start, stop or restart the hub.

`beta.baseline` checks that version string, the twelve maintenance routes, and that `configuration.manage` and `hub.manage` stay unimplemented. The catalog test still requires schema v9 and `audit_events`. The hub executable does not gain a library for this check.

## Earlier baseline — beta-0.1

The branch starts from main `423a958`. The branch name is `beta-0.1`. The hub, the API and WebAdmin reported `beta-0.1`. That increment did not add a route.

## Limits

Browser kick and mute stay undispatched. Status and health stay bearer-only. `Require local` stays. A public authenticated gateway and remote administrator access stay separate work.

Run Check status on public GitHub-hosted runners `ubuntu-22.04`, `ubuntu-24.04` and `ubuntu-26.04`. Each runner is specified with four logical CPUs and 16 GB of memory. Each job checks that `nproc` equals 4 and uses `nproc` workers. Pull requests to `main` run the same checks. Local validation uses eight CPU threads. Remote CI completion is not awaited. A merge to `main` waits until the required checks succeed.
