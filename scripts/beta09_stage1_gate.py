#!/usr/bin/env python3
"""Beta-0.9 Stage I: exact-head process evidence with explicit client gaps.

The prior ncdc evidence is real; Python Peer sockets are a protocol fixture,
not a *second* independent ADC client. A green report here does not imply
native DC++/AirDC++ execution or approval of live SQL account refresh.
"""

import argparse
import json
import os
from pathlib import Path
import platform
import subprocess

from beta07_client_evidence import COMMIT_SHA, PINS, collect_tests, require_hosted_runner
from beta08_stage1_gate import verify as verify_beta08

PATCH_CASES = {
    "0.9.1": ("adc.ncdc", "adcs.ncdc", "adc.capabilities.ncdc",
              "adcs.capabilities.ncdc", "adc.sup.ncdc", "adc.inf.ncdc",
              "adc.identity.ncdc", "accounts.adc_authentication.process"),
    "0.9.2": ("accounts.adc_authentication.process",),
    "0.9.3": ("adc.sch.process", "adc.res.process", "adc.ctm.process",
              "adc.rcm.process", "adc.transfererrors.ncdc", "adcs.transfererrors.ncdc"),
    "0.9.4": ("adcs.tls_security.process", "adcs.process",
              "adcs.ctm.adcs.ncdc", "adcs.rcm.adcs.ncdc"),
}


def verify(junit: Path, ncdc_report: dict, sha: str, image: str) -> dict:
    """Reject wrong-head/skipped/failed evidence without inventing a client."""
    if not isinstance(sha, str) or not COMMIT_SHA.fullmatch(sha) or image not in PINS:
        raise ValueError("Invalid exact-head SHA or Ubuntu image")
    # Do not weaken any older client identity, JUnit or external-client check.
    prior = verify_beta08(junit, ncdc_report, sha, image)
    required = set().union(*PATCH_CASES.values())
    actual = collect_tests(junit, allowed_names=required)
    for patch, tests in PATCH_CASES.items():
        missing = [name for name in tests if actual.get(name) != "passed"]
        if missing:
            raise ValueError(f"beta-{patch} missing, skipped or failed process tests: {','.join(missing)}")
    if (prior.get("result") != "passed" or
            any(v.get("state") != "not_executed" for v in prior["additional_native_clients"].values())):
        raise ValueError("An unexecuted external client was represented as successful")

    candidates = {
        "DC++": {"state": "not_executed", "reason": "native Windows binary not in hosted Ubuntu fixture"},
        "AirDC++": {"state": "not_executed", "reason": "no pinned executable process fixture"},
        "EiskaltDC++": {"state": "candidate_only", "reason": "Ubuntu daemon executable, ADC/ADCS compatibility not yet exercised by a pinned process fixture"},
    }
    return {
        "schema": 1, "source_sha": sha, "os_image": image,
        "scope": "synthetic GitHub-hosted Ubuntu with real pinned ncdc; not hardware or production certification",
        "primary_executed_client": {"name": "ncdc", "version": prior["ncdc_package_version"],
                                    "package_sha256": prior["ncdc_package_sha256"],
                                    "executable_sha256": prior["ncdc_executable_sha256"]},
        "additional_clients": candidates,
        "patches": {name: {"state": "process_cases_passed", "tests": list(names)}
                    for name, names in PATCH_CASES.items()},
        "limitations": ["independent second client not executed",
                        "restart-only account snapshot; no live SQL revocation",
                        "same-host transport cases cannot qualify external NAT reachability",
                        "client-side hostname/SNI trust; no claim of server-side SNI allowlist"],
        "stage1_complete": False,
        "result": "partial_evidence_only",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--junit", required=True, type=Path)
    parser.add_argument("--ncdc-report", required=True, type=Path)
    parser.add_argument("--sha", required=True)
    parser.add_argument("--os-image", required=True, choices=tuple(PINS))
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    require_hosted_runner(args.os_image, os.environ,
                          platform.freedesktop_os_release().get("VERSION_ID"))
    root = Path(__file__).resolve().parents[1]
    head = subprocess.check_output(["git", "-C", str(root), "rev-parse", "HEAD"],
                                   text=True, timeout=8).strip()
    if args.sha != head:
        raise ValueError("Not the checked-out source SHA")
    evidence = json.loads(args.ncdc_report.read_text(encoding="utf-8"))
    report = verify(args.junit, evidence, args.sha, args.os_image)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("x", encoding="utf-8") as file:
        json.dump(report, file, indent=2, sort_keys=True)
        file.write("\n")
    print("beta-0.9 Stage I process evidence recorded; second-client qualification still pending")


if __name__ == "__main__":
    main()
