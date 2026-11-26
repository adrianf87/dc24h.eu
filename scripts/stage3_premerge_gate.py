#!/usr/bin/env python3
"""Fail-closed OWNER branch/main GitHub Ubuntu external-only qualification.

This script is intentionally safe on a GitHub-hosted runner. It neither
connects to a host nor invokes sudo, systemd, certbot or any local secrets.
"""

import os
import re


REPOSITORY = "adrianf87/dc24h.eu-twilight"
BRANCH = "refs/heads/fix/stage3-ncdc-command-ack-regression"
CONFIRMATION = "RUN_STAGE3_UBUNTU_EXTERNAL_ONLY"
REFS = frozenset({BRANCH, "refs/heads/main"})
PHASES = frozenset({"none", "before", "after"})


def guard(env):
    phase = env.get("PREMERGE_PHASE", "")
    sha = env.get("PREMERGE_CANDIDATE_SHA", "")
    if not all((
        env.get("GITHUB_EVENT_NAME") == "workflow_dispatch",
        env.get("GITHUB_REPOSITORY") == REPOSITORY,
        env.get("GITHUB_ACTOR") == "adrianf87",
        env.get("GITHUB_REF") in REFS,
        env.get("RUNNER_ENVIRONMENT") == "github-hosted",
        env.get("OWNER_CONFIRMATION") == CONFIRMATION,
        env.get("PRODUCTION_RESTARTS_ACK") == "false",
        phase in PHASES,
        re.fullmatch(r"[0-9a-f]{40}", sha) is not None,
        sha == env.get("GITHUB_SHA"),
    )):
        raise RuntimeError("Stage III external Ubuntu OWNER/ref/SHA/runner guard failed")
    return phase


if __name__ == "__main__":
    try:
        print("STAGE3_PREMERGE_EXTERNAL_GUARD_PASS phase=" + guard(os.environ), flush=True)
    except RuntimeError as exc:
        print("STAGE3_PREMERGE_EXTERNAL_GUARD_FAIL: " + str(exc), flush=True)
        raise SystemExit(2)
