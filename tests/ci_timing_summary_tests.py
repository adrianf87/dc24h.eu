"""Prove that CI time artifacts cannot leak names or failure payloads."""

from pathlib import Path
import tempfile
import unittest

import importlib.util


SCRIPT = Path(__file__).resolve().parents[1] / "scripts/ci_timing_summary.py"
spec = importlib.util.spec_from_file_location("ci_timing_summary", SCRIPT)
timing = importlib.util.module_from_spec(spec)
spec.loader.exec_module(timing)


class TimingSummaryTests(unittest.TestCase):
    def test_grouped_timings_exclude_test_output_and_paths(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            ctest, browser = root / "ctest.xml", root / "browser.xml"
            ctest.write_text('<testsuite><testcase name="database.process" time="0.1" />'
                '<testcase name="api.private-account-secret" time="0.4">'
                '<failure message="cookie=private">private-token</failure>'
                '</testcase></testsuite>', encoding="utf8")
            browser.write_text('<testsuites><testsuite><testcase name="user@example.test" '
                'time="0.2" /></testsuite></testsuites>', encoding="utf8")
            report = timing.summarize(ctest, browser, "ubuntu-24.04", "a" * 40)
            self.assertEqual(report["ctest"]["state"], "failed")
            self.assertEqual(report["ctest"]["groups"]["database"]["p95_test_ms"], 100)
            self.assertEqual(report["browser"]["groups"]["browser"]["p99_test_ms"], 200)
            self.assertFalse(report["capacity_claim"])
            for secret in ("private-account-secret", "private-token", "cookie=private", "user@example.test"):
                self.assertNotIn(secret, str(report))

    def test_missing_or_broken_reports_fail_closed(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.assertEqual(timing.suite_summary(root / "absent.xml", True)["state"], "not-run")
            report = root / "bad.xml"
            for contents in ("<testsuite />", "<testsuite><testcase name='x'/></testsuite>",
                             "<testsuite><testcase time='NaN'/></testsuite>"):
                report.write_text(contents, encoding="utf8")
                with self.subTest(contents=contents), self.assertRaises(ValueError):
                    timing.suite_summary(report, True)

    def test_only_known_ubuntu_labels_and_exact_sha(self):
        with tempfile.TemporaryDirectory() as directory:
            missing = Path(directory) / "none"
            for bad_os, bad_sha in (("debian-13", "a" * 40), ("ubuntu-24.04", "unknown")):
                with self.assertRaises(ValueError):
                    timing.summarize(missing, missing, bad_os, bad_sha)


if __name__ == "__main__":
    unittest.main()
