#!/usr/bin/env python3
"""Verify sanitized pre-merge Stage III external/host/external evidence.

No network, sudo or private state; only bounded machine-readable PASS reports.
Run chronology must also be checked against the original GitHub timestamps.
"""

import argparse
import json
import re
from pathlib import Path


EXTERNAL_SCOPE = "PREMERGE_EXTERNAL_NCDC_NO_HOST_WRITES"
HOST_SCOPE = "TRUSTED_MAIN_STAGE3_HOST_RESTARTS_AND_TLS_REHEARSAL"


def validate(before, host, after, candidate_sha, trusted_main_sha):
    if (re.fullmatch(r"[0-9a-f]{40}", candidate_sha) is None or
            re.fullmatch(r"[0-9a-f]{40}", trusted_main_sha) is None):
        raise RuntimeError("Expected immutable commit SHA values")
    for phase, report in (("before", before), ("after", after)):
        if not isinstance(report, dict) or any((
            report.get("source_sha") != candidate_sha,
            report.get("phase") != phase,
            report.get("scope") != EXTERNAL_SCOPE,
            report.get("result") != "PASS",
        )):
            raise RuntimeError("External pre-merge evidence mismatch at phase=" + phase)
    if not isinstance(host, dict) or any((
        host.get("candidate_sha") != candidate_sha,
        host.get("source_sha") != trusted_main_sha,
        host.get("scope") != HOST_SCOPE,
        host.get("result") != "PASS",
    )):
        raise RuntimeError("Trusted main host evidence mismatch")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--before", type=Path, required=True)
    parser.add_argument("--host", type=Path, required=True)
    parser.add_argument("--after", type=Path, required=True)
    parser.add_argument("--candidate-sha", required=True)
    parser.add_argument("--trusted-main-sha", required=True)
    args = parser.parse_args()
    try:
        reports = [json.loads(path.read_text(encoding="utf-8"))
                   for path in (args.before, args.host, args.after)]
        validate(*reports, args.candidate_sha, args.trusted_main_sha)
    except (OSError, ValueError, RuntimeError) as exc:
        print("STAGE3_PREMERGE_EVIDENCE_FAIL: " + type(exc).__name__, flush=True)
        raise SystemExit(2)
    print("STAGE3_PREMERGE_EVIDENCE_PASS (check original run chronology)", flush=True)


if __name__ == "__main__":
    main()
