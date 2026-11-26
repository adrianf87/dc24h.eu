# Stage III — PR #23 pre-merge qualification (OWNER procedure)

This procedure completes **external ncdc → controlled host restart/TLS →
independent external ncdc** checks before merging
`fix/stage3-ncdc-command-ack-regression` into `main`.

The public guest hub is `adcs://server-747585786.dc24h.eu:1512`
(`62.171.156.32`). It remains live except for brief, approved systemd
restarts. Never use the retired Stage III hostname or IPv4.

## Security boundary (mandatory)

- **Never run PR Python/source code as root on `server-747585786`.** The public
  repo has a self-hosted runner with passwordless `sudo`; GitHub warns about
  the risk of executing untrusted PR workflows. The **branch workflow YAML
  itself is executable and privileged in full mode**, so the OWNER must review
  its exact candidate commit before authorizing a manual dispatch. Checking
  out a pinned `main` SHA does not by itself make branch YAML trustworthy.
- All pre/post external tests run on `runs-on: ubuntu-24.04` (GitHub-hosted),
  with read-only repository permissions and synthetic public ADC traffic.
- The automatic `pull_request` event **never** schedules host `sudo`. An
  explicit OWNER `workflow_dispatch` from the reviewed branch with
  `premerge_phase=full`, the exact candidate SHA, the exact reviewed `main`
  SHA, a distinct confirmation and restart acknowledgement is required.
  `premerge-host` checks out only immutable `main` source and executes its
  existing `scripts/public_stage3_qualification_host.py` using `sudo -n`.
- Host restart/TLS authority is **separate** from the PR's external-test
  confirmation. The automated path requires
  `RUN_STAGE3_PREMERGE_FULL_QUALIFICATION` **and** the checked restart-ack box;
  the manual fallback requires `OWNER: RUN_STAGE3_PREMERGE_HOST_TESTS` at the
  host terminal. Do not infer restart approval from a successful PR check.
- No automated merge occurs from this procedure. The OWNER should review
  all required PR checks, the three evidence reports and chronology first.

## Automated full branch qualification — recommended

This is the complete **BEFORE → sudo HOST → AFTER → EVIDENCE** pipeline in a
single OWNER-dispatched GitHub Actions run **on the PR branch, before merge**.
It performs three public Stage III systemd restarts plus one same-pair TLS
rehearsal restart. Public guests may be briefly disconnected. Never dispatch
without an authorized maintenance window and independently available SSH.

1. Review the complete branch workflow YAML and all script changes on the
   exact candidate SHA. Verify current `main` SHA and the deployed unit.
2. Under `debian13-stage3-full-qualification.yml` choose **Run workflow** and
   select `fix/stage3-ncdc-command-ack-regression`.
3. Provide these **exact** inputs:

   | Input | Value |
   | --- | --- |
   | `confirmation` | `RUN_STAGE3_PREMERGE_FULL_QUALIFICATION` |
   | `production_restarts_ack` | `true` (checked, four brief restarts) |
   | `premerge_phase` | `full` |
   | `premerge_candidate_sha` | Exact reviewed PR #23 head SHA |
   | `premerge_trusted_main_sha` | Exact reviewed `main` SHA |

4. If the web UI displays outdated inputs from `main`, do not guess. A
   signed-in OWNER may explicitly pass the branch and inputs with GitHub CLI:

   ```bash
   gh workflow run debian13-stage3-full-qualification.yml \
     --ref fix/stage3-ncdc-command-ack-regression \
     -f confirmation=RUN_STAGE3_PREMERGE_FULL_QUALIFICATION \
     -f production_restarts_ack=true \
     -f premerge_phase=full \
     -f premerge_candidate_sha=REPLACE_WITH_VERIFIED_40_CHAR_PR23_SHA \
     -f premerge_trusted_main_sha=REPLACE_WITH_VERIFIED_40_CHAR_MAIN_SHA
   ```

5. Require **SUCCESS** in all four jobs: `premerge-external`,
   `premerge-host`, `premerge-external-after`, and `premerge-evidence`. The
   first and third run all real ncdc/chat/search/reconnect and bounded public
   port tests; the host job runs only the trusted-main service/TLS qualification.
   The final job downloads all three sanitized artifacts and checks the
   immutable candidate/trusted main SHA and correct PASS scopes.

