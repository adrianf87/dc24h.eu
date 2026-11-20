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
