"""Negative gates for the beta-0.7 external-client evidence contract."""
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import beta07_client_evidence as evidence


class EvidenceTests(unittest.TestCase):
    def setUp(self):
        self.states = {name: "passed" for name in evidence.CAPABILITIES.values()}
        self.client = {"package": "ncdc", "version": evidence.PINS["ubuntu-24.04"],
                       "package_sha256": "a" * 64, "executable_sha256": "b" * 64}

    def test_real_process_matrix_keeps_other_clients_untested(self):
        report = evidence.produce("ubuntu-24.04", "1" * 40, self.client, self.states)
        self.assertEqual(report["result"], "passed")
        self.assertEqual(report["capabilities"]["ncdc"]["adcs_ctm"]["state"], "passed")
        for unsupported in ("DC++", "AirDC++"):
            self.assertEqual(report["capabilities"][unsupported]["state"], "untested")

    def test_missing_or_failed_case_refused(self):
        for condition in (None, "failed", "skipped"):
            with self.subTest(condition=condition):
                states = dict(self.states)
                if condition is None:
                    del states["adc.ncdc"]
                else:
                    states["adc.ncdc"] = condition
                with self.assertRaisesRegex(ValueError, "incomplete"):
                    evidence.produce("ubuntu-24.04", "1" * 40, self.client, states)

    def test_invalid_sha_or_unverified_digest_refused(self):
        with self.assertRaisesRegex(ValueError, "source SHA"):
            evidence.produce("ubuntu-24.04", "foo", self.client, self.states)
        altered = dict(self.client, package_sha256="unverified")
        with self.assertRaisesRegex(ValueError, "digest"):
            evidence.produce("ubuntu-24.04", "1" * 40, altered, self.states)

    def test_junit_skips_fail_and_duplicates_refused(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "junit.xml"
            path.write_text('<testsuite><testcase name="adc.ncdc" time="0.3"/>'
                            '<testcase name="adcs.ncdc" time="0.1"><skipped/></testcase>'
                            '</testsuite>', encoding="utf-8")
            states = evidence.collect_tests(path)
            self.assertEqual(states["adc.ncdc"], "passed")
            self.assertEqual(states["adcs.ncdc"], "failed")
            path.write_text('<testsuite><testcase name="adc.ncdc"/>'
                            '<testcase name="adc.ncdc"/></testsuite>', encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "Duplicate"):
                evidence.collect_tests(path)


if __name__ == "__main__":
    unittest.main()
