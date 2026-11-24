#!/usr/bin/env python3
"""Bind three real transport workloads to source, toolchain and raw numeric inputs.

The profiles are equivalent only *within* a transport. ADC, ADCS and private
password+TOTP HTTPS have intentionally different authorization costs.
"""

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

import beta07_ubuntu_perf_matrix as matrix


VERSION = re.compile(r"\d+\.\d+(?:\.\d+)?(?:[.-][A-Za-z0-9.]+)?\Z")


def evaluate(sha, image, adc_bytes, https_bytes, summary, compiler, node, lock_digest):
    if (not isinstance(compiler, str) or not VERSION.fullmatch(compiler) or
            not isinstance(node, str) or not VERSION.fullmatch(node.lstrip("v")) or
            not isinstance(lock_digest, str) or not re.fullmatch(r"[0-9a-f]{64}", lock_digest)):
        raise ValueError("Missing locked build/dependency identity")
    adc = json.loads(adc_bytes)
    https = json.loads(https_bytes)
    recomputed = matrix.evaluate(sha, image, adc, https)
    if not isinstance(summary, dict) or summary != recomputed:
        raise ValueError("Transport summary does not reproduce raw three-window measurements")
    return {"schema": 1, "source_sha": sha, "os_image": image,
            "build_type": "Release", "compiler_version": compiler,
            "node_version": node, "npm_lock_sha256": lock_digest,
            "adc_window_input_sha256": hashlib.sha256(adc_bytes).hexdigest(),
            "https_window_input_sha256": hashlib.sha256(https_bytes).hexdigest(),
            "benchmark_policy_sha256": hashlib.sha256(json.dumps(
                matrix.POLICY, sort_keys=True, separators=(",", ":")).encode()).hexdigest(),
            "profiles": recomputed["profiles"], "failed_requests": 0,
            "all_three_profiles_recomputed": True,
            "cross_protocol_throughput_equivalent": False,
            "production_sla_or_capacity_claim": False}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--sha", required=True)
    parser.add_argument("--os-image", required=True)
    parser.add_argument("--adc", type=Path, required=True)
    parser.add_argument("--https", type=Path, required=True)
    parser.add_argument("--summary", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(argv)
    actual = subprocess.check_output(["git", "-C", str(args.root), "rev-parse", "HEAD"],
                                     timeout=10).decode().strip()
    if actual != args.sha:
        parser.error("Wrong source head")
    compiler = subprocess.check_output(["g++", "-dumpfullversion", "-dumpversion"],
                                       timeout=10).decode().strip()
    node = subprocess.check_output(["node", "--version"], timeout=10).decode().strip()
    lock = hashlib.sha256((args.root / "webadmin/package-lock.json").read_bytes()).hexdigest()
    report = evaluate(args.sha, args.os_image, args.adc.read_bytes(), args.https.read_bytes(),
                      json.loads(args.summary.read_text()), compiler, node, lock)
    with args.output.open("x", encoding="utf8") as output:
        json.dump(report, output, sort_keys=True)
        output.write("\n")
    print("Three transport windows reproduced against locked build identity; no SLA")


if __name__ == "__main__":
    raise SystemExit(main())
