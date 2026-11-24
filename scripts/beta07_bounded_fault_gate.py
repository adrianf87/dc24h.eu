#!/usr/bin/env python3
"""Qualify disposable drain, slow-peer, TLS and retry tests from CTest JUnit.

Only fixed testcase identities, durations and exact-run provenance are retained;
never copy child process output, addresses or credential material into artifacts.
"""

import argparse
import json
import math
from pathlib import Path
import re
import xml.etree.ElementTree as ET


REQUIRED = (
    "adc.login.process", "adc.close.process", "transfers.protocol.process",
    "adcs.tls_security.process", "systemd.sandbox",
)
MAX_CASE_SECONDS = 240.0
SHA = re.compile(r"[0-9a-f]{40}\Z")
IMAGES = {"ubuntu-22.04", "ubuntu-24.04", "ubuntu-26.04"}


def evaluate(junit, source_sha, os_image):
    if not isinstance(source_sha, str) or not SHA.fullmatch(source_sha) or os_image not in IMAGES:
        raise ValueError("Wrong-head or unapproved runner")
    root = ET.parse(junit).getroot()
    tests = root.findall(".//testcase")
    names = [item.get("name") for item in tests]
    if len(names) != len(set(names)) or not set(REQUIRED).issubset(set(names)):
        raise ValueError("Missing or duplicate required fault test")
    durations = {}
    for case in tests:
        if case.get("name") not in REQUIRED:
            continue
        if any(case.find(tag) is not None for tag in ("failure", "error", "skipped")):
            raise ValueError("Required bounded fault test did not pass")
        try:
            duration = float(case.get("time", "nan"))
        except ValueError:
            raise ValueError("Missing bounded fault duration") from None
        if not math.isfinite(duration) or not 0 <= duration <= MAX_CASE_SECONDS:
            raise ValueError("Fault test exceeded predeclared terminal bound")
        durations[case.get("name")] = round(duration, 3)
    if set(durations) != set(REQUIRED):
        raise ValueError("Incomplete fault duration evidence")
    return {"schema": 1, "source_sha": source_sha, "os_image": os_image,
            "required_cases": list(REQUIRED), "case_seconds": durations,
            "case_terminal_bound_seconds": MAX_CASE_SECONDS,
            "all_required_fault_cases_passed": True,
            "read_only_aggregate_evidence": True, "production_sla_claim": False}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--junit", type=Path, required=True)
    parser.add_argument("--sha", required=True)
    parser.add_argument("--os-image", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(argv)
    report = evaluate(args.junit, args.sha, args.os_image)
    with args.output.open("x", encoding="utf8") as output:
        json.dump(report, output, sort_keys=True); output.write("\n")
    print("Bounded disposable ADC/ADCS/TLS/drain fault evidence qualified; no SLA")


if __name__ == "__main__":
    raise SystemExit(main())
