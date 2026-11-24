#!/usr/bin/env python3
"""Validate three equivalent *within-profile* hosted Ubuntu sample windows.

Predeclared wide CI regression ceilings; no cross-protocol throughput ratio,
statistical significance, production capacity, or user SLA is inferred. ADC,
ADCS and password+TOTP authenticated HTTPS have different authorization costs.
"""

import argparse
import json
import math
from pathlib import Path
import re


WINDOWS = 3
# Approved before collection; robust to CPU/VM scheduling, not tuned to results.
POLICY = {
    "adc": {"requests": 32, "concurrency": 4, "p99_ceiling_ms": 500.0, "min_rps": 2.0,
            "max_cpu_percent_one_core": 400.0, "max_rss_growth_kib": 65536},
    "adcs": {"requests": 32, "concurrency": 4, "p99_ceiling_ms": 1000.0, "min_rps": 2.0,
             "max_cpu_percent_one_core": 400.0, "max_rss_growth_kib": 65536},
    "authenticated_https": {"requests": 8, "concurrency": 2, "p99_ceiling_ms": 2000.0,
                            "min_rps": 0.2, "max_cpu_percent_one_core": 400.0,
                            "max_rss_growth_kib": 131072},
}
SHA = re.compile(r"[0-9a-f]{40}\Z")


def nonnegative(value):
    return type(value) in (int, float) and math.isfinite(value) and value >= 0


def validate_profile_summary(profile, result):
    """Fail closed on the complete, bounded aggregate (including resource use)."""
    if profile not in POLICY or not isinstance(result, dict):
        raise ValueError("Unknown or missing performance profile")
    policy = POLICY[profile]
    if (type(result.get("windows")) is not int or result["windows"] != WINDOWS or
            type(result.get("samples")) is not int or
            result["samples"] != WINDOWS * policy["requests"] or
            type(result.get("failed_requests")) is not int or
            result["failed_requests"] != 0 or
            result.get("ci_regression_envelope_passed") is not True):
        raise ValueError("Missing successful performance samples")
    for key, ceiling in (("max_observed_p99_ms", policy["p99_ceiling_ms"]),
                         ("max_cpu_percent_one_core", policy["max_cpu_percent_one_core"]),
                         ("rss_growth_kib", policy["max_rss_growth_kib"])):
        if not nonnegative(result.get(key)) or result[key] > ceiling:
            raise ValueError("Nonfinite or exceeded performance/resource budget")
    if (not nonnegative(result.get("min_success_rps")) or
            result["min_success_rps"] < policy["min_rps"]):
        raise ValueError("Unacceptable or nonfinite successful throughput")


def measured_percentile(samples, fraction):
    return round(samples[math.ceil(len(samples) * fraction) - 1], 3)


