#!/usr/bin/env python3
"""Validate a redacted CTest report against the exact hosted-Ubuntu CI context.

This proves internal consistency of a GitHub Actions report, not a signed
attestation or a guarantee that the host environment cannot be compromised.
"""

import argparse
import json
import os
from pathlib import Path
import platform
import re


IMAGES = {"debian-13"}
SHA = re.compile(r"[0-9a-f]{40}\Z")
SUITE = re.compile(r"[a-zA-Z0-9_.-]{1,80}\Z")
EXPECTED_KEYS = {"schema", "suite", "commit_sha", "state", "tests", "failures", "skipped"}


def validate(report, *, sha, suite, image, environment, release):
    if not isinstance(sha, str) or not SHA.fullmatch(sha) or sha == "0" * 40:
        raise ValueError("Exact nonzero source SHA is required")
    if not isinstance(suite, str) or not SUITE.fullmatch(suite):
        raise ValueError("Invalid CI suite")
    if image not in IMAGES or release != image.removeprefix("debian-"):
        raise ValueError("Required self-hosted Debian 13 image does not match the running OS")
    if environment != "self-hosted":
        raise ValueError("Expected Debian 13 self-hosted evidence")
    if not isinstance(report, dict) or set(report) != EXPECTED_KEYS:
        raise ValueError("Incomplete or unreviewed CI result shape")
    if type(report["schema"]) is not int or report["schema"] != 1 or (
        report["commit_sha"] != sha or report["suite"] != suite or
        report["state"] != "passed" or type(report["tests"]) is not int or
        report["tests"] <= 0 or type(report["failures"]) is not int or
        report["failures"] != 0 or type(report["skipped"]) is not int or
        report["skipped"] != 0
    ):
        raise ValueError("Wrong-head, skipped or failed CI evidence")
    return {"schema": 1, "source_sha": sha, "os_image": image,
            "suite": suite, "result": "passed", "tests": report["tests"],
            "production_attestation": False}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--summary", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--sha", required=True)
    parser.add_argument("--suite", required=True)
    parser.add_argument("--os-image", required=True)
    args = parser.parse_args()
    if os.environ.get("GITHUB_ACTIONS") != "true" or os.environ.get("RUNNER_OS") != "Linux":
        parser.error("This evidence gate runs only on GitHub-self-hosted Debian 13 CI")
    if not args.summary.is_file() or args.summary.stat().st_size > 8192:
        parser.error("Missing or oversized redacted CI summary")
    try:
        report = json.loads(args.summary.read_text(encoding="utf-8"))
        checked = validate(report, sha=args.sha, suite=args.suite,
                           image=args.os_image,
                           environment=os.environ.get("RUNNER_ENVIRONMENT"),
                           release=platform.freedesktop_os_release().get("VERSION_ID"))
    except (ValueError, OSError, UnicodeError, json.JSONDecodeError) as error:
        parser.error("Unverifiable CI evidence: " + str(error))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("x", encoding="utf-8") as output:
        json.dump(checked, output, sort_keys=True, indent=2)
        output.write("\n")
    print("Verified self-hosted Debian 13 CI suite:", checked["suite"])


if __name__ == "__main__":
    main()
