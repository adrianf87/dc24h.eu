"""Offline threshold and provenance checks with synthetic, never-live results."""

import copy
import importlib.util
from pathlib import Path
import unittest


SCRIPT = Path(__file__).resolve().parents[1] / "scripts/stage1_perf_gate.py"
spec = importlib.util.spec_from_file_location("stage1_perf_gate", SCRIPT)
gate = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gate)


def synthetic():
    policy = {"schema": 1, "approved_before_utc": "2026-11-22T12:00:00Z",
              "max_cpu_percent": 80, "max_rss_kib": 200000, "max_mariadb_threads": 50,
              "max_mariadb_errors": 0, "profiles": {
                  name: {"max_p95_ms": 100, "max_p99_ms": 200,
                         "max_error_rate": 0, "min_success_rps": 1}
                  for name in gate.PROFILES}}
    evidence = {"schema": 1, "measurement_started_utc": "2026-11-23T12:00:00Z",
                "target": {"os": "Debian", "release": "13", "cpu": "Intel Xeon D-1521",
                           "logical_threads": 8, "memory_mib": 31997, "isolated": True,
                           "systemd": True, "commit_sha": "a" * 40},
                "resources": {"cpu_peak_percent": 50, "rss_peak_kib": 100000,
                              "mariadb_peak_threads": 8, "mariadb_errors": 0},
                "profiles": {name: {"schema": 1, "profile": name,
                    "target": "127.0.0.1 only", "capacity_claim": False,
                    "requests": 10, "success": 10, "failure": 0, "elapsed_seconds": 3,
                    "error_rate": 0, "successful_requests_per_second": 3.333,
                    "p50_ms": 10, "p95_ms": 20, "p99_ms": 30}
                    for name in gate.PROFILES}}
    return policy, evidence


class PerfAcceptanceTests(unittest.TestCase):
    def test_synthetic_thresholds_do_not_grant_release_or_capacity(self):
        policy, evidence = synthetic()
        result = gate.evaluate(policy, evidence)
        self.assertTrue(result["numerical_thresholds_met"])
        self.assertFalse(result["capacity_claim"])
        self.assertFalse(result["release_approved"])

    def test_breach_is_reported_without_approving_capacity(self):
        policy, evidence = synthetic()
        evidence["profiles"]["api"]["p99_ms"] = 300
        evidence["resources"]["mariadb_errors"] = 2
        result = gate.evaluate(policy, evidence)
        self.assertFalse(result["numerical_thresholds_met"])
        self.assertEqual(result["failed_checks"], ["api", "mariadb_errors"])

    def test_missing_metric_or_profile_fails_closed(self):
        policy, evidence = synthetic()
        scenarios = []
        for key in ("api", "adcs"):
            altered = copy.deepcopy(evidence)
            del altered["profiles"][key]
            scenarios.append((policy, altered))
        altered = copy.deepcopy(evidence)
        del altered["resources"]["cpu_peak_percent"]
        scenarios.append((policy, altered))
        altered = copy.deepcopy(evidence)
        altered["profiles"]["adc"]["requests"] = 0
        scenarios.append((policy, altered))
        for p, e in scenarios:
            with self.subTest(evidence=e), self.assertRaises(ValueError):
                gate.evaluate(p, e)

    def test_wrong_hardware_or_backdated_policy_is_rejected(self):
        policy, evidence = synthetic()
        evidence["target"]["cpu"] = "AMD EPYC"
        with self.assertRaises(ValueError):
            gate.evaluate(policy, evidence)
        evidence["target"]["cpu"] = "Intel Xeon D-1521"
        policy["approved_before_utc"] = "2026-11-23T12:00:00Z"
        with self.assertRaises(ValueError):
            gate.evaluate(policy, evidence)

    def test_disallow_invented_throughput_and_invalid_measurement_windows(self):
        policy, evidence = synthetic()
        for changes in ({"successful_requests_per_second": 1000},
                        {"elapsed_seconds": 0},
                        {"success": 0, "failure": 10, "error_rate": 1,
                         "successful_requests_per_second": 0},
                        {"success": -1, "failure": 11, "error_rate": 1.1}):
            altered = copy.deepcopy(evidence)
            altered["profiles"]["adc"].update(changes)
            with self.subTest(changes=changes), self.assertRaises(ValueError):
                gate.evaluate(policy, altered)


if __name__ == "__main__":
    unittest.main()
