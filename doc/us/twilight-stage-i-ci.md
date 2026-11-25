# dc24h.eu-twilight — Stage I: repository identity and CI evidence

This document covers GitHub Actions qualification only. **It does not authorize
a merge, an RC release, or a production deployment.** See Issue #1 for scope.

## Two distinct repository identities

| Role | Repository | Purpose |
| --- | --- | --- |
| Historical RC1 source | `adrianf87/dc24h.eu-high-starter` | PR #243, Fix Pass, `v0.9.20-rc.1`, and original evidence |
| New project | `adrianf87/dc24h.eu-twilight` | Independent PRs, workflow runs, artifacts, and current Twilight evidence |

Do not rewrite genuine historical source URLs to point to Twilight. The
`scripts/beta08_github_evidence.py` and `scripts/beta09_artifact_evidence.py`
tools default to the original source repository to preserve legacy verification.
Twilight requires an **explicit** `--repository` and the `twilight-pr` mode
(or `main` after a separately approved merge, when the `push` event is verified).

## Example: verify the Stage I pull request

Run this only when **all five workflows** for the exact PR branch SHA have
finished, using a local checkout at that SHA:

```bash
python3 scripts/beta09_artifact_evidence.py \
  --root . \
  --sha "$(git rev-parse HEAD)" \
  --repository adrianf87/dc24h.eu-twilight \
  --mode twilight-pr \
  --branch fix/twilight-ci-repository-identity \
  --pr-number PR_NUMBER \
  --output /tmp/twilight-stage-i-evidence.json
```

Replace `PR_NUMBER` with the number of the **open Twilight PR** targeting
`main`. The process needs a `GITHUB_TOKEN` environment variable with read-only
access to GitHub Actions metadata and artifacts. Never put the token in
arguments or logs. The output path must not already exist.

The verifier:

- confirms that the PR belongs to Twilight, targets `main`, and references
  the exact `head.ref` and `head.sha`;
- reads the source branch `HEAD` twice and rejects any intervening change;
- requires a complete, successful set of C++/WebAdmin, CodeQL, VM/systemd,
  TSAN/fuzz, and storage jobs on the intended GitHub-hosted Ubuntu runners;
- rejects skipped checks, foreign repositories, wrong SHAs, older attempts,
  or incomplete job and artifact inventories;
- downloads GitHub artifact ZIPs, independently checks SHA-256, and parses only
  the explicitly permitted, redacted reports.

Negative tests: `tests/beta08_github_evidence_tests.py`,
`tests/beta09_artifact_evidence_tests.py`, and
`tests/beta09_release_candidate_gate_tests.py`. Exactly **8**
`github.repository` conditions across **5** workflow files are protected by
`tests/beta_baseline_tests.py`.

## `main` protection and acceptance

At the original Stage I audit, `main` in the new repository **was not
protected**. Once genuine PR checks are green, OWNER should configure GitHub
Settings → Rules → Rulesets (or Branch protection) to require PR review and
the actual, stable status checks on `main`. Never configure required checks
based on skipped results or historical RC1 evidence.

Verifier output and passing GitHub CI are technical evidence; they **do not
constitute** OWNER authorization to change protection settings, merge, or
publish a new RC.
