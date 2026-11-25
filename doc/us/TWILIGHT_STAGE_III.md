# dc24h.eu-twilight — Stage III operator branding (review candidate)

This stage changes *display text*, not runtime identity, network trust or installed service names.

## Changed in the proposed Stage III patch

- The `dc24h --version` output now displays `dc24h.eu-twilight beta-0.9.20`; the executable remains `dc24h`.
- Startup log prefixes, PL/EN CLI help text, the default ADC welcome, and the example INI welcome display the Twilight brand. A previously saved operator-supplied `welcome_message` is not rewritten.
- Descriptions in example systemd units and explanatory Apache/INI comments use the Twilight name. Unit filenames, executable paths, `ExecStart`, package names and actual services remain unchanged.
- Process, log, ADC welcome and isolated package/rollback test expectations are updated. Historical rollback binaries retain their *original* version identity; the evidence field still records only `beta-0.9.20` for the current candidate.

## Explicitly unchanged until a separate OWNER decision

| Identifier | Current value / behavior | Reason |
| --- | --- | --- |
| C++ `server.name` in the v1 status API | `dc24h.eu` | Strict protocol/data contract, not a UI label |
| WebAdmin status parsers and fixtures | Require `dc24h.eu` | Continue to reject foreign and malformed status responses |
| Public/Apache `ServerName`, redirect, Host and Origin checks | Existing deployment examples | DNS, TLS and origin trust are infrastructure decisions |
| WebAuthn `rpID`, configured Origin, credentials | Unchanged | A display-name change must not break credential verification |
| Binary, npm package, systemd unit, config and state path identifiers | Existing `dc24h*` names | Name migration and rollback belong to Stage IV |
| Source history, old RC1 tag and links | Unchanged | Preserve provenance |

The OWNER must separately choose whether `server.name` will remain the stable v1 technical identifier or migrate with an explicit compatibility and negative-test policy. Do not silently accept both names, rename the DNS host, alter passkey origins or migrate existing state.

## Acceptance boundaries

Review the exact final branch SHA in GitHub-hosted Ubuntu 22.04/24.04/26.04 CI: C++ Debug/Release, real process/CLI, ADC/ADCS, WebAdmin/browser, sanitizer/security, VM/systemd and isolated package recovery. CI success does not authorize production activation, package rename, private-state migration or a release. The protected `main` merge requires an explicit OWNER decision.