The external-only `before`/`after` mode and the manual host procedure below
remain available as a fallback. They must **not** be combined with artifacts
from a different candidate SHA and must respect the BEFORE → HOST → AFTER
ordering. **Neither path merges PR #23 automatically.**

## Manual fallback path (OWNER operated)

### A. Freeze and check the candidate

1. Record the exact 40-character **head SHA** of PR #23 (not the synthetic
   GitHub pull-request merge SHA). Confirm the PR head belongs to the same
   repository and the branch is `fix/stage3-ncdc-command-ack-regression`.
2. Record the exact 40-character current `main` SHA. Review that commit's
   `scripts/public_stage3_qualification_host.py`, its imports and the
   deployed Stage III manifest. Any change to `main` requires a new review.
3. Stop immediately on missing credentials, identity mismatch, forbidden
   public listeners, unexpected cert/key state, failing CI or unapproved
   service interruption. Do not change firewall rules, public ports or keys
   to force a PASS.

### B. External ncdc qualification — BEFORE host tests

From the GitHub Actions workflow
`debian13-stage3-full-qualification.yml`, use **Run workflow** and select
the **PR #23 branch** (not `main`):

| Input | Exact value |
| --- | --- |
| `confirmation` | `RUN_STAGE3_PREMERGE_EXTERNAL` |
| `production_restarts_ack` | `false` (unchecked) |
| `premerge_phase` | `before` |
| `premerge_candidate_sha` | Exact candidate SHA from section A |

Only the `premerge-external` job may run. Its guard checks the OWNER actor,
repository, branch, GitHub-hosted environment and exact checkout SHA. Require
`SUCCESS` and download the `stage3-premerge-external-before` artifact, which
contains a sanitized `stage3-premerge-external.json`. Preserve the run URL and
UTC completion time. The PR's automatic `external-pr` PASS is useful but is
**not a substitute** for the explicitly labeled BEFORE run.

If GitHub's manual-run web form still shows the **old `main` inputs** after
selecting the PR branch, do not guess or dispatch with missing parameters.
Refresh and verify the selected ref. The OWNER can explicitly supply the
branch-specific inputs from a signed-in machine with GitHub CLI:

```bash
gh workflow run debian13-stage3-full-qualification.yml \
  --ref fix/stage3-ncdc-command-ack-regression \
  -f confirmation=RUN_STAGE3_PREMERGE_EXTERNAL \
  -f production_restarts_ack=false \
  -f premerge_phase=before \
  -f premerge_candidate_sha=REPLACE_WITH_VERIFIED_40_CHAR_PR23_SHA
```

Check the resulting workflow's ref and SHA before trusting its result; a
skipped job is **not** a successful qualification.

### C. Trusted `main` host checks — manually on the VPS

**This stage deliberately interrupts the public guest hub**: the trusted
script performs three `systemctl restart dc24h-stage3-adcs.service` cycles,
then one same-pair TLS copy/restart rehearsal. It does not invoke certbot,
change the Let's Encrypt archive or activate unrelated services.

Only after a distinct OWNER approval, use SSH to reach the approved VPS,
switch to `github-runner` (`sudo -iu github-runner` if needed), and execute
the following from that account. Replace both placeholders with the verified
40-character SHAs from section A; **never copy scripts from the PR branch**:

