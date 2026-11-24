"""Offline negative checks for the exact-head, three-window hosted gate."""

import copy
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import beta07_ubuntu_perf_matrix as matrix


SHA = "a" * 40
OS = "ubuntu-24.04"


def data():
    adc = {"schema": 1, "source_sha": SHA, "os_image": OS, "windows": {}}
    for profile in ("adc", "adcs"):
        adc["windows"][profile] = [{
            "schema": 1, "profile": profile, "target": "127.0.0.1 only",
            "requests": 32, "success": 32, "failure": 0, "capacity_claim": False,
            "concurrency": 4, "configured_rate_per_second": 8,
            "p50_ms": 4.0, "p95_ms": 10.0, "p99_ms": 12.0,
            "elapsed_seconds": 4.0, "successful_requests_per_second": 8.0,
            "target_rss_before_kib": 1000, "target_rss_after_kib": 1050,
            "target_cpu_mean_percent_one_core": 15.0,
        } for _ in range(3)]
    https = {"schema": 1, "source_sha": SHA, "os_image": OS,
             "beta07_three_windows": []}
    for i in range(3):
        https["beta07_three_windows"].append({
            "schema": 1, "window": i + 1, "requests": 8, "success": 8, "failure": 0,
            "concurrency": 2, "rate_limit_safe_batch_spacing_ms": 2200,
            "p50_ms": 20.0, "p95_ms": 30.0, "p99_ms": 40.0,
            "elapsed_seconds": 8.0, "successful_requests_per_second": 1.0,
            "rss_before_kib": {"account_admin": 1000, "gateway": 2000, "apache": 3000},
            "rss_after_kib": {"account_admin": 1100, "gateway": 2100, "apache": 3100},
            "cpu_mean_percent_one_core": 20.0, "production_sla_claim": False,
        })
    return adc, https


class MatrixTests(unittest.TestCase):
    def test_complete_three_profile_matrix_is_bounded_and_not_an_sla(self):
        adc, https = data()
        result = matrix.evaluate(SHA, OS, adc, https)
        self.assertEqual(set(result["profiles"]), {"adc", "adcs", "authenticated_https"})
        self.assertFalse(result["production_capacity_or_sla_claim"])
        self.assertFalse(result["across_protocol_rps_directly_comparable"])
        self.assertEqual(result["profiles"]["adc"]["samples"], 96)
        self.assertEqual(result["profiles"]["authenticated_https"]["samples"], 24)

    def test_wrong_sha_partial_windows_bad_resources_and_fabricated_rps_fail(self):
        base_adc, base_https = data()
        for damage in (
            lambda a, h: a.update(source_sha="b" * 40),
            lambda a, h: a["windows"]["adcs"].pop(),
            lambda a, h: a["windows"]["adc"][0].update(p99_ms=2000),
            lambda a, h: a["windows"]["adc"][0].update(successful_requests_per_second=100),
            lambda a, h: h["beta07_three_windows"][0].update(failure=1),
            lambda a, h: h["beta07_three_windows"][0]["rss_before_kib"].update(apache=None),
            lambda a, h: h["beta07_three_windows"][0].update(rate_limit_safe_batch_spacing_ms=0),
        ):
            with self.subTest(damage=damage):
                adc, https = copy.deepcopy(base_adc), copy.deepcopy(base_https)
                damage(adc, https)
                with self.assertRaises(ValueError):
                    matrix.evaluate(SHA, OS, adc, https)


if __name__ == "__main__":
    unittest.main()
