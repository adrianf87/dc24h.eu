# Debian 13 self-hosted runner — supplemental testing only

This directory documents the OWNER-designated GitHub Actions runner
[`server-747585786`](https://github.com/adrianf87/dc24h.eu-twilight/settings/actions/runners/21)
for **opt-in, non-production** checks of `dc24h.eu-twilight`.

The first reviewed [read-only runner test](https://github.com/adrianf87/dc24h.eu-twilight/actions/runs/45757153126)
confirmed Debian 13, 16 logical CPU threads, the `github-runner` account,
systemd, inactive hub services, and basic build tools. Passwordless sudo is
OWNER-authorized but its noninteractive operation is **not yet independently
verified**. The runner's relationship to the public
hub IPv4 `51.255.80.233` has not been verified.

## Contents

- [Runner profile](doc/RUNNER_PROFILE.md) — identity, claimed capabilities, and required verification.
- [Security and test policy](doc/TEST_POLICY.md) — privileged-runner risk, opt-in gate, and forbidden operations.
- [Test plan](doc/STAGE_II_TEST_PLAN.md) — allowed smoke tests and still-open Stage II acceptance.
- [Supplemental test tools](tests/README.md) — bounded, read-only checks and local regression tests.

## CI integration

The workflow [`.github/workflows/debian13-supplemental.yml`](../.github/workflows/debian13-supplemental.yml)
is **manual-only** (`workflow_dispatch`). It has no `push` or `pull_request` trigger,
never calls `sudo`, and requires a dedicated runner label
`twilight-debian13-stage2`. This label has been verified on runner #21.

The separate [Stage II sudo qualification workflow](../.github/workflows/debian13-stage2-sudo.yml)
originally performed only inventory and synthetic checks. Following explicit
OWNER confirmation that `server-747585786` is a dedicated test VPS without
production secrets/state, the revised workflow adds a third `sudo -n` call for
real test-only `/opt` installation and temporary `systemd` service, conflict,
crash/restart, backup/restore and binary rollback checks. The unprivileged
runner keeps its existing passwordless sudo privileges; no `sudoers` changes
are required. The rehearsal never enables the real hub units or accesses
existing `/etc/dc24h` and `/var/lib/dc24h` state. It requires a separately
reviewed merge into protected `main` and a new explicit OWNER dispatch phrase.
Build dependencies still carry root-escalation risk on an unrestricted-sudo
runner; only run it on the approved disposable test host.

GitHub's standard workflows continue to use **GitHub-hosted Ubuntu** as required
CI. The self-hosted run is supplemental evidence only and is not a required PR
status check. GitHub only exposes a new `workflow_dispatch` workflow after its
workflow file exists on the default branch. It cannot be activated merely by
adding this file to an unmerged draft PR.

**No OWNER Stage II acceptance, merge, release, production deployment, private
state migration, or public hub/WebAdmin activation is granted here.**
