"""Fail-closed exact-head beta-0.9 Stage I evidence and honest client gaps."""

import copy
from pathlib import Path
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from beta07_client_evidence import CAPABILITIES, PINS, produce
from beta08_stage1_gate import PATCH_CASES as OLD_CASES
from beta09_stage1_gate import PATCH_CASES, verify


class StageOneEvidenceTests(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory(prefix="beta09-stage1-")
        self.addCleanup(temp.cleanup)
        self.junit = Path(temp.name) / "ctest.xml"
        self.sha, self.image = "c" * 40, "ubuntu-24.04"
        package = {"package": "ncdc", "version": PINS[self.image],
                   "package_sha256": "a" * 64, "executable_sha256": "b" * 64}
        self.report = produce(self.image, self.sha, self.sha, package,
                              {test: "passed" for test in CAPABILITIES.values()})
        self.names = set(CAPABILITIES.values())
        self.names.update(*OLD_CASES.values())
        self.names.update(*PATCH_CASES.values())
        self.write_junit()

    def write_junit(self, *, absent=None, altered=None, duplicate=None):
        root = ET.Element("testsuite")
        for name in sorted(self.names - ({absent} if absent else set())):
            testcase = ET.SubElement(root, "testcase", {
                "name": name, "status": "run", "time": "0.12"})
            if altered and name == altered[0]:
                if altered[1] in ("failure", "error", "skipped"):
                    ET.SubElement(testcase, altered[1])
                else:
                    testcase.set("time", altered[1])
        if duplicate:
            ET.SubElement(root, "testcase", {"name": duplicate, "status": "run", "time": "0.1"})
        ET.ElementTree(root).write(self.junit, encoding="unicode")

    def test_all_process_suites_pass_but_second_client_remains_unqualified(self):
        result = verify(self.junit, self.report, self.sha, self.image)
        self.assertEqual(set(result["patches"]), set(PATCH_CASES))
        self.assertFalse(result["stage1_complete"])
        self.assertEqual(result["result"], "partial_evidence_only")
        self.assertEqual(result["additional_clients"]["EiskaltDC++"]["state"], "candidate_only")
        self.assertTrue(all(c["state"] != "passed" for c in result["additional_clients"].values()))

    def test_required_process_cases_cannot_be_skipped_missing_or_failing(self):
        for name in ("adc.res.process", "adc.rcm.process", "adcs.tls_security.process"):
            with self.subTest(name=name):
                self.write_junit(absent=name)
                with self.assertRaisesRegex(ValueError, "missing, skipped or failed|missing/failed"):
                    verify(self.junit, self.report, self.sha, self.image)
        for node in ("failure", "error", "skipped", "nan", "inf", "-1"):
            with self.subTest(node=node):
                self.write_junit(altered=("adc.res.process", node))
                with self.assertRaisesRegex(ValueError, "missing, skipped or failed"):
                    verify(self.junit, self.report, self.sha, self.image)

    def test_wrong_head_and_forged_external_client_are_refused(self):
        with self.assertRaises(ValueError):
            verify(self.junit, self.report, "d" * 40, self.image)
        forged = copy.deepcopy(self.report)
        forged["capabilities"]["AirDC++"] = {"state": "passed", "version": "3.0"}
        with self.assertRaises(ValueError):
            verify(self.junit, forged, self.sha, self.image)
        forged = copy.deepcopy(self.report)
        forged["client"]["package_sha256"] = "unauthenticated"
        with self.assertRaises(ValueError):
            verify(self.junit, forged, self.sha, self.image)

    def test_duplicate_junit_case_is_not_a_second_executed_client(self):
        self.write_junit(duplicate="adc.res.process")
        with self.assertRaisesRegex(ValueError, "Duplicate"):
            verify(self.junit, self.report, self.sha, self.image)


if __name__ == "__main__":
    unittest.main()
