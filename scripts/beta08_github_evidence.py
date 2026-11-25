#!/usr/bin/env python3
"""Read-only, authenticated GitHub Actions final-head qualification.

Use only AFTER all PR workflows finish. Never accept a user-authored CI manifest
as proof; fetch runs, exact-attempt jobs and immutable artifact digests directly
from GitHub's REST API. This checker never merges, releases or deploys.
"""

import argparse
from datetime import datetime, timedelta
import json
import os
import re
import subprocess
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path


REPO = "adrianf87/dc24h.eu-high-starter"
BRANCH = "beta-0.8.x-stage-full"
FIX_BRANCH = "beta-0.9.x-stage-full-fix-pass"
IMAGES = ("ubuntu-22.04", "ubuntu-24.04", "ubuntu-26.04")
SHA = re.compile(r"[0-9a-f]{40}\Z")
DIGEST = re.compile(r"sha256:[0-9a-f]{64}\Z")
WORKFLOWS = {
    "build.yml": {
        *(f"C++ {build} - {image}" for image in IMAGES for build in ("Debug", "Release")),
        *(f"WebAdmin - {image}" for image in IMAGES),
        "C++ address - ubuntu-24.04", "C++ undefined - ubuntu-24.04",
        "CI (C++ and WebAdmin)",
    },
    "stage4-ubuntu-vm-validation.yml": {f"Stage IV VM - {image}" for image in IMAGES},
    "beta07-codeql.yml": {f"CodeQL C++ and TypeScript - {image}" for image in IMAGES},
    "beta08-hardening.yml": {"TSAN - ubuntu-24.04", "ADC parser fuzz - ubuntu-24.04"},
}
RC_WORKFLOWS = {
    "beta09-stage4-storage.yml": {f"Stage IV private storage - {image}" for image in IMAGES},
}


def _collection(value, field):
    if (not isinstance(value, dict) or type(value.get("total_count")) is not int or
            value["total_count"] > 100 or value["total_count"] < 0 or
            not isinstance(value.get(field), list) or
            len(value[field]) != value["total_count"]):
        raise ValueError("Missing, paginated or malformed GitHub API collection")
    return value[field]


def _expected_artifacts(workflow, run_id, attempt):
    suffix = f"-{run_id}-{attempt}"
    if workflow == "build.yml":
        return ({f"cpp-{image}-{build}{suffix}" for image in IMAGES for build in ("Debug", "Release")}
                | {f"beta07-npm-audit-{image}{suffix}" for image in IMAGES}
                | {f"beta08-build-audit-{image}{suffix}" for image in IMAGES}
                | {f"sanitizer-{kind}{suffix}" for kind in ("address", "undefined")})
    if workflow == "stage4-ubuntu-vm-validation.yml":
        return {f"stage4-ubuntu-vm-{image}{suffix}" for image in IMAGES}
    if workflow == "beta08-hardening.yml":
        return {f"tsan-evidence{suffix}", f"fuzz-evidence{suffix}"}
    return set()  # CodeQL is a successful completed job, not a fabricated artifact.


def _time(value):
    if not isinstance(value, str) or not value.endswith("Z"):
        raise ValueError("Missing GitHub UTC event timestamp")
    try:
        parsed = datetime.fromisoformat(value.replace("Z", "+00:00"))
    except ValueError as error:
        raise ValueError("Invalid GitHub event timestamp") from error
    return parsed


def _current_branch(get, sha, branch=BRANCH):
    ref = get(f"/branches/{branch}")
    if (not isinstance(ref, dict) or ref.get("name") != branch or
            not isinstance(ref.get("commit"), dict) or ref["commit"].get("sha") != sha):
        raise ValueError("Source SHA is no longer the current development branch HEAD")


