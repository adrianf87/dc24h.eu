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
