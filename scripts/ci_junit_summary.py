#!/usr/bin/env python3
"""Emit a minimal, non-sensitive CI artifact from CTest JUnit output.

Never upload raw test output: a failing integration fixture can accidentally
include a credential or a private API response in its failure text.
"""

import argparse
import json
from pathlib import Path
import re
import xml.etree.ElementTree as ET


def summary(path):
    if not path.is_file():
        return {"state": "not-run", "tests": 0, "failures": 0, "skipped": 0}
    root = ET.parse(path).getroot()
    if root.tag not in ("testsuite", "testsuites"):
        raise ValueError("Unexpected CTest JUnit root")
    cases = list(root.iter("testcase"))
    if not cases:
        raise ValueError("CTest JUnit has no test cases")
    failed = sum(any(item.tag in ("failure", "error") for item in case) for case in cases)
    skipped = sum(any(item.tag == "skipped" for item in case) for case in cases)
    return {"state": "failed" if failed else "passed", "tests": len(cases),
            "failures": failed, "skipped": skipped}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--junit", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--suite", required=True)
    parser.add_argument("--sha", required=True)
    args = parser.parse_args()
    if not re.fullmatch(r"[a-zA-Z0-9_.-]{1,80}", args.suite):
        parser.error("Invalid suite identifier")
    if not re.fullmatch(r"[0-9a-f]{40}", args.sha):
        parser.error("Invalid commit SHA")
    result = {"schema": 1, "suite": args.suite, "commit_sha": args.sha,
              **summary(args.junit)}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("x", encoding="utf-8") as output:
        json.dump(result, output, indent=2, sort_keys=True)
        output.write("\n")
    print(f"{args.suite}: {result['state']} ({result['tests']} tests, "
          f"{result['failures']} failures, {result['skipped']} skipped)")


if __name__ == "__main__":
    main()
