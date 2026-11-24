#!/usr/bin/env python3
"""Run a bounded hosted Clang/libFuzzer ADC parser smoke without raw logs."""

import argparse
import json
import os
from pathlib import Path
import platform
import re
import subprocess
import time


RUNS = 1024
SEED = 424242
MAX_SECONDS = 180
SHA = re.compile(r"[0-9a-f]{40}\Z")


def classify(returncode, stderr, elapsed, requested=RUNS):
    match = re.findall(rb"#\s*(\d+)\s+DONE\b", stderr)
    if (returncode != 0 or not 0 < elapsed <= MAX_SECONDS or not match or
            int(match[-1]) < requested or
            re.search(rb"(ERROR: (?:AddressSanitizer|libFuzzer)|SUMMARY:.*Sanitizer|==ABORTING==)",
                      stderr)):
        raise ValueError("Fuzzer incomplete, timed out or sanitizer finding")
    return int(match[-1])


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--corpus", type=Path, required=True)
    parser.add_argument("--sha", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        version = platform.freedesktop_os_release().get("VERSION_ID")
        if (os.environ.get("GITHUB_ACTIONS") != "true" or
                os.environ.get("RUNNER_ENVIRONMENT") != "github-hosted" or
                os.environ.get("RUNNER_OS") != "Linux" or version != "24.04" or
                not SHA.fullmatch(args.sha)):
            raise ValueError("Only GitHub-hosted Ubuntu 24.04 hardening is permitted")
        actual = subprocess.check_output(["git", "-C", str(args.root), "rev-parse", "HEAD"],
                                         timeout=10).decode().strip()
        if actual != args.sha or not args.binary.is_file() or not args.corpus.is_dir():
            raise ValueError("Missing exact-head fuzzer executable or seed corpus")
        start = time.monotonic()
        result = subprocess.run([str(args.binary), f"-runs={RUNS}", f"-seed={SEED}",
                                 "-max_len=4096", "-timeout=5", "-rss_limit_mb=512",
                                 str(args.corpus)], capture_output=True, timeout=MAX_SECONDS,
                                check=False)
        elapsed = time.monotonic() - start
        observed = classify(result.returncode, result.stderr[:1024 * 1024], elapsed)
        report = {"schema": 1, "source_sha": args.sha, "os_image": "ubuntu-24.04",
                  "compiler": "clang-libfuzzer", "seed": SEED, "requested_runs": RUNS,
                  "observed_runs": observed, "input_max_bytes": 4096,
                  "rss_limit_mib": 512, "timeout_per_input_seconds": 5,
                  "elapsed_seconds": round(elapsed, 3), "sanitizer_findings": 0,
                  "all_required_fuzz_checks_passed": True,
                  "raw_traces_uploaded": False, "production_security_certified": False}
        with args.output.open("x", encoding="utf8") as output:
            json.dump(report, output, sort_keys=True)
            output.write("\n")
    except (OSError, ValueError, subprocess.SubprocessError):
        parser.exit(1, "Bounded hosted fuzz smoke refused; no raw trace exported.\n")
    print("Exact-head bounded libFuzzer smoke passed; no production certification")


if __name__ == "__main__":
    raise SystemExit(main())
