#!/usr/bin/env python3
"""Fail-closed offline CodeQL SARIF gate, compatible with default setup.

Only de-identified totals are printed. GitHub's configured default setup owns
the Code Scanning upload; this workflow verifies the exact PR head locally.
"""

import argparse
import json
from pathlib import Path


EXPECTED_LANGUAGES = {"cpp", "javascript-typescript"}
# CodeQL writes javascript.sarif for its combined JavaScript/TypeScript
# extractor, even when init uses the language ID javascript-typescript.
SARIF_LANGUAGE_BY_FILENAME = {
    "cpp.sarif": "cpp",
    "cpp.sarif.json": "cpp",
    "javascript.sarif": "javascript-typescript",
    "javascript.sarif.json": "javascript-typescript",
    "javascript-typescript.sarif": "javascript-typescript",
    "javascript-typescript.sarif.json": "javascript-typescript",
}


def evaluate(paths):
    files = list(paths)
    if not files:
        raise ValueError("Missing CodeQL SARIF results")
    languages = set()
    total = 0
    high = 0
    errors = 0
    for path in files:
        doc = json.loads(path.read_text(encoding="utf8"))
        if doc.get("version") != "2.1.0" or not isinstance(doc.get("runs"), list) or not doc["runs"]:
            raise ValueError("Malformed CodeQL SARIF")
        for run in doc["runs"]:
            driver = run.get("tool", {}).get("driver", {})
            if driver.get("name") != "CodeQL":
                raise ValueError("Unexpected SARIF analyzer")
            properties = run.get("properties") or {}
            language = properties.get("semmle.formatSpecifier") or properties.get("language")
            if language in EXPECTED_LANGUAGES:
                languages.add(language)
            by_filename = SARIF_LANGUAGE_BY_FILENAME.get(path.name.lower())
            if by_filename:
                languages.add(by_filename)
            rules = {rule["id"]: rule for rule in driver.get("rules", []) if isinstance(rule, dict) and "id" in rule}
            for result in run.get("results", []):
                total += 1
                level = result.get("level", rules.get(result.get("ruleId"), {}).get("defaultConfiguration", {}).get("level"))
                if level == "error":
                    errors += 1
                rule = rules.get(result.get("ruleId"), {})
                severity = result.get("properties", {}).get("security-severity", rule.get("properties", {}).get("security-severity"))
                if severity is not None and float(severity) >= 7:
                    high += 1
    if languages != EXPECTED_LANGUAGES:
        raise ValueError("Incomplete C++ and TypeScript CodeQL languages")
    if high or errors:
        raise ValueError(f"CodeQL blocking findings: high_security={high}, errors={errors}")
    return {"sarif_files": len(files), "languages": sorted(languages), "findings_total": total,
            "high_security": high, "errors": errors}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    args = parser.parse_args(argv)
    result = evaluate(sorted(args.input.rglob("*.sarif")))
    print(json.dumps(result, sort_keys=True))


if __name__ == "__main__":
    main()
