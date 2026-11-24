#!/usr/bin/env python3
"""Fail closed on production dependency advisories without exporting raw JSON.

The npm audit JSON may contain package paths, registry data or advisory text.
Only aggregate severity counts are allowed in the CI artifact or stdout.
"""

import argparse
import json
from pathlib import Path
import re
import subprocess


SHA = re.compile(r"[0-9a-f]{40}\Z")
IMAGES = {"ubuntu-22.04", "ubuntu-24.04", "ubuntu-26.04"}
LEVELS = ("info", "low", "moderate", "high", "critical")


def evaluate(raw, source_sha, checkout_sha, os_image, audit_exit):
    if (not isinstance(source_sha, str) or not SHA.fullmatch(source_sha) or
            source_sha != checkout_sha or os_image not in IMAGES or
            type(audit_exit) is not int or audit_exit != 0):
        raise ValueError("Missing successful exact-head npm audit")
    if (not isinstance(raw, dict) or raw.get("auditReportVersion") != 2 or
            raw.get("error") is not None or not isinstance(raw.get("vulnerabilities"), dict) or
            not isinstance(raw.get("metadata"), dict)):
        raise ValueError("Incomplete or failed npm audit response")
    counts = raw["metadata"].get("vulnerabilities")
    if (not isinstance(counts, dict) or set(counts) != set(LEVELS) | {"total"} or
            any(type(counts[name]) is not int or counts[name] < 0 for name in (*LEVELS, "total")) or
            counts["total"] != sum(counts[name] for name in LEVELS)):
        raise ValueError("Invalid npm advisory severity totals")
    findings = raw["vulnerabilities"]
    observed = {name: 0 for name in LEVELS}
    for finding in findings.values():
        if not isinstance(finding, dict) or finding.get("severity") not in LEVELS:
            raise ValueError("Unrecognized or malformed npm advisory")
        observed[finding["severity"]] += 1
    if len(findings) != counts["total"] or any(observed[name] != counts[name] for name in LEVELS):
        raise ValueError("npm advisory catalog does not match totals")
    if counts["high"] or counts["critical"]:
        raise ValueError("Unreviewed high-severity production dependency advisory")
    return {"schema": 1, "source_sha": source_sha, "os_image": os_image,
            "production_dependencies_audited": True,
            "advisory_totals": {name: counts[name] for name in LEVELS},
            "unreviewed_high_or_critical": 0, "raw_advisories_exported": False,
            "production_release_approved": False}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--sha", required=True)
    parser.add_argument("--os-image", required=True)
    parser.add_argument("--audit", type=Path, required=True)
    parser.add_argument("--audit-exit", type=int, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        if args.audit.stat().st_size > 4 * 1024 * 1024:
            raise ValueError("Oversized private audit result")
        checkout_sha = subprocess.check_output(
            ["git", "-C", str(args.root), "rev-parse", "HEAD"],
            timeout=10).decode("ascii").strip()
        report = evaluate(json.loads(args.audit.read_text(encoding="utf8")),
                          args.sha, checkout_sha, args.os_image, args.audit_exit)
    except (OSError, ValueError, TypeError, KeyError, UnicodeError, json.JSONDecodeError,
            subprocess.CalledProcessError, subprocess.TimeoutExpired):
        parser.exit(1, "npm production dependency audit refused; private details suppressed.\n")
    with args.output.open("x", encoding="utf8") as file:
        json.dump(report, file, sort_keys=True); file.write("\n")
    print("Exact-head production dependency advisory gate passed; no raw advisories exported")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
