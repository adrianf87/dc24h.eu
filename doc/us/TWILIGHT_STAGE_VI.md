# dc24h.eu-twilight — Stage VI: full qualification and OWNER release decision

**Status: QUALIFICATION IN PROGRESS — release NO-GO.** OWNER accepted Stage V on 2026-11-25, but merge of [PR #9](https://github.com/adrianf87/dc24h.eu-twilight/pull/9) was deferred until Stage VI is complete. The live-branding audit now includes WebAdmin footers and the ADC hub identity visible to clients. Nothing here authorizes tagging, deployment, private-state migration or a public listener. [Issue #1](https://github.com/adrianf87/dc24h.eu-twilight/issues/1) remains the governing checklist.

## Rebranding scope and intentional exceptions

| Area | Current contract and evidence | Boundary |
| --- | --- | --- |
| Visible product identity | `dc24h.eu-twilight` in C++ version/banner, ADC `IINF NI` hub name, PL/US messages, sample INI, PL/EN WebAdmin footers and active surfaces; `scripts/twilight_stage6_preflight.py`. | Static checks for 12 active files and negative regressions, **not** certification of every client or display. |
| API v1 status | C++ emits `server.name=dc24h.eu-twilight`; both WebAdmin validators reject the old name. | The v1 status identity is deliberately incompatible with some old clients. Coordinate clients rather than silently accepting the old value. |
| Passkeys and external origin | Display-only `rpName` changed; `rpID`, `expectedOrigin` and remote gateway Origin checks remain strict. | DNS, certificate names, actual Origin and WebAuthn `rpID` are **not** automatically renamed. Physical/synced passkeys are not certified. |
| Systemd and installation | New unit name, `Conflicts=dc24h.service`, explicit OWNER marker, preserved `dc24h` account. | UID/GID, `/etc/dc24h`, `/var/lib/dc24h`, private state and MariaDB schema are unchanged; no live migration. |
| GitHub | Five workflow repository guards contain the reviewed Twilight identity; historical source repositories remain intentional exceptions. | Old commit history, links to High Starter, and the old RC1 evidence may retain their original names. |
| Stable technical IDs | C++ `namespace dc24h`, `include/dc24h`, `DC24H_*`, ADC/ADCS, API paths and DB identity remain technical. | No protocol/data renaming without a separately approved migration design. |

## Initial preflight delivered in this PR

- `scripts/twilight_stage6_preflight.py` checks the exact `git rev-parse HEAD`, a 40-character source SHA and only the GitHub-hosted Ubuntu **22.04 / 24.04 / 26.04** job labels. It checks 12 active UI/CLI/ADC files and their security/installation contracts, including the exact WebAdmin footers and ADC `IINF NI` identity.
- `tests/twilight_stage6_preflight_tests.py` covers fail-closed negative mutations: API identity, Passkey RP ID/Origin, systemd marker/conflicts, foreign GitHub repository, npm name, bad SHA and unsupported OS.
- The C++ Debug/Release matrix in `.github/workflows/build.yml` generates a redacted `twilight-stage6-preflight.json` artifact on the exact checked-out source SHA. The ZIP validator explicitly permits and checks this optional new report without retroactively requiring it from historical RC1 artifacts.
- This report deliberately says **`NO-GO`** with false values for independently verified final-SHA CI/ZIP and release authorization. Its `stage_v_documentation_accepted: false` means **this source-only script cannot attest a separate external OWNER decision**; it does not retract the actual Stage V acceptance recorded in Issue #1. Passing preflight **never means release GO**.
- No production service, `systemctl`, `dpkg`, private data, TLS, database or public listener is touched by this source-only check.

## Independently verified CI baseline — before the follow-up changes

For SHA [`9ba51d60245a233e3ffc4e3b80e34f5c1f804fbb`](https://github.com/adrianf87/dc24h.eu-twilight/commit/9ba51d60245a233e3ffc4e3b80e34f5c1f804fbb) on PR #9:

- **5/5 successful workflows and 23/23 successful jobs**, no failed or skipped jobs. C++ Debug/Release and WebAdmin on Ubuntu 22.04/24.04/26.04 plus ASan/UBSan, CodeQL, TSAN/fuzz, storage/fault and VM/systemd. Workflow run IDs: `45714909507`, `45714909336`, `45714909508`, `45714909435`, `45714909496`.
- **19/19 artifact ZIPs** downloaded via the GitHub API and independently hashed. All **19/19 SHA-256 digests** matched GitHub metadata; `scripts/beta09_artifact_evidence.py` validated **178 JSON reports**, including **18 expressly permitted legacy reports without embedded SHA**. The review covered allowed filenames, required report sets, embedded source identity where present, bounded data and report semantics. Workflows without ZIP outputs are assessed through their jobs instead.
- This evidence is **specific to the prior SHA**. Fixes to the ADC identity, footers and regression checks change the PR head. **Final-head CI, jobs and ZIPs must be rechecked** before any merge decision. Never transfer green evidence to another commit.

## Full Stage VI acceptance gates — NOT YET SATISFIED

1. **Stage V was accepted by OWNER on 2026-11-25**; retain bilingual docs, GitHub description, changelog and link-audit evidence. Stage VI located and corrected three further old-brand occurrences in current UI/ADC. Historical, protocol and infrastructure exceptions remain explicitly justified.
2. Freeze the final head SHA of the PR and verify **every required GitHub-hosted workflow and job at that exact SHA**: C++ Debug/Release, WebAdmin/Playwright, CodeQL, Ubuntu 22.04/24.04/26.04 VM/systemd, ASan/UBSan, TSAN/fuzz, storage and dependency audit. Failed, canceled, skipped or foreign-SHA jobs cannot be counted as passing.
3. Independently download CI artifact ZIPs; hash their bytes against GitHub metadata (SHA-256), then validate required report sets, embedded source identity, job/run attempt and report semantics. Green badges alone are insufficient.
4. Extend negative contract testing and human review: incorrect `server.name`, repository/SHA, Origin/`rpID`, old client/config handling, denied migration without OWNER, TLS/session isolation. Do not weaken fail-closed behavior for cosmetic backward compatibility.
5. Review installation and rollback on a **representative host**. Stage IV's synthetic package/service rehearsal (PR #7) is not a production host migration test. Debian/Xeon capacity and hardware, physical client/passkey coverage, SLA, RPO/RTO, private-state cold-start, atomic v1→v2 migration and audit rollover policy remain unproven or require explicit scope exclusions and separate decisions.
6. Request separate OWNER decisions with repository, branch, frozen SHA, full CI/artifact inventory, exceptions, risks and RC scope: **`MERGE GO/NO-GO` separately from `RELEASE GO/NO-GO`**. Neither authorizes a production deployment, state migration or public listener activation.

## Decision record — pending qualification

| Field | Current value |
| --- | --- |
| Repository / branch | `adrianf87/dc24h.eu-twilight` / `rebrand/stage-vi-release-qualification` |
| Final frozen source SHA | **unverified** |
| Final-SHA hosted CI | **unverified** |
| Independently checked ZIPs / reports | **unverified** |
| Stage V / extended negative acceptance | **Stage V accepted 2026-11-25; Stage VI extended qualification still in progress** |
| Production host / rollback and limits | **not accepted** |
| OWNER merge decision | **separately required** |
| OWNER new Twilight tag/RC decision | **separately required — NO-GO** |

Do not overwrite `v0.9.20-rc.1` (historical High Starter). Any future Twilight tag must be a new, separately approved and explicitly scoped evaluation release. This Stage VI work never constitutes production GO.