```bash
set -euo pipefail
umask 077
TRUSTED_MAIN_SHA='REPLACE_WITH_VERIFIED_40_CHAR_MAIN_SHA'
CANDIDATE_SHA='REPLACE_WITH_VERIFIED_40_CHAR_PR23_SHA'

test "$(id -un)" = 'github-runner'
test "$(hostname -s)" = 'server-747585786'
test "$(sudo -n id -u)" = '0'
[[ "$TRUSTED_MAIN_SHA" =~ ^[0-9a-f]{40}$ ]]
[[ "$CANDIDATE_SHA" =~ ^[0-9a-f]{40}$ ]]

REPOSITORY_URL='https://github.com/adrianf87/dc24h.eu-twilight.git'
ACTUAL_MAIN_SHA="$(git ls-remote "$REPOSITORY_URL" refs/heads/main | cut -f1)"
test "$ACTUAL_MAIN_SHA" = "$TRUSTED_MAIN_SHA"

TRUSTED_DIR="$(mktemp -d /home/github-runner/stage3-trusted-main.XXXXXXXX)"
git clone --depth 1 --branch main --no-checkout "$REPOSITORY_URL" "$TRUSTED_DIR"
git -C "$TRUSTED_DIR" checkout --detach "$TRUSTED_MAIN_SHA"
test "$(git -C "$TRUSTED_DIR" rev-parse HEAD)" = "$TRUSTED_MAIN_SHA"

printf 'Type OWNER: RUN_STAGE3_PREMERGE_HOST_TESTS to allow 4 service restarts: '
read -r OWNER_HOST_GO
test "$OWNER_HOST_GO" = 'OWNER: RUN_STAGE3_PREMERGE_HOST_TESTS'

cd "$TRUSTED_DIR"
sudo -n python3 scripts/public_stage3_qualification_host.py \
  --confirmation RUN_STAGE3_PUBLIC_QUALIFICATION \
  --actor adrianf87 --ref refs/heads/main \
  --runner server-747585786 --runner-user github-runner \
  --expected-sha "$TRUSTED_MAIN_SHA" \
  2>&1 | tee "$TRUSTED_DIR/stage3-premerge-host.sanitized.log"

printf '{"candidate_sha":"%s","source_sha":"%s","scope":"TRUSTED_MAIN_STAGE3_HOST_RESTARTS_AND_TLS_REHEARSAL","result":"PASS"}\n' \
  "$CANDIDATE_SHA" "$TRUSTED_MAIN_SHA" \
  > "$TRUSTED_DIR/stage3-premerge-host.json"
```

The existing main-script guards validate Debian 13, unit/manifest identity,
service-owned TLS files, root/runner identity, current checkout SHA, the
listener allowlist and active service. They abort on failure. **Do not**
write the host PASS JSON if the command fails, and do not suppress an exit
status. Preserve the sanitized log/JSON privately for OWNER review; they do
not contain TLS key bytes.

If a restart or the TLS rehearsal fails: **STOP**, inspect service status and
the script's rollback result from trusted main, restore availability under
OWNER supervision and keep PR #23 unmerged. Do not automatically retry
privileged operations.

### D. External ncdc qualification — AFTER host tests

Dispatch the **same workflow and exact unchanged candidate SHA** on the
PR #23 branch, with `confirmation=RUN_STAGE3_PREMERGE_EXTERNAL`,
`production_restarts_ack=false`, and `premerge_phase=after`.
If using the CLI fallback, repeat the command from B with `after` instead of
`before` and the identical candidate SHA.

Require `SUCCESS`. Download `stage3-premerge-external-after`, retain the run
URL and UTC start time, and verify it started **after the host step ended**.
This independently tests CA/SAN TLS, guest login, two-client chat/search,
real ncdc inbound/outbound chat, reconnects and forbidden ports after the
service was restarted. Any new PR commit invalidates **both** external
reports: rerun B/C/D against the new immutable head SHA.

### E. Evidence, CI and merge hold

Copy the two external JSON reports and manually generated host JSON into
separate folders (both external artifacts contain the same filename). From
any trusted machine with Python 3, run:

```bash
python3 scripts/stage3_premerge_evidence.py \
  --before before/stage3-premerge-external.json \
  --host stage3-premerge-host.json \
  --after after/stage3-premerge-external.json \
  --candidate-sha REPLACE_WITH_VERIFIED_40_CHAR_PR23_SHA \
  --trusted-main-sha REPLACE_WITH_VERIFIED_40_CHAR_MAIN_SHA
```

`STAGE3_PREMERGE_EVIDENCE_PASS` verifies that all three bounded reports are
PASS, their scopes and phases match, both external runs belong to the same PR
head, and host code comes from the exact trusted main SHA. It **does not**
authenticate the reports or prove ordering: separately verify the original
GitHub artifact provenance, run URLs and BEFORE → HOST → AFTER timestamps.

Merge PR #23 only after **all required PR CI checks are successful**, all
three reports have been reviewed, the live hub is healthy, and the OWNER has
explicitly accepted the result. Formal Stage III acceptance/Issue #10 closure
remains a separate step. The workflow's original protected-`main` full
qualification may still be required by acceptance policy after merge; do not
equate this pre-merge rehearsal with automated production certification.
