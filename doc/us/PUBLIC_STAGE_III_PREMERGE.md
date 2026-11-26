# Stage III exact-commit workflow and evidence

**CURRENT OWNER SCOPE (2026-11-27):** Only existing GitHub-hosted CI and formal OWNER review remain Stage III acceptance gates. Do **not** dispatch a new external/latest-SHA public DNS A/PTR or ADCS snapshot, real DC++ client test, or Contabo ingress/firewall/port test. They are explicitly excluded, as are controlled VPS and live ADCS restarts. The extended/snapshot commands below describe **historical, reusable workflow capabilities only** and are **not instructions to run them for this Stage III qualification**. Retain separate `OWNER STAGE III ACCEPTANCE GO` and `OWNER MERGE GO` before merge. [Current checklist](PUBLIC_STAGE_III_QUALIFICATION.md).

The public guest hub is `adcs://server-747585786.dc24h.eu:1512` at
`62.171.156.32`. [Stage III qualification report](PUBLIC_STAGE_III_QUALIFICATION.md).

## Automatic pull request qualification

`.github/workflows/public-stage3-qualification.yml` checks out the exact
candidate head SHA. The offline build, full relevant CTest suite, systemd
example verification, two-client loopback, TLS fixtures and stability tests
use GitHub-hosted Ubuntu 22.04, 24.04 and 26.04. Fixed package and systemd
commands run inside the temporary VMs. The CTest systemd fixture invokes
packaged system tools for offline unit verification and isolated transient
units. The candidate hub runs under an unprivileged service account.

Pull requests to `main` exercise the isolated matrix. Their reports mark
public checks NOT TESTED in that scope. After a push to protected `main` in
the upstream repository, independent Ubuntu 24.04 jobs verify public DNS/TLS,
the bounded public TCP set, real ncdc, and two independent guest sessions.
The public guest jobs require successful loopback and public identity
prerequisites. Missing, failed, or incomplete evidence blocks the gate for
required jobs. The former premerge OWNER public snapshot is **withdrawn from this Stage III
scope**; do not dispatch it. PR CI evidence is evaluated for the current head.

The evidence job downloads immutable Actions artifacts and publishes the
checksummed `stage3-qualification-report-<attempt>` artifact. Download
artifacts for the same run attempt and verify the internal SHA-256 manifests
from their individual directories using `sha256sum -c SHA256SUMS`.
Run the full workflow again for a complete new attempt.

## OWNER extended public stability

From the OWNER account in GitHub Actions, select the workflow registered
as `debian13-stage3-full-qualification.yml`, which already exists on protected
`main`. Select the OWNER-approved source branch and enter its verified full
SHA. The workflow on the selected branch
contains the extended tests. The expected duration is 32–35 minutes plus queue
time, with a 40-minute job timeout. It uses at most two concurrent synthetic
guests and a bounded connection schedule.

```bash
gh workflow run debian13-stage3-full-qualification.yml \
  --repo adrianf87/dc24h.eu-twilight \
  --ref test/stage3-public-adcs-final-qualification \
  -f confirmation=RUN_STAGE3_PUBLIC_EXTENDED_STABILITY \
  -f production_restarts_ack=false \
  -f premerge_phase=none \
  -f premerge_candidate_sha=REPLACE_WITH_VERIFIED_40_CHARACTER_SHA
```

Record the workflow run URL, exact SHA and expected duration. While queued or
running, record **PENDING**. On completion review
`stage3-extended-evidence-<attempt>` from the run's Artifacts section.
Existing historical public snapshot evidence may be cited as historical only;
do not request a new snapshot for this Stage III acceptance.

## OWNER before/after snapshot

For an external snapshot, use the same workflow and exact candidate SHA with
`confirmation=RUN_STAGE3_UBUNTU_EXTERNAL_ONLY`. Use `premerge_phase=before`
or `premerge_phase=after` as a label for an operational change, or `none` for
an ordinary candidate check. Use `production_restarts_ack=false`.
A separately authorized operator records the actual host maintenance times in
the Debian 13 record. Compare them with the independent external snapshots.
The snapshot produces the `stage3-external-snapshot-<phase>-<attempt>` artifact.

**OWNER exception:** A controlled reboot of the Debian 13 VPS and an intentional
restart of the live ADCS service are removed from the Stage III test plan.
Do not use the before/after snapshot feature to schedule those tests.
A new external snapshot is also excluded from this Stage III acceptance.
Missing external and restart evidence is neither a blocker nor a PASS.

The main ref is available for this workflow after an explicitly authorized
merge. Each run provides evidence for the checked-out commit. The final
decision requires OWNER review, and a merge requires separate `OWNER MERGE GO`.
