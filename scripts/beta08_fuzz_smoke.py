#!/usr/bin/env python3
"""Run a bounded hosted Clang/libFuzzer ADC parser smoke without raw logs."""

import argparse
import json
import os
from pathlib import Path
import platform
import re
import resource
import shutil
import subprocess
import tempfile
import time


RUNS = 1024
SEED = 424242
MAX_SECONDS = 180
MAX_LOG_BYTES = 1024 * 1024
SHA = re.compile(r"[0-9a-f]{40}\Z")


def classify(returncode, stderr, elapsed, requested=RUNS):
    match = re.findall(rb"#\s*(\d+)\s+DONE\b", stderr)
    if (returncode != 0 or not 0 < elapsed <= MAX_SECONDS or not match or
            int(match[-1]) < requested or
            re.search(rb"(ERROR: (?:AddressSanitizer|libFuzzer)|SUMMARY:.*Sanitizer|==ABORTING==)",
                      stderr)):
        raise ValueError("Fuzzer incomplete, timed out or sanitizer finding")
    return int(match[-1])


def _limit_child_log():
    # libFuzzer/ASan may emit arbitrary text on a crash. Never let a stuck
    # parser fill CI disk or hold a full diagnostic trace in process memory.
    resource.setrlimit(resource.RLIMIT_FSIZE, (MAX_LOG_BYTES, MAX_LOG_BYTES))


def run_bounded(binary, corpus):
    seeds = sorted(corpus.iterdir())
    if (not 1 <= len(seeds) <= 16 or
            any(seed.is_symlink() or not seed.is_file() or
                not 0 < seed.stat().st_size <= 4096 for seed in seeds)):
        raise ValueError("Unreviewed, malformed or oversized fuzzer seed corpus")
    with tempfile.TemporaryDirectory(prefix="beta08-fuzz-private-") as private:
        private_root = Path(private)
        private_corpus = private_root / "corpus"
        private_corpus.mkdir()
        for seed in seeds:
            shutil.copyfile(seed, private_corpus / seed.name)
        with tempfile.TemporaryFile(mode="w+b") as private_log:
            started = time.monotonic()
            result = subprocess.run(
                [str(binary), f"-runs={RUNS}", f"-seed={SEED}", "-max_len=4096",
                 "-timeout=5", "-rss_limit_mb=512",
                 f"-artifact_prefix={private_root}/", str(private_corpus)],
                cwd=private_root, stdout=subprocess.DEVNULL, stderr=private_log,
                timeout=MAX_SECONDS, check=False, preexec_fn=_limit_child_log)
            elapsed = time.monotonic() - started
            private_log.seek(0, os.SEEK_END)
            if private_log.tell() >= MAX_LOG_BYTES:
                raise ValueError("Unbounded fuzzer diagnostic output")
            private_log.seek(0)
            observed = classify(result.returncode, private_log.read(MAX_LOG_BYTES), elapsed)
    return observed, elapsed


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
        observed, elapsed = run_bounded(args.binary, args.corpus)
        report = {"schema": 1, "source_sha": args.sha, "os_image": "ubuntu-24.04",
                  "compiler": "clang-libfuzzer", "seed": SEED, "requested_runs": RUNS,
                  "observed_runs": observed, "input_max_bytes": 4096,
                  "rss_limit_mib": 512, "timeout_per_input_seconds": 5,
                  "elapsed_seconds": round(elapsed, 3), "sanitizer_findings": 0,
                  "bounded_diagnostic_bytes": MAX_LOG_BYTES,
                  "ephemeral_corpus_and_crash_artifacts": True,
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
