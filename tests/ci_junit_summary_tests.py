"""Ensure CI artifacts never serialize failure text, test logs or credentials."""

import importlib.util
from pathlib import Path
import tempfile
import unittest


SCRIPT = Path(__file__).resolve().parents[1] / "scripts/ci_junit_summary.py"
spec = importlib.util.spec_from_file_location("ci_junit_summary", SCRIPT)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class JUnitSummaryTests(unittest.TestCase):
    def test_missing_report_is_not_passed(self):
        with tempfile.TemporaryDirectory() as directory:
            self.assertEqual(module.summary(Path(directory) / "missing.xml")["state"], "not-run")

    def test_failure_output_never_enters_summary(self):
        with tempfile.TemporaryDirectory() as directory:
            report = Path(directory) / "ctest.xml"
            report.write_text(
                '<testsuite tests="2"><testcase name="ok" />'
                '<testcase name="bad"><failure message="secret-cookie=abc">'
                'private response</failure></testcase></testsuite>', encoding="utf-8")
            result = module.summary(report)
            self.assertEqual(result, {"state": "failed", "tests": 2, "failures": 1, "skipped": 0})
            self.assertNotIn("secret-cookie", str(result))
            self.assertNotIn("private response", str(result))

    def test_empty_or_malformed_report_is_not_success(self):
        with tempfile.TemporaryDirectory() as directory:
            report = Path(directory) / "ctest.xml"
            for value in ("<testsuite />", "<notjunit />", "<testsuite"):
                report.write_text(value, encoding="utf-8")
                with self.assertRaises(Exception):
                    module.summary(report)


if __name__ == "__main__":
    unittest.main()
