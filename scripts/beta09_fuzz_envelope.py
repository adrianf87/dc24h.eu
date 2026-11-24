#!/usr/bin/env python3
"""Require two distinct, bounded ASan/libFuzzer parser targets on exact Ubuntu head."""

import argparse
import json
import math
import os
from pathlib import Path
import platform
import re
import subprocess

import beta08_fuzz_smoke as smoke


PROFILES = ("adc_parser", "adc_fragments")


def evaluate(sha, records):
    if (not isinstance(sha, str) or not re.fullmatch(r"[0-9a-f]{40}", sha) or
            not isinstance(records, dict) or set(records) != set(PROFILES)):
        raise ValueError("Wrong source or missing independent fuzz profile")
    observed_total = 0
    for name in PROFILES:
        report = records[name]
        if (not isinstance(report, dict) or report.get("schema") != 1 or
                report.get("source_sha") != sha or
                report.get("os_image") != "ubuntu-24.04" or
                report.get("fuzzer_profile") != name or
                report.get("compiler") != "clang-libfuzzer" or
                type(report.get("requested_runs")) is not int or
                report["requested_runs"] != smoke.RUNS or
                type(report.get("observed_runs")) is not int or
                report["observed_runs"] < smoke.RUNS or
                report.get("seed") != smoke.SEED or
                report.get("input_max_bytes") != 4096 or
                report.get("rss_limit_mib") != 512 or
                report.get("timeout_per_input_seconds") != 5 or
                type(report.get("elapsed_seconds")) not in (int, float) or
                not math.isfinite(report["elapsed_seconds"]) or
                not 0 < report["elapsed_seconds"] <= smoke.MAX_SECONDS or
                report.get("bounded_diagnostic_bytes") != smoke.MAX_LOG_BYTES or
                report.get("ephemeral_corpus_and_crash_artifacts") is not True or
                report.get("sanitizer_findings") != 0 or
                report.get("all_required_fuzz_checks_passed") is not True or
                report.get("raw_traces_uploaded") is not False or
                report.get("production_security_certified") is not False):
            raise ValueError("Missing, skipped, altered or unbounded Clang/ASan fuzz report")
        observed_total += report["observed_runs"]
    return {"schema": 1, "source_sha": sha, "os_image": "ubuntu-24.04",
            "distinct_fuzzer_profiles": list(PROFILES), "required_runs": smoke.RUNS * len(PROFILES),
            "observed_runs": observed_total, "sanitizer_findings": 0,
            "both_required_fuzz_checks_passed": True,
            "independent_production_security_certification": False,
            "raw_inputs_exported": False}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--sha", required=True)
    parser.add_argument("--parser-report", type=Path, required=True)
    parser.add_argument("--fragments-report", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        if (os.environ.get("GITHUB_ACTIONS") != "true" or
                os.environ.get("RUNNER_ENVIRONMENT") != "github-hosted" or
                os.environ.get("RUNNER_OS") != "Linux" or
                platform.freedesktop_os_release().get("VERSION_ID") != "24.04"):
            raise ValueError("Requires hosted Ubuntu 24.04")
        actual = subprocess.check_output(["git", "-C", str(args.root), "rev-parse", "HEAD"],
                                         timeout=10).decode().strip()
        if actual != args.sha:
            raise ValueError("Wrong checkout")
        paths = {"adc_parser": args.parser_report, "adc_fragments": args.fragments_report}
        if any(not 0 < path.stat().st_size <= 8192 for path in paths.values()):
            raise ValueError("Unbounded fuzz summary")
        records = {name: json.loads(path.read_text(encoding="utf8"))
                   for name, path in paths.items()}
        report = evaluate(args.sha, records)
        with args.output.open("x", encoding="utf8") as stream:
            json.dump(report, stream, sort_keys=True)
            stream.write("\n")
    except (OSError, ValueError, TypeError, KeyError, UnicodeError,
            subprocess.SubprocessError, json.JSONDecodeError):
        parser.exit(1, "Two-profile bounded parser fuzz evidence incomplete; NO-GO.\n")
    print("2048 combined hosted Clang/ASan fuzz runs qualified without raw crash artifacts")


if __name__ == "__main__":
    raise SystemExit(main())