def evaluate(get, sha, branch=BRANCH, event="pull_request", artifact_callback=None):
    if (not isinstance(sha, str) or not SHA.fullmatch(sha) or
            sha == "0" * 40 or branch not in (BRANCH, "beta-0.9.x-stage-full", FIX_BRANCH, "main") or
            event not in ("pull_request", "push") or
            (event == "push") != (branch in ("main", FIX_BRANCH))):
        raise ValueError("Invalid exact source head or development branch")
    # An old, entirely green run is not final-head evidence after a new push.
    # Re-read the authoritative branch at the end to catch concurrent updates.
    _current_branch(get, sha, branch)
    runs = _collection(get(f"/actions/runs?head_sha={sha}&per_page=100"), "workflow_runs")
    selected = {}
    required = ({**WORKFLOWS, **RC_WORKFLOWS} if branch in
                ("beta-0.9.x-stage-full", FIX_BRANCH, "main") else WORKFLOWS)
    for workflow in required:
        matching = [r for r in runs if isinstance(r, dict) and
                    r.get("path", "").split("@", 1)[0] == f".github/workflows/{workflow}" and
                    r.get("head_sha") == sha and r.get("head_branch") == branch and
                    r.get("event") == event]
        if not matching:
            raise ValueError("Required exact-head workflow run absent")
        # The latest triggered run is authoritative, including when it failed;
        # older green runs must not mask a newer failed or pending attempt.
        run = max(matching, key=lambda r: r.get("id", -1))
        if (type(run.get("id")) is not int or run["id"] < 1 or
                type(run.get("run_attempt")) is not int or run["run_attempt"] < 1 or
                run.get("status") != "completed" or run.get("conclusion") != "success" or
                run.get("repository", {}).get("full_name") != REPO):
            raise ValueError("Pending/failed/wrong-repository workflow run")
        run_id, attempt = run["id"], run["run_attempt"]
        jobs = _collection(get(f"/actions/runs/{run_id}/attempts/{attempt}/jobs?per_page=100"), "jobs")
        identities = set()
        job_windows = []
        for job in jobs:
            if not isinstance(job, dict):
                raise ValueError("Malformed workflow job")
            name = job.get("name")
            image = next((image for image in IMAGES if name and name.endswith(image)),
                         "ubuntu-24.04" if name == "CI (C++ and WebAdmin)" else None)
            if (name not in required[workflow] or name in identities or image is None or
                    job.get("head_sha") != sha or job.get("run_id") != run_id or
                    job.get("status") != "completed" or job.get("conclusion") != "success" or
                    not isinstance(job.get("labels"), list) or image not in job["labels"]):
                raise ValueError("Missing, skipped, foreign or non-hosted CI job")
            identities.add(name)
            start, end = _time(job.get("started_at")), _time(job.get("completed_at"))
            if end < start:
                raise ValueError("Invalid workflow attempt job time window")
            job_windows.append((start, end))
        if identities != required[workflow]:
            raise ValueError("Incomplete exact-attempt workflow job matrix")
        artifacts = _collection(get(f"/actions/runs/{run_id}/artifacts?per_page=100"), "artifacts")
        expected = _expected_artifacts(workflow, run_id, attempt)
        observed = {}
        artifact_ids = set()
        for artifact in artifacts:
            if not isinstance(artifact, dict) or artifact.get("name") not in expected:
                continue  # Older attempts may remain in the same workflow run.
            name = artifact["name"]
            created = _time(artifact.get("created_at"))
            artifact_id = artifact.get("id")
            if (name in observed or type(artifact_id) is not int or artifact_id <= 0 or
                    artifact_id in artifact_ids or artifact.get("expired") is not False or
                    type(artifact.get("size_in_bytes")) is not int or
                    not 0 < artifact["size_in_bytes"] <= 64 * 1024 * 1024 or
                    not isinstance(artifact.get("digest"), str) or
                    not DIGEST.fullmatch(artifact["digest"]) or
                    artifact.get("workflow_run", {}).get("id") != run_id or
                    artifact.get("workflow_run", {}).get("head_sha") != sha or
                    artifact.get("workflow_run", {}).get("head_branch") != branch or
                    created < min(start for start, _ in job_windows) - timedelta(minutes=2) or
                    created > max(end for _, end in job_windows) + timedelta(minutes=2)):
                raise ValueError("Unverified artifact digest, origin or attempt")
            observed[name] = artifact["digest"]
            artifact_ids.add(artifact_id)
            if artifact_callback is not None:
                artifact_callback(artifact, workflow)
        if set(observed) != expected:
            raise ValueError("Missing exact-attempt redacted artifact with digest")
        selected[workflow] = {"run_id": run_id, "attempt": attempt,
                              "jobs": len(identities), "artifact_digests": observed}
    _current_branch(get, sha, branch)
    return {"schema": 1, "source_sha": sha, "branch": branch,
            "github_api_authenticated": True, "verified_workflows": selected,
            "all_required_hosted_checks_successful": True,
            "artifact_digests_verified_from_github_metadata_only": True,
            "artifact_archive_contents_independently_verified": False,
            "owner_release_approval": False, "merge_authorized": False,
            "production_deployment_authorized": False}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--sha", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(argv)
    token = os.environ.get("GITHUB_TOKEN", "")
    if not token or len(token) > 2048:
        parser.exit(1, "Read-only GitHub Actions token required; no acceptance.\n")
    try:
        checked_out = subprocess.check_output(["git", "-C", str(args.root), "rev-parse", "HEAD"],
                                              timeout=10).decode("ascii").strip()
        if checked_out != args.sha:
            raise ValueError("Wrong checkout")

        def github_get(path):
            url = "https://api.github.com/repos/" + REPO + path
            request = urllib.request.Request(url, headers={
                "Authorization": "Bearer " + token,
                "Accept": "application/vnd.github+json",
                "X-GitHub-Api-Version": "2022-11-28", "User-Agent": "dc24h-beta08-evidence"})
            with urllib.request.urlopen(request, timeout=15) as response:
                payload = response.read(2 * 1024 * 1024 + 1)
                if response.status != 200 or len(payload) > 2 * 1024 * 1024:
                    raise ValueError("Unbounded or unavailable API data")
                return json.loads(payload)

        report = evaluate(github_get, args.sha)
        with args.output.open("x", encoding="utf8") as output:
            json.dump(report, output, sort_keys=True)
            output.write("\n")
    except (OSError, ValueError, TypeError, KeyError, json.JSONDecodeError,
            subprocess.SubprocessError, urllib.error.URLError):
        parser.exit(1, "Exact-head GitHub API evidence unavailable or incomplete; NO-GO.\n")
    print("Authentic exact-attempt GitHub API metadata verified; Owner approval still required")


if __name__ == "__main__":
    raise SystemExit(main())
