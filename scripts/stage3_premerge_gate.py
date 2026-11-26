#!/usr/bin/env python3
"""Fail-closed OWNER pre-merge *external-only* qualification guard.

This script is intentionally safe on a GitHub-hosted runner. It neither
connects to a host nor invokes sudo, systemd, certbot or any local secrets.
"""

import os
import re


REPOSITORY = "adrianf87/dc24h.eu-twilight"
BRANCH = "refs/heads/fix/stage3-ncdc-command-ack-regression"
CONFIRMATION = "RUN_STAGE3_PREMERGE_EXTERNAL"
FULL_CONFIRMATION = "RUN_STAGE3_PREMERGE_FULL_QUALIFICATION"
PHASES = frozenset({"before", "after", "full"})


def guard(env):
    phase = env.get("PREMERGE_PHASE", "")
    sha = env.get("PREMERGE_CANDIDATE_SHA", "")
    full = phase == "full"
    if not all((
        env.get("GITHUB_EVENT_NAME") == "workflow_dispatch",
        env.get("GITHUB_REPOSITORY") == REPOSITORY,
        env.get("GITHUB_ACTOR") == "adrianf87",
        env.get("GITHUB_REF") == BRANCH,
        env.get("RUNNER_ENVIRONMENT") == "github-hosted",
        env.get("OWNER_CONFIRMATION") == (FULL_CONFIRMATION if full else CONFIRMATION),
        env.get("PRODUCTION_RESTARTS_ACK") == ("true" if full else "false"),
        phase in PHASES,
        re.fullmatch(r"[0-9a-f]{40}", sha) is not None,
        sha == env.get("GITHUB_SHA"),
        not full or re.fullmatch(r"[0-9a-f]{40}",
                                env.get("PREMERGE_TRUSTED_MAIN_SHA", "")) is not None,
    )):
        raise RuntimeError("Stage III pre-merge external OWNER/ref/SHA/runner guard failed")
    return phase


if __name__ == "__main__":
    try:
        print("STAGE3_PREMERGE_EXTERNAL_GUARD_PASS phase=" + guard(os.environ), flush=True)
    except RuntimeError as exc:
        print("STAGE3_PREMERGE_EXTERNAL_GUARD_FAIL: " + str(exc), flush=True)
        raise SystemExit(2)
