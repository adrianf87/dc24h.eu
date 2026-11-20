# Beta baseline — beta-0.1

The release channel is Beta. The branch starts from main `423a958`. The branch name is `beta-0.1`.

The hub, the API and WebAdmin report `beta-0.1`. The CMake project number and the WebAdmin package number are `0.1.0`. Those numbers are the package tuple. Operators see `beta-0.1`. The status parser accepts that beta form, `beta-N.N.N`, and the existing `alpha-N.N.N` form. It still rejects any other version text.

Hub behavior, ADC bytes, recipient sets and security decisions stay the ones delivered through alpha-0.9.17. Schema v9 is unchanged. This is a developmental Beta. This is not a production release.

`beta.baseline` checks that version string, the twelve maintenance routes, and that `configuration.manage` and `hub.manage` stay unimplemented. The catalog test still requires schema v9 and `audit_events`. The hub executable does not gain a library for this check.

## Limits

This does not add a route, INI key, CLI flag, status field, WebAdmin page, Apache setting or schema migration. Browser kick and mute stay undispatched. Status and health stay bearer-only. `Require local` stays. No residual threat is closed. A public authenticated gateway, remote administrator access, `configuration.manage` and `hub.manage` stay separate work.

Run Check status on GitHub-hosted Ubuntu 24.04. Add `beta-0.1` to the push branches. Local validation uses eight CPU threads. Remote CI completion is not awaited.
