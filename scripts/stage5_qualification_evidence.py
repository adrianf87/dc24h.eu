#!/usr/bin/env python3
"""Export Stage V test results without copying raw test transcripts."""

import argparse
from collections import Counter
import json
from pathlib import Path
import re
from xml.etree import ElementTree as ET


STATUS = {"success": "PASS", "failure": "FAIL", "skipped": "SKIP", "cancelled": "NOT TESTED"}
IDENTIFIER = re.compile(r"[a-z][a-z0-9_.-]{0,63}")
SAFE_NAME = re.compile(r"[a-zA-Z0-9][a-zA-Z0-9 _.,():'/+-]{0,179}")
UNEXPECTED_TOKEN = re.compile(r"\b(?:[a-fA-F0-9]{24,}|\d{8})\b")


def junit_cases(path):
    if not path.is_file():
        return [("report", "NOT TESTED")]
    try:
        if path.stat().st_size > 10 * 1024 * 1024:
            raise ValueError("JUnit report too large")
        raw = path.read_bytes()
        if b"<!DOCTYPE" in raw.upper():
            raise ValueError("Unexpected XML document type")
        root = ET.fromstring(raw)
        cases = list(root.iter("testcase"))
        if root.tag not in {"testsuites", "testsuite"} or not cases:
            raise ValueError("Missing JUnit cases")
    except (ET.ParseError, OSError, ValueError):
        return [("invalid JUnit report", "FAIL")]

    results = []
    for index, case in enumerate(cases, start=1):
        name = case.get("name", "")
        if not SAFE_NAME.fullmatch(name) or UNEXPECTED_TOKEN.search(name):
            name = f"case {index}"
        # CTest captures Python unittest's skip result inside its output.
        # Recognize that result without retaining or printing the output.
        transcript = case.findtext("system-out", "")
        if case.find("failure") is not None or case.find("error") is not None:
            status = "FAIL"
        elif (case.find("skipped") is not None or case.get("status") == "notrun" or
              re.search(r"(?m)^OK \(skipped=[1-9][0-9]*\)\s*$", transcript)):
            status = "SKIP"
        else:
            status = "PASS"
        results.append((name, status))
    return results


def export(output, sha, suites):
    output.mkdir(parents=True, exist_ok=True)
    xml = ET.Element("testsuites")
    log = [f"Source SHA {sha}"]
    summary = {"schema": 1, "source_sha": sha, "suites": {}}
    totals = Counter()
    for suite, results in suites.items():
        counts = Counter(status for _, status in results)
        totals.update(counts)
        summary["suites"][suite] = {status: counts[status] for status in STATUS.values()}
        element = ET.SubElement(xml, "testsuite", {
            "name": suite, "tests": str(len(results)), "failures": str(counts["FAIL"]),
            "errors": "0", "skipped": str(counts["SKIP"] + counts["NOT TESTED"]),
        })
        ET.SubElement(ET.SubElement(element, "properties"), "property", name="source_sha", value=sha)
        for name, status in results:
            case = ET.SubElement(element, "testcase", name=name, classname=suite)
            if status == "FAIL":
                ET.SubElement(case, "failure", message="Test failed. Raw transcript omitted.")
            elif status in {"NOT TESTED", "SKIP"}:
                ET.SubElement(case, "skipped", message=status)
            log.append(f"{status} {suite} / {name}")
    summary["totals"] = {status: totals[status] for status in STATUS.values()}
    ET.indent(xml)
    ET.ElementTree(xml).write(output / "qualification.xml", encoding="utf-8", xml_declaration=True)
    (output / "qualification.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    (output / "qualification.log").write_text("\n".join(log) + "\n", encoding="utf-8")
    for status, count in summary["totals"].items():
        print(f"{status} {count}")
    return bool(totals["FAIL"] or totals["NOT TESTED"] or totals["SKIP"])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sha", required=True)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--junit", action="append", default=[], metavar="SUITE=PATH")
    parser.add_argument("--check", action="append", default=[], metavar="CHECK=OUTCOME")
    args = parser.parse_args()
    if not re.fullmatch(r"[a-f0-9]{40}", args.sha):
        parser.error("The source SHA must contain 40 lowercase hex characters")
    suites = {}
    for values, kind in ((args.junit, "junit"), (args.check, "check")):
        for value in values:
            name, sep, detail = value.partition("=")
            if not sep or not detail or not IDENTIFIER.fullmatch(name) or name in suites:
                parser.error("Invalid or repeated suite/check")
            if kind == "junit":
                suites[name] = junit_cases(Path(detail))
            elif detail in STATUS:
                suites[name] = [(name, STATUS[detail])]
            else:
                parser.error("Unknown check outcome")
    if not suites:
        parser.error("At least one suite/check is required")
    return export(args.output, args.sha, suites)


if __name__ == "__main__":
    raise SystemExit(main())
