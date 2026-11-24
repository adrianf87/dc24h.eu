"""Negative same-workload performance and process provenance regressions."""

import copy
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import beta06_auth_gate as gate

SHA = "b" * 40


def report():
    return {"schema": 1, "source_sha": SHA, "os_image": "ubuntu-24.04",
            "evidence": "real-account-admin-node-apache-authenticated-totp",
            "maintenance_mode": True, "hub_readonly_boundary_verified": True,
            "requests": 24, "success": 24, "failure": 0, "concurrency": 2,
            "elapsed_seconds": 3.2, "successful_requests_per_second": 7.5,
            "p50_ms": 10, "p95_ms": 25, "p99_ms": 35, "max_ms": 35,
            "max_ci_p99_ms": 2000,
            "rss_before_kib": {"account_admin": 9000, "gateway": 20000, "apache": 6000},
            "rss_after_kib": {"account_admin": 9001, "gateway": 20200, "apache": 6000},
            "cpu_mean_percent_one_core": 12.5, "synthetic_credentials_only": True,
            "passkey_browser_flow_exercised": False,
            "capacity_claim": False, "production_sla_claim": False}


class RealAuthGateTests(unittest.TestCase):
    def test_equal_workload_accepts_honest_observations(self):
        result = gate.validate(report(), sha=SHA, image="ubuntu-24.04")
        self.assertTrue(result["ci_regression_gate_passed"])
        self.assertFalse(result["production_capacity_claim"])

    def test_refuses_wrong_head_image_or_forged_fixture(self):
        cases = ({"source_sha": "c" * 40}, {"os_image": "ubuntu-22.04"},
                 {"evidence": "synthetic-upstream-only"}, {"capacity_claim": True},
                 {"passkey_browser_flow_exercised": True})
        for changes in cases:
            item = report(); item.update(changes)
            with self.subTest(changes=changes), self.assertRaises(ValueError):
                gate.validate(item, sha=SHA, image="ubuntu-24.04")

    def test_refuses_missing_faked_or_mismatched_measurements(self):
        cases = ({"requests": 12}, {"concurrency": 4}, {"success": 23},
                 {"failure": 1}, {"successful_requests_per_second": 700},
                 {"p50_ms": 90}, {"p99_ms": 2100}, {"p99_ms": float("nan")},
                 {"cpu_mean_percent_one_core": None},
                 {"rss_after_kib": {"account_admin": 4, "gateway": 5}},
                 {"rss_before_kib": {"account_admin": 0, "gateway": 1, "apache": 1}},
                 {"maintenance_mode": False}, {"hub_readonly_boundary_verified": False})
        for changes in cases:
            item = copy.deepcopy(report()); item.update(changes)
            with self.subTest(changes=changes), self.assertRaises(ValueError):
                gate.validate(item, sha=SHA, image="ubuntu-24.04")


if __name__ == "__main__":
    unittest.main()
