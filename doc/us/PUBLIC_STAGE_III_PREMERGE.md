# Stage III — PR #23 qualification using GitHub-hosted Ubuntu only

The public guest ADCS hub is `adcs://server-747585786.dc24h.eu:1512`, IPv4
`62.171.156.32`. Do not use the retired alternative target.

## OWNER runner decision

The Debian 13 VPS is **not a GitHub Actions runner**. The entire former runner
directory, dedicated Debian runner workflows, root-enabled host qualification
and activation automation have been removed from this branch. Every remaining
CI job uses GitHub-hosted Ubuntu 22.04, 24.04 or 26.04.

In this configuration, `sudo` is allowed **only inside ephemeral GitHub Ubuntu
VMs** for trusted package installation and benign OS-tool checks. It cannot
restart the public VPS, inspect its systemd unit, access its private Let's
Encrypt certificate files or prove a real host rollback. Never infer those
outcomes from a successful external CI run.

## Before merge: automatic regression

The `pull_request` run of
`.github/workflows/debian13-stage3-full-qualification.yml` includes:

1. Offline Stage III policy and ncdc regression tests on Ubuntu 24.04.
2. Safe `sudo -n id -u` and systemd-tool presence checks on ephemeral Ubuntu
   22.04, 24.04 and 26.04 environments.
3. A real `ncdc` client, two synthetic guest ADC clients, inbound and outbound
   chat, search, CTM/RCM, reconnect, public CA/SAN TLS identity and bounded
   forbidden TCP ports, from independent GitHub Ubuntu 24.04 networking.

The GitHub runner uses `sudo` to install the packaged `ncdc` executable **on
its own VM**, never on the Debian hub VPS. The external job uploads a sanitized
`stage3-ubuntu-external-pr` report. A skipped job is not a PASS.

## Manual exact-SHA external qualification on the branch or main

Under the same workflow select **Run workflow** and the desired ref:

- `fix/stage3-ncdc-command-ack-regression` for **before-merge** tests;
- `main` for **after-merge** tests, only when OWNER separately authorizes merge.

Required inputs:

| Input | Value |
| --- | --- |
| `confirmation` | `RUN_STAGE3_UBUNTU_EXTERNAL_ONLY` |
| `production_restarts_ack` | `false` (no VPS restarts) |
| `premerge_phase` | `none`, `before`, or `after` (report label) |
| `premerge_candidate_sha` | Exact 40-character SHA of the selected ref |

CLI example for the unmerged branch:

```bash
gh workflow run debian13-stage3-full-qualification.yml \
  --ref fix/stage3-ncdc-command-ack-regression \
  -f confirmation=RUN_STAGE3_UBUNTU_EXTERNAL_ONLY \
  -f production_restarts_ack=false \
  -f premerge_phase=before \
  -f premerge_candidate_sha=REPLACE_WITH_VERIFIED_40_CHAR_BRANCH_SHA
```

The OWNER, checked-out commit and runner type are verified before public
testing. The result is **external-only** evidence. Selecting `after` does not
prove that any real server restart happened between the two runs.

## Host operations removed from CI

The running public service is independent of GitHub-hosted Ubuntu. Actual
Debian 13 systemd restarts, on-host listener audit, local filesystem backup,
TLS pair replacement, restore, rollback and certificate renewal remain
**unverified by this workflow**. Those tasks require a separate, OWNER-operated
VPS maintenance procedure with an independently confirmed backup, remote SSH
access and maintenance window. This repository must not silently reconnect
an Ubuntu runner to the VPS over SSH or introduce secret-based remote sudo.

**Merge hold:** require green GitHub Ubuntu policy/ncdc/port CI, independently
document any missing live-host acceptance criteria, and obtain a separate
OWNER decision before merging PR #23 or closing Issue #10. Do not label Stage
III fully accepted based on external-only checks.
