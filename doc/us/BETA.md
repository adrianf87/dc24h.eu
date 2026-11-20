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
