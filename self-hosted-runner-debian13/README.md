# Debian 13 self-hosted runner — supplemental testing only

This directory documents the OWNER-designated GitHub Actions runner
[`server-747585786`](https://github.com/adrianf87/dc24h.eu-twilight/settings/actions/runners/21)
for **opt-in, non-production** checks of `dc24h.eu-twilight`.

The OWNER reports Debian 13, 16 logical CPU threads, a `github-runner` account,
and passwordless sudo on that account. These are **OWNER-supplied claims, not
independently verified host inventory**. The runner's relationship to the public
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
`twilight-debian13-stage2`. The OWNER must first apply that exact label to the
intended runner in GitHub Settings; the label is **not claimed to exist yet**.

GitHub's standard workflows continue to use **GitHub-hosted Ubuntu** as required
CI. The self-hosted run is supplemental evidence only and is not a required PR
status check. GitHub only exposes a new `workflow_dispatch` workflow after its
workflow file exists on the default branch. It cannot be activated merely by
adding this file to an unmerged draft PR.

**No OWNER Stage II acceptance, merge, release, production deployment, private
state migration, or public hub/WebAdmin activation is granted here.**
