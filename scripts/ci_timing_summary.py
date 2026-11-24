#!/usr/bin/env python3
"""Summarize Ubuntu CTest/browser timings without publishing test output.

These are test-case wall-clock durations, NOT application request latencies,
target hardware measurements, benchmark throughput, or capacity acceptance.
"""

import argparse
import json
import math
from pathlib import Path
import re
import xml.etree.ElementTree as ET


GROUPS = ("database", "apache2", "api", "adc", "adcs", "systemd", "remote", "beta")


def percentile(values, fraction):
    if not values:
        return None
    sorted_values = sorted(values)
    return round(sorted_values[max(0, math.ceil(len(sorted_values) * fraction) - 1)] * 1000, 3)


def suite_summary(path, group_by_name):
    if not path.is_file():
        return {"state": "not-run", "tests": 0, "failures": 0, "skipped": 0, "groups": {}}
    if path.stat().st_size > 10 * 1024 * 1024:
        raise ValueError("JUnit evidence exceeds the safe size limit")
    root = ET.parse(path).getroot()
    if root.tag not in ("testsuite", "testsuites"):
        raise ValueError("Unexpected JUnit document")
    cases = list(root.iter("testcase"))
    if not cases:
        raise ValueError("JUnit report contains no tests")
    durations = {}
    failures = skips = 0
    for case in cases:
        seconds = float(case.attrib.get("time", "nan"))
        if not math.isfinite(seconds) or not 0 <= seconds <= 7200:
            raise ValueError("Invalid or missing test duration")
        # Only a fixed category is published, never a test name or failure text.
        name = case.attrib.get("name", "")
        prefix = name.split(".", 1)[0]
        group = prefix if group_by_name and prefix in GROUPS else ("other" if group_by_name else "browser")
        durations.setdefault(group, []).append(seconds)
        failures += int(any(child.tag in ("failure", "error") for child in case))
        skips += int(any(child.tag == "skipped" for child in case))
    groups = {group: {"count": len(values), "p50_test_ms": percentile(values, 0.5),
                      "p95_test_ms": percentile(values, 0.95),
                      "p99_test_ms": percentile(values, 0.99),
                      "total_test_seconds": round(sum(values), 3)}
              for group, values in sorted(durations.items())}
    return {"state": "failed" if failures or skips else "passed", "tests": len(cases),
            "failures": failures, "skipped": skips, "groups": groups}


def summarize(ctest, browser, os_name, sha):
    if os_name not in ("ubuntu-22.04", "ubuntu-24.04", "ubuntu-26.04"):
        raise ValueError("Only hosted Ubuntu CI environments are supported")
    if re.fullmatch(r"[0-9a-f]{40}", sha) is None:
        raise ValueError("Exact source commit SHA is required")
    return {"schema": 1, "os": os_name, "commit_sha": sha,
            "evidence": "hosted-ubuntu-test-duration-only",
            "ctest": suite_summary(ctest, True), "browser": suite_summary(browser, False),
            "capacity_claim": False, "target_hardware_accepted": False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ctest", type=Path, required=True)
    parser.add_argument("--browser", type=Path, required=True)
    parser.add_argument("--os", required=True)
    parser.add_argument("--sha", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = summarize(args.ctest, args.browser, args.os, args.sha)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("x", encoding="utf-8") as out:
        json.dump(result, out, indent=2, sort_keys=True)
        out.write("\n")
    print(f"Ubuntu CI timing summary saved: CTest {result['ctest']['state']}, "
          f"browser {result['browser']['state']}; no capacity claim")
    return 0 if result["ctest"]["state"] == result["browser"]["state"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
