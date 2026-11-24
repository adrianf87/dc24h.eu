#!/usr/bin/env python3
"""Gate full npm runtime AND build-chain advisories without exposing raw JSON."""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess

import beta07_npm_audit_gate as audit


def evaluate(raw, sha, checked_out, image, audit_exit, lockbytes):
    old = audit.evaluate(raw, sha, checked_out, image, audit_exit)
    if not isinstance(lockbytes, bytes) or not lockbytes:
        raise ValueError("Missing build dependency lock")
    return {"schema": 1, "source_sha": sha, "os_image": image,
            "npm_lock_sha256": hashlib.sha256(lockbytes).hexdigest(),
            "runtime_and_build_chain_audited": True,
            "advisory_totals": old["advisory_totals"],
            "unreviewed_high_or_critical": 0,
            "dependency_exceptions": [],  # Never invent a reviewer-approved waiver.
            "raw_advisories_exported": False, "production_release_approved": False}


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
        if not 0 < args.audit.stat().st_size <= 4 * 1024 * 1024:
            raise ValueError("Missing/oversized private npm audit")
        actual = subprocess.check_output(["git", "-C", str(args.root), "rev-parse", "HEAD"],
                                         timeout=10).decode().strip()
        report = evaluate(json.loads(args.audit.read_text(encoding="utf8")),
                          args.sha, actual, args.os_image, args.audit_exit,
                          (args.root / "webadmin/package-lock.json").read_bytes())
        with args.output.open("x", encoding="utf8") as output:
            json.dump(report, output, sort_keys=True)
            output.write("\n")
    except (OSError, ValueError, TypeError, KeyError, subprocess.SubprocessError):
        parser.exit(1, "Full npm build-chain advisory gate refused; raw details suppressed.\n")
    print("Runtime and build-chain npm advisories reviewed at exact head; no release authorization")


if __name__ == "__main__":
    raise SystemExit(main())
