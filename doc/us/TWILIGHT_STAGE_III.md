# dc24h.eu-twilight — Stage III operator branding and status identity

The initial Stage III merge changed *display text* only. After a separate OWNER authorization, a follow-up PR also changes the exact v1 `server.name` identifier and its WebAdmin consumers. Network trust and installed service names remain unchanged.

## Operator branding merged in Stage III

- The `dc24h --version` output now displays `dc24h.eu-twilight beta-0.9.20`; the executable remains `dc24h`.
- Startup log prefixes, PL/EN CLI help text, the default ADC welcome, and the example INI welcome display the Twilight brand. A previously saved operator-supplied `welcome_message` is not rewritten.
- Descriptions in example systemd units and explanatory Apache/INI comments use the Twilight name. Unit filenames, executable paths, `ExecStart`, package names and actual services remain unchanged.
- Process, log, ADC welcome and isolated package/rollback test expectations are updated. Historical rollback binaries retain their *original* version identity; the evidence field still records only `beta-0.9.20` for the current candidate.

## OWNER-approved status identity change (follow-up PR)

- C++ now publishes `server.name = "dc24h.eu-twilight"` in the existing v1 status response; `api_version`, JSON shape, version and all counters remain unchanged.
- Both WebAdmin validators (`parseSnapshot` and `parseRemoteSummary`), their normalized outputs and their test fixtures now require **exactly** `dc24h.eu-twilight`.
- The legacy value `dc24h.eu`, foreign values, empty values and missing names are refused. The update does **not** add a permissive dual-name fallback or infer identity from HTTP Host.
- This is a deliberate *breaking compatibility cutover* for status consumers: a new server and new WebAdmin must be deployed together in any separately authorized rollout. Old WebAdmin cannot read the new server, and new WebAdmin rejects old status sources. Inventory/update external status clients before rollout; no silent migration is provided.
- Tests verify the C++ status name and strict acceptance/refusal in both WebAdmin parsers. This PR needs its own exact-head GitHub-hosted CI and OWNER merge acceptance.

## Explicitly unchanged until another separate OWNER decision

| Identifier | Current value / behavior | Reason |
| --- | --- | --- |
| Public/Apache `ServerName`, redirect, Host and Origin checks | Existing deployment examples | DNS, TLS and origin trust are infrastructure decisions |
| WebAuthn `rpID`, configured Origin, credentials | Unchanged | A display-name change must not break credential verification |
| Binary, npm package, systemd unit, config and state path identifiers | Existing `dc24h*` names | Name migration and rollback belong to Stage IV |
| Source history, old RC1 tag and links | Unchanged | Preserve provenance |

The new `server.name` value is **not** a DNS name, TLS certificate subject, WebAuthn RP ID or configuration path. Do not rename hosts, alter passkey origins or migrate private state as a side effect of this source-only change.

## Acceptance boundaries

Review the exact final branch SHA in GitHub-hosted Ubuntu 22.04/24.04/26.04 CI: C++ Debug/Release, real process/CLI, ADC/ADCS, WebAdmin/browser, sanitizer/security, VM/systemd and isolated package recovery. CI success does not authorize production activation, package rename, private-state migration or a release. The protected `main` merge requires an explicit OWNER decision.
