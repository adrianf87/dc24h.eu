#!/usr/bin/env python3
"""Fail-closed OWNER branch/main hosted Ubuntu 22.04 external-route qualification.

This script is intentionally safe on a GitHub-hosted Ubuntu 22.04 runner. It neither
connects to a host nor invokes sudo, systemd, certbot or any local secrets.
"""

import os
import re


REPOSITORY = "adrianf87/dc24h.eu-twilight"
CONFIRMATION = "RUN_STAGE3_UBUNTU2204_EXTERNAL_ONLY"
PHASES = frozenset({"none", "before", "after"})


def guard(env):
    phase = env.get("PREMERGE_PHASE", "")
    sha = env.get("PREMERGE_CANDIDATE_SHA", "")
    if not all((
        env.get("GITHUB_EVENT_NAME") == "workflow_dispatch",
        env.get("GITHUB_REPOSITORY") == REPOSITORY,
        env.get("GITHUB_ACTOR") == env.get("GITHUB_TRIGGERING_ACTOR") == "adrianf87",
        re.fullmatch(r"refs/heads/.+", env.get("GITHUB_REF", "")) is not None,
        env.get("RUNNER_ENVIRONMENT") == "github-hosted",
        env.get("OWNER_CONFIRMATION") == CONFIRMATION,
        env.get("PRODUCTION_RESTARTS_ACK") == "false",
        phase in PHASES,
        re.fullmatch(r"[0-9a-f]{40}", sha) is not None,
        sha == env.get("GITHUB_SHA"),
    )):
        raise RuntimeError("Stage III external Ubuntu 22.04 OWNER/ref/SHA/runner guard failed")
    return phase


if __name__ == "__main__":
    try:
        print("STAGE3_PREMERGE_EXTERNAL_GUARD_PASS phase=" + guard(os.environ), flush=True)
    except RuntimeError as exc:
        print("STAGE3_PREMERGE_EXTERNAL_GUARD_FAIL: " + str(exc), flush=True)
        raise SystemExit(2)
