"""Evaluate bounded Ubuntu-only ADC/ADCS endurance against pinned Stage I smoke.

The reference is sourced from run 45283779224, commit 96c3f873. Workloads
differ (12 versus 3 x 32 requests). Comparisons show drift and broad CI guard
rails, not a throughput SLA, statistical significance or hardware rating.
"""

import json
from pathlib import Path


REFERENCE = Path(__file__).resolve().parents[1] / "tests/fixtures/stage1_ubuntu_loopback_reference.json"
WINDOWS = 3
REQUESTS_PER_WINDOW = 32


def compare(os_image, runs, reference=None):
    baseline = reference if reference is not None else json.loads(REFERENCE.read_text())
    if baseline.get("schema") != 1 or baseline.get("source_sha") != "96c3f873c2c94ea5e252b7db7e9b007b470330c2":
        raise ValueError("Unexpected Stage I baseline provenance")
    if os_image not in baseline["profiles"] or set(runs) != {"adc", "adcs"}:
        raise ValueError("Missing hosted Ubuntu image or ADC/ADCS profile")
    evidence = {}
    for profile, windows in runs.items():
        if len(windows) != WINDOWS:
            raise ValueError("Incomplete endurance windows")
        original = baseline["profiles"][os_image][profile]
        if any(not isinstance(original.get(k), (int, float)) or original[k] < 0
               for k in ("p99_ms", "rps", "rss_after_kib", "error_rate")):
            raise ValueError("Incomplete Stage I reference")
        p99, rps, rss = [], [], []
        for report in windows:
            if (report.get("profile") != profile or report.get("target") != "127.0.0.1 only" or
                report.get("capacity_claim") is not False or
                report.get("requests") != REQUESTS_PER_WINDOW or report.get("success") != REQUESTS_PER_WINDOW or
                report.get("failure") != 0 or report.get("error_rate") != 0 or
                report.get("concurrency") != 4 or report.get("configured_rate_per_second") != 8):
                raise ValueError("Unsafe, failed or incomplete endurance workload")
            for key in ("p50_ms", "p95_ms", "p99_ms", "successful_requests_per_second",
                        "target_rss_before_kib", "target_rss_after_kib"):
                value = report.get(key)
                if type(value) not in (int, float) or not 0 <= value < float("inf"):
                    raise ValueError("Missing finite endurance measurement: " + key)
            if not report["p50_ms"] <= report["p95_ms"] <= report["p99_ms"]:
                raise ValueError("Invalid percentile ordering")
            p99.append(report["p99_ms"])
            rps.append(report["successful_requests_per_second"])
            rss.append(report["target_rss_after_kib"])
        # Generous, explicit *CI regression* ceilings account for short Stage I
        # samples and runner scheduling. No capacity or stability certification.
        max_p99 = max(200.0 if profile == "adc" else 500.0, original["p99_ms"] * 10)
        min_rps = original["rps"] * 0.4
        growth = max(rss) - min(rss[0], windows[0]["target_rss_before_kib"])
        passed = max(p99) <= max_p99 and min(rps) >= min_rps and growth <= 32768
        evidence[profile] = {
            "windows": WINDOWS, "total_requests": WINDOWS * REQUESTS_PER_WINDOW,
            "concurrency": 4, "configured_rate_per_second": 8,
            "baseline_p99_ms": original["p99_ms"], "observed_max_p99_ms": max(p99),
            "baseline_rps": original["rps"], "observed_min_rps": min(rps),
            "baseline_rss_after_kib": original["rss_after_kib"],
            "observed_peak_rss_kib": max(rss), "observed_rss_growth_kib": growth,
            "errors": 0, "max_ci_p99_ms": max_p99, "min_ci_rps": round(min_rps, 3),
            "max_ci_rss_growth_kib": 32768, "ci_regression_gate_passed": passed,
        }
    return {
        "schema": 1, "evidence": "hosted-ubuntu-loopback-endurance", "os": os_image,
        "baseline_sha": baseline["source_sha"], "baseline_run": baseline["workflow_run"],
        "baseline_workload": baseline["workload"], "profiles": evidence,
        "ci_regression_gate_passed": all(e["ci_regression_gate_passed"] for e in evidence.values()),
        "production_capacity_claim": False,
        "note": "Different workload durations: Stage I 12 requests; Stage IV three 32-request windows. All 216 ADC/ADCS connections including Stage I-style smokes remain below the default per-IP 256 admissions/minute; API/WebAdmin lack comparable Stage I request-level measurements.",
    }
