"""Offline, deterministic Stage IV endurance regression-gate contracts."""

import json
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import stage4_endurance_gate as gate


def fixture(profile):
    return {
        "schema": 1, "profile": profile, "target": "127.0.0.1 only", "capacity_claim": False,
        "requests": 32, "success": 32, "failure": 0, "error_rate": 0,
        "p50_ms": 1, "p95_ms": 2, "p99_ms": 3, "max_ms": 3,
        "elapsed_seconds": 4.267, "successful_requests_per_second": 7.5, "concurrency": 4,
        "configured_rate_per_second": 8,
        "target_rss_before_kib": 10000, "target_rss_after_kib": 10100,
        "target_cpu_mean_percent_one_core": 17.5,
    }


class EnduranceGateTests(unittest.TestCase):
    def runs(self):
        return {profile: [fixture(profile) for _ in range(3)] for profile in ("adc", "adcs")}

    def test_pinned_stage1_measurements_are_non_sensitive_and_all_images_exist(self):
        reference = json.loads(gate.REFERENCE.read_text())
        self.assertEqual(reference["workflow_run"], 45283779224)
        self.assertEqual(set(reference["profiles"]), {"ubuntu-22.04", "ubuntu-24.04", "ubuntu-26.04"})
        for os_image in reference["profiles"]:
            result = gate.compare(os_image, self.runs(), reference)
            self.assertTrue(result["ci_regression_gate_passed"])
            self.assertFalse(result["production_capacity_claim"])
            self.assertEqual(result["baseline_sha"], reference["source_sha"])

    def test_refuse_incomplete_failed_or_overbudget_measurements(self):
        runs = self.runs()
        runs["adc"][0]["failure"] = 1
        with self.assertRaisesRegex(ValueError, "failed or incomplete"):
            gate.compare("ubuntu-22.04", runs)
        runs = self.runs()
        runs["adcs"][1]["target_rss_after_kib"] = 90000
        self.assertFalse(gate.compare("ubuntu-24.04", runs)["ci_regression_gate_passed"])
        runs = self.runs()
        runs["adc"][1]["p99_ms"] = 5000
        runs["adc"][1]["max_ms"] = 5000
        self.assertFalse(gate.compare("ubuntu-26.04", runs)["ci_regression_gate_passed"])
        runs = self.runs()
        runs["adcs"][1]["successful_requests_per_second"] = 0
        with self.assertRaisesRegex(ValueError, "Inconsistent endurance"):
            gate.compare("ubuntu-22.04", runs)
        runs = self.runs()
        runs["adc"].pop()
        with self.assertRaisesRegex(ValueError, "Incomplete endurance"):
            gate.compare("ubuntu-22.04", runs)

    def test_fabricated_throughput_missing_cpu_or_nonpositive_windows_fail_closed(self):
        for update in ({"successful_requests_per_second": 1000},
                       {"elapsed_seconds": 0},
                       {"target_cpu_mean_percent_one_core": None},
                       {"target_rss_before_kib": 0},
                       {"schema": True},
                       {"max_ms": 1}):
            runs = self.runs()
            runs["adc"][0].update(update)
            with self.subTest(update=update), self.assertRaises(ValueError):
                gate.compare("ubuntu-22.04", runs)

    def test_rss_growth_checks_every_window_before_and_after(self):
        runs = self.runs()
        # Peak 42 MiB is within 32 MiB of the first window's 10 MiB,
        # but not within 32 MiB of the second window's 8 MiB baseline.
        runs["adc"][1]["target_rss_before_kib"] = 8000
        runs["adc"][1]["target_rss_after_kib"] = 42000
        self.assertFalse(gate.compare("ubuntu-24.04", runs)["ci_regression_gate_passed"])

    def test_baseline_provenance_and_numbers_cannot_be_fabricated(self):
        reference = json.loads(gate.REFERENCE.read_text())
        reference["workflow_run"] += 1
        with self.assertRaisesRegex(ValueError, "provenance"):
            gate.compare("ubuntu-24.04", self.runs(), reference)
        reference = json.loads(gate.REFERENCE.read_text())
        reference["profiles"]["ubuntu-24.04"]["adc"]["rps"] = float("nan")
        with self.assertRaisesRegex(ValueError, "Stage I reference"):
            gate.compare("ubuntu-24.04", self.runs(), reference)

    def test_release_and_recovery_artifacts_require_a_real_commit_sha(self):
        for invalid in (None, "", "0" * 40, "a" * 39, "A" * 40, "unknown"):
            with self.subTest(invalid=invalid), self.assertRaisesRegex(ValueError, "source commit SHA"):
                gate.required_source_sha(invalid)
        self.assertEqual(gate.required_source_sha("a" * 40), "a" * 40)


if __name__ == "__main__":
    unittest.main()
