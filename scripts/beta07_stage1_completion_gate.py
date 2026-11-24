#!/usr/bin/env python3
"""Offline, fail-closed Stage 1 completion checker for approved GitHub CI.

The input must be assembled from actual current-head hosted workflow results,
not manual declarations. No API secret, workflow token, names or raw logs enter
the output. This gate never marks a PR ready or merges on its own.
"""

import argparse
import json
from pathlib import Path
import re
import subprocess


IMAGES = ("ubuntu-22.04", "ubuntu-24.04", "ubuntu-26.04")
REQUIRED = {
    *(("cpp", image, configuration) for image in IMAGES for configuration in ("Debug", "Release")),
    *(("webadmin", image, "browser") for image in IMAGES),
    *(("vm", image, "integrated") for image in IMAGES),
    *(("codeql", image, "cpp-typescript") for image in IMAGES),
    ("sanitizer", "ubuntu-24.04", "address"),
    ("sanitizer", "ubuntu-24.04", "undefined"),
}
SHA = re.compile(r"[0-9a-f]{40}\Z")


def evaluate(doc, checked_out_sha):
    if not isinstance(doc, dict) or set(doc) != {"schema", "head_sha", "patches", "jobs", "artifacts", "version"} or (
            doc.get("schema") != 1 or not isinstance(doc.get("head_sha"), str) or
            not SHA.fullmatch(doc["head_sha"]) or doc["head_sha"] != checked_out_sha or
            doc.get("version") != "beta-0.7.20"):
        raise ValueError("Missing final exact-head beta-0.7.20 qualification")
    patches = doc.get("patches")
    if not isinstance(patches, dict) or set(patches) != {f"0.7.{n}" for n in range(1, 21)} or (
            any(value is not True for value in patches.values())):
        raise ValueError("Stage 1 must verify all 20 patches on one head")
    jobs = doc.get("jobs")
    if not isinstance(jobs, list) or len(jobs) != len(REQUIRED):
        raise ValueError("Incomplete exact-head CI matrix")
    seen = set()
    for job in jobs:
        if not isinstance(job, dict) or set(job) != {"kind", "os_image", "configuration", "head_sha", "status", "conclusion", "run_id"}:
            raise ValueError("Invalid CI job proof")
        identity = (job["kind"], job["os_image"], job["configuration"])
        if (identity not in REQUIRED or identity in seen or job["head_sha"] != checked_out_sha or
                job["status"] != "completed" or job["conclusion"] != "success" or
                type(job["run_id"]) is not int or job["run_id"] < 1):
            raise ValueError("Pending, failed, skipped, duplicate or wrong-head job")
        seen.add(identity)
    if seen != REQUIRED:
        raise ValueError("Missing required Ubuntu or CodeQL check")
    artifacts = doc.get("artifacts")
    names = {"ncdc_matrix", "real_passkey", "matched_six_store", "installed_dpkg_rollback",
             "three_transport_windows", "bounded_exhaustion", "npm_advisories",
             "action_lock_provenance", "risk_register", "pl_runbook"}
    if (not isinstance(artifacts, dict) or set(artifacts) != names or
            any(value is not True for value in artifacts.values())):
        raise ValueError("Missing mandatory real-process acceptance evidence")
    return {"schema": 1, "source_sha": checked_out_sha,
            "hosted_ubuntu_images": list(IMAGES), "accepted_patches": 20,
            "successful_required_jobs": len(REQUIRED), "accepted_evidence_categories": len(names),
            "stage1_ci_complete": True, "stage2_complete": False,
            "merge_authorized": False, "production_deployment_authorized": False}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(argv)
    head = subprocess.check_output(["git", "-C", str(args.root), "rev-parse", "HEAD"],
                                   timeout=10).decode("ascii").strip()
    result = evaluate(json.loads(args.manifest.read_text()), head)
    with args.output.open("x", encoding="utf8") as file:
        json.dump(result, file, sort_keys=True); file.write("\n")
    print("Stage 1 exact-head evidence qualified; Stage 2 and merge still require separate approval")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
