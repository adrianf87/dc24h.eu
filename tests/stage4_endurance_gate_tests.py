"""Offline, deterministic Stage IV endurance regression-gate contracts."""

import json
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import stage4_endurance_gate as gate


def fixture(profile):
    return {
        "profile": profile, "target": "127.0.0.1 only", "capacity_claim": False,
        "requests": 32, "success": 32, "failure": 0, "error_rate": 0,
        "p50_ms": 1, "p95_ms": 2, "p99_ms": 3,
        "successful_requests_per_second": 7.5, "concurrency": 4,
        "configured_rate_per_second": 8,
        "target_rss_before_kib": 10000, "target_rss_after_kib": 10100,
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
        self.assertFalse(gate.compare("ubuntu-26.04", runs)["ci_regression_gate_passed"])
        runs = self.runs()
        runs["adcs"][1]["successful_requests_per_second"] = 0
        self.assertFalse(gate.compare("ubuntu-22.04", runs)["ci_regression_gate_passed"])
        runs = self.runs()
        runs["adc"].pop()
        with self.assertRaisesRegex(ValueError, "Incomplete endurance"):
            gate.compare("ubuntu-22.04", runs)


if __name__ == "__main__":
    unittest.main()