def evaluate(sha, os_image, adc_data, https_data):
    if (not SHA.fullmatch(sha) or os_image not in ("ubuntu-22.04", "ubuntu-24.04", "ubuntu-26.04") or
            adc_data.get("schema") != 1 or adc_data.get("source_sha") != sha or
            adc_data.get("os_image") != os_image or https_data.get("schema") != 1 or
            https_data.get("source_sha") != sha or https_data.get("os_image") != os_image):
        raise ValueError("Wrong-head or wrong-image performance evidence")
    if set(adc_data.get("windows", {})) != {"adc", "adcs"} or (
            not isinstance(https_data.get("beta07_three_windows"), list)):
        raise ValueError("Incomplete transport profile matrix")
    profiles = {**adc_data["windows"], "authenticated_https": https_data["beta07_three_windows"]}
    approved = {}
    for profile, policy in POLICY.items():
        runs = profiles[profile]
        if not isinstance(runs, list) or len(runs) != WINDOWS:
            raise ValueError("Exactly three measurement windows are required")
        p99s, rps, cpus, rss_start, rss_end = [], [], [], [], []
        for i, row in enumerate(runs):
            if (not isinstance(row, dict) or row.get("schema") != 1 or
                    type(row.get("requests")) is not int or row["requests"] != policy["requests"] or
                    type(row.get("success")) is not int or row["success"] != policy["requests"] or
                    type(row.get("failure")) is not int or row["failure"] != 0 or
                    row.get("concurrency") != policy["concurrency"] or
                    row.get("production_sla_claim", False) is not False or
                    (profile == "authenticated_https" and
                     (row.get("window") != i + 1 or row.get("rate_limit_safe_batch_spacing_ms") != 2200)) or
                    (profile != "authenticated_https" and
                     (row.get("profile") != profile or row.get("configured_rate_per_second") != 8 or
                      row.get("capacity_claim") is not False))):
                raise ValueError("Incomparable or failed measurement window")
            for key in ("p50_ms", "p95_ms", "p99_ms", "elapsed_seconds",
                        "successful_requests_per_second"):
                if not nonnegative(row.get(key)):
                    raise ValueError("Invalid numeric latency/throughput")
            samples = row.get("latency_samples_ms")
            if (not isinstance(samples, list) or len(samples) != policy["requests"] or
                    any(not nonnegative(value) for value in samples) or
                    samples != sorted(samples) or
                    any(not math.isclose(row[key], measured_percentile(samples, fraction),
                                         rel_tol=0, abs_tol=0.005)
                        for key, fraction in (("p50_ms", 0.50), ("p95_ms", 0.95),
                                              ("p99_ms", 0.99)))):
                raise ValueError("Percentiles are not backed by matching measured latencies")
            if (not 0 < row["elapsed_seconds"] or not 0 < row["successful_requests_per_second"] or
                    not row["p50_ms"] <= row["p95_ms"] <= row["p99_ms"] or
                    row["elapsed_seconds"] * 1000 < max(samples) or
                    not math.isclose(row["successful_requests_per_second"],
                                     policy["requests"] / row["elapsed_seconds"],
                                     rel_tol=0.02, abs_tol=0.005)):
                raise ValueError("Inconsistent percentile or successful RPS")
            if profile == "authenticated_https":
                before, after = row.get("rss_before_kib"), row.get("rss_after_kib")
                if (not isinstance(before, dict) or not isinstance(after, dict) or
                        set(before) != {"account_admin", "gateway", "apache"} or set(after) != set(before)):
                    raise ValueError("Missing real C++/Node/Apache RSS observation")
                if any(not nonnegative(v) or v <= 0 for v in list(before.values()) + list(after.values())):
                    raise ValueError("Invalid C++/Node/Apache memory reading")
                rss_start.append(sum(before.values()))
                rss_end.append(sum(after.values()))
                cpu = row.get("cpu_mean_percent_one_core")
                by_process = row.get("cpu_mean_percent_one_core_by_process")
                if (not isinstance(by_process, dict) or set(by_process) != set(before) or
                        any(not nonnegative(value) for value in by_process.values()) or
                        not nonnegative(cpu) or
                        not math.isclose(cpu, sum(by_process.values()),
                                         rel_tol=0, abs_tol=0.005)):
                    raise ValueError("Missing or contradictory C++/Node/Apache CPU measurements")
            else:
                if not all(nonnegative(row.get(k)) for k in (
                        "target_rss_before_kib", "target_rss_after_kib", "target_cpu_mean_percent_one_core")):
                    raise ValueError("Missing native C++ CPU/RSS observation")
                if (row.get("target") != "127.0.0.1 only" or
                        row["target_rss_before_kib"] <= 0 or row["target_rss_after_kib"] <= 0 or
                        row.get("failure") != 0 or row.get("error_kinds", {}) != {}):
                    raise ValueError("Unsafe or incomplete native transport observation")
                rss_start.append(row["target_rss_before_kib"])
                rss_end.append(row["target_rss_after_kib"])
                cpu = row["target_cpu_mean_percent_one_core"]
            if not nonnegative(cpu):
                raise ValueError("Missing CPU observation")
            p99s.append(row["p99_ms"])
            rps.append(row["successful_requests_per_second"])
            cpus.append(cpu)
        growth = max(0, max(rss_end) - min(rss_start))
        if (max(p99s) > policy["p99_ceiling_ms"] or min(rps) < policy["min_rps"] or
                max(cpus) > policy["max_cpu_percent_one_core"] or growth > policy["max_rss_growth_kib"]):
            raise ValueError("Predeclared hosted CI regression envelope exceeded")
        approved[profile] = {"windows": WINDOWS, "samples": WINDOWS * policy["requests"],
                             "max_observed_p99_ms": max(p99s), "min_success_rps": min(rps),
                             "max_cpu_percent_one_core": max(cpus),
                             "rss_growth_kib": growth, "failed_requests": 0,
                             "ci_regression_envelope_passed": True}
    return {"schema": 1, "source_sha": sha, "os_image": os_image, "profiles": approved,
            "within_profile_windows_comparable": True,
            "across_protocol_rps_directly_comparable": False,
            "production_capacity_or_sla_claim": False}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sha", required=True)
    parser.add_argument("--os-image", required=True)
    parser.add_argument("--adc-windows", type=Path, required=True)
    parser.add_argument("--https-report", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(argv)
    report = evaluate(args.sha, args.os_image, json.loads(args.adc_windows.read_text()),
                      json.loads(args.https_report.read_text()))
    with args.output.open("x", encoding="utf8") as file:
        json.dump(report, file, sort_keys=True); file.write("\n")
    print("Three verified windows per transport; hosted regression bounds met, no SLA")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
