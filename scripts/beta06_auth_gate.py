"""Fail closed on fabricated or non-comparable real authenticated API evidence.

Ubuntu-hosted measurements use a fixed 24-request / 2-concurrent workload.
This is an explicit CI regression envelope, not a production throughput SLA.
"""

import argparse
import json
import math
from pathlib import Path

from beta06_ci_provenance import IMAGES, SHA

WORKLOAD = {"requests": 24, "concurrency": 2, "max_ci_p99_ms": 2000,
            "request_spacing_ms": 1100}
PROCESSES = {"account_admin", "gateway", "apache"}


def validate(report, *, sha, image):
    if not isinstance(sha, str) or not SHA.fullmatch(sha) or sha == "0" * 40:
        raise ValueError("Invalid source SHA")
    if image not in IMAGES or not isinstance(report, dict) or (
        report.get("schema") != 1 or report.get("source_sha") != sha or
        report.get("os_image") != image or
        report.get("evidence") != "real-account-admin-node-apache-authenticated-totp" or
        report.get("maintenance_mode") is not True or
        report.get("hub_readonly_boundary_verified") is not True or
        report.get("synthetic_credentials_only") is not True or
        report.get("passkey_browser_flow_exercised") is not False or
        report.get("capacity_claim") is not False or
        report.get("production_sla_claim") is not False):
        raise ValueError("Unverifiable real-authenticated workload provenance")
    if any(type(report.get(key)) is not int or report[key] != value for key, value in
           {**WORKLOAD, "success": 24, "failure": 0}.items()):
        raise ValueError("Incomplete or changed authenticated workload")
    numbers = ("elapsed_seconds", "successful_requests_per_second", "p50_ms",
               "p95_ms", "p99_ms", "max_ms", "cpu_mean_percent_one_core")
    if any(type(report.get(key)) not in (int, float) or
           not math.isfinite(report[key]) or report[key] < 0 for key in numbers):
        raise ValueError("Missing or fabricated timing/CPU metrics")
    if (report["elapsed_seconds"] <= 0 or report["successful_requests_per_second"] <= 0 or
        not report["p50_ms"] <= report["p95_ms"] <= report["p99_ms"] <= report["max_ms"] or
        not math.isclose(report["successful_requests_per_second"],
                         WORKLOAD["requests"] / report["elapsed_seconds"],
                         rel_tol=0.01, abs_tol=0.005)):
        raise ValueError("Inconsistent successful latency/throughput evidence")
    for kind in ("rss_before_kib", "rss_after_kib"):
        values = report.get(kind)
        if not isinstance(values, dict) or set(values) != PROCESSES or (
            any(type(v) is not int or v <= 0 for v in values.values())):
            raise ValueError("Incomplete or fabricated process RSS evidence")
    if report["p99_ms"] > WORKLOAD["max_ci_p99_ms"]:
        raise ValueError("Real authenticated API p99 regression")
    # Fixed workload is comparable between images and repeated CI runs; do not
    # equate it with beta-0.5's mock-backend 48-request throughput evidence.
    return {"schema": 1, "os_image": image, "source_sha": sha,
            "workload": WORKLOAD.copy(), "ci_regression_gate_passed": True,
            "baseline_kind": "beta06-equivalent-real-auth-workload",
            "production_capacity_claim": False}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--sha", required=True)
    parser.add_argument("--os-image", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if not args.report.is_file() or args.report.stat().st_size > 8192:
        parser.error("Missing/oversized authenticated report")
    try:
        result = validate(json.loads(args.report.read_text()),
                          sha=args.sha, image=args.os_image)
    except (OSError, ValueError, TypeError) as error:
        parser.error("Authenticated CI gate failed: " + str(error))
    with args.output.open("x", encoding="utf-8") as output:
        json.dump(result, output, indent=2, sort_keys=True)
        output.write("\n")
    print("Validated equivalent real authenticated HTTPS workload")


if __name__ == "__main__":
    main()
