#!/usr/bin/env python3
"""Offline numerical review of operator-supplied, isolated target measurements.

This is not a hardware attestation, capacity certification or release approval.
The threshold policy must be approved *before* the actual pilot measurements.
"""

import argparse
from datetime import datetime, timezone
import json
import math
from pathlib import Path
import re


PROFILES = ("adc", "adcs", "webadmin", "api")


def valid_number(value, minimum=0):
    return type(value) in (int, float) and minimum <= value < float("inf")


def utc_time(value):
    if not isinstance(value, str) or not value.endswith("Z"):
        raise ValueError("A UTC timestamp ending in Z is required")
    parsed = datetime.fromisoformat(value.replace("Z", "+00:00"))
    if parsed.tzinfo != timezone.utc:
        raise ValueError("UTC timestamp required")
    return parsed


def evaluate(policy, evidence):
    if policy.get("schema") != 1 or evidence.get("schema") != 1:
        raise ValueError("Unsupported evidence or policy schema")
    target = evidence.get("target", {})
    expected = {"os": "Debian", "release": "13", "cpu": "Intel Xeon D-1521",
                "logical_threads": 8, "memory_mib": 31997, "isolated": True, "systemd": True}
    if any(type(target.get(k)) is not type(v) or target.get(k) != v for k, v in expected.items()):
        raise ValueError("Not the documented Debian 13 Xeon D-1521 target host")
    if not re.fullmatch(r"[0-9a-f]{40}", str(target.get("commit_sha", ""))):
        raise ValueError("Missing exact build commit SHA")
    approved = utc_time(policy.get("approved_before_utc"))
    measured = utc_time(evidence.get("measurement_started_utc"))
    if approved >= measured:
        raise ValueError("Acceptance limits must predate the measurement")
    limits = policy.get("profiles")
    profiles = evidence.get("profiles")
    if not isinstance(limits, dict) or set(limits) != set(PROFILES) or (
        not isinstance(profiles, dict) or set(profiles) != set(PROFILES)
    ):
        raise ValueError("All four profiles and policies are mandatory")
    failures = []
    for profile in PROFILES:
        report, limit = profiles[profile], limits[profile]
        if not isinstance(report, dict) or not isinstance(limit, dict):
            raise ValueError("Invalid report or limits")
        for key in ("requests", "success", "failure", "p50_ms", "p95_ms", "p99_ms",
                    "error_rate", "successful_requests_per_second", "elapsed_seconds"):
            if not valid_number(report.get(key)):
                raise ValueError(f"Missing or invalid {profile}.{key}")
        if (report.get("schema") != 1 or report.get("profile") != profile or
            report.get("target") != "127.0.0.1 only" or report.get("capacity_claim") is not False or
            type(report["requests"]) is not int or not 1 <= report["requests"] <= 1000 or
            type(report["success"]) is not int or not 1 <= report["success"] <= report["requests"] or
            type(report["failure"]) is not int or report["failure"] < 0 or
            report["requests"] != report["success"] + report["failure"] or
            report["elapsed_seconds"] <= 0 or
            abs(report["error_rate"] - report["failure"] / report["requests"]) > 0.000001 or
            # The probe rounds elapsed time and successful RPS to 3 decimals.
            # A caller cannot supply a fabricated throughput to satisfy policy.
            not math.isclose(report["successful_requests_per_second"],
                             report["success"] / report["elapsed_seconds"],
                             rel_tol=0.01, abs_tol=0.005) or
            not report["p50_ms"] <= report["p95_ms"] <= report["p99_ms"]):
            raise ValueError(f"Inconsistent {profile} probe result")
        for key in ("max_p95_ms", "max_p99_ms", "max_error_rate", "min_success_rps"):
            if not valid_number(limit.get(key)):
                raise ValueError(f"Missing preapproved {profile}.{key} limit")
        if (report["p95_ms"] > limit["max_p95_ms"] or
            report["p99_ms"] > limit["max_p99_ms"] or
            report["error_rate"] > limit["max_error_rate"] or
            report["successful_requests_per_second"] < limit["min_success_rps"]):
            failures.append(profile)
    resources = evidence.get("resources", {})
    for key in ("cpu_peak_percent", "rss_peak_kib", "mariadb_peak_threads", "mariadb_errors"):
        if not valid_number(resources.get(key)):
            raise ValueError(f"Missing observed resource metric: {key}")
    for key in ("max_cpu_percent", "max_rss_kib", "max_mariadb_threads", "max_mariadb_errors"):
        if not valid_number(policy.get(key)):
            raise ValueError(f"Missing preapproved resource threshold: {key}")
    for observed, threshold in (("cpu_peak_percent", "max_cpu_percent"),
                                ("rss_peak_kib", "max_rss_kib"),
                                ("mariadb_peak_threads", "max_mariadb_threads"),
                                ("mariadb_errors", "max_mariadb_errors")):
        if resources[observed] > policy[threshold]:
            failures.append(observed)
    return {"schema": 1, "commit_sha": target["commit_sha"],
            "numerical_thresholds_met": not failures, "failed_checks": failures,
            "capacity_claim": False, "release_approved": False,
            "requires_independent_operator_review": True}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--policy", type=Path, required=True)
    parser.add_argument("--evidence", type=Path, required=True)
    args = parser.parse_args()
    result = evaluate(json.loads(args.policy.read_text(encoding="utf-8")),
                      json.loads(args.evidence.read_text(encoding="utf-8")))
    print(json.dumps(result, sort_keys=True))
    return 0 if result["numerical_thresholds_met"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
