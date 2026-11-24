"""Negative evidence tests for beta-0.8.1–0.8.4, no live client claims."""
import copy
from pathlib import Path
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from beta07_client_evidence import CAPABILITIES, PINS, produce
from beta08_stage1_gate import PATCH_CASES, verify


class StageOneGateTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="beta08-stage1-")
        self.addCleanup(self.temp.cleanup)
        self.xml = Path(self.temp.name) / "ctest.xml"
        self.sha = "a" * 40
        self.os_image = "ubuntu-24.04"
        package = {"package": "ncdc", "version": PINS[self.os_image],
                   "package_sha256": "b" * 64, "executable_sha256": "c" * 64}
        states = {name: "passed" for name in CAPABILITIES.values()}
        self.previous = produce(self.os_image, self.sha, self.sha, package, states)
        self.names = set(CAPABILITIES.values())
        for cases in PATCH_CASES.values():
            self.names.update(cases)
        self.write()

    def write(self, bad=None, duplicate=None):
        root = ET.Element("testsuite")
        for name in sorted(self.names):
            attributes = {"name": name, "status": "run", "time": "0.1"}
            if bad and bad[0] == name:
                attributes.update(bad[1])
            ET.SubElement(root, "testcase", attributes)
        if duplicate:
            ET.SubElement(root, "testcase", {"name": duplicate, "time": "0.1"})
        ET.ElementTree(root).write(self.xml, encoding="unicode")

    def test_exact_head_four_patch_provenance_and_honest_client_inventory(self):
        report = verify(self.xml, self.previous, self.sha, self.os_image)
        self.assertEqual(set(report["patches"]), {"0.8.1", "0.8.2", "0.8.3", "0.8.4"})
        self.assertTrue(all(value["state"] == "passed" for value in report["patches"].values()))
        self.assertEqual(report["additional_native_clients"]["DC++"]["state"], "not_executed")
        self.assertEqual(report["additional_native_clients"]["AirDC++"]["version"], None)

    def test_missing_or_skipped_transport_and_security_case_fails(self):
        for name in ("adc.sch.process", "adcs.tls_security.process",
                     "accounts.adc_authentication.process", "adc.capabilities.ncdc"):
            with self.subTest(name=name):
                self.names.remove(name)
                try:
                    self.write()
                    with self.assertRaisesRegex(ValueError, "missing/failed|Missing or forged"):
                        verify(self.xml, self.previous, self.sha, self.os_image)
                finally:
                    self.names.add(name)
        for status in ("notrun", "skipped"):
            self.write(bad=("adc.ctm.process", {"status": status}))
            with self.assertRaisesRegex(ValueError, "missing/failed"):
                verify(self.xml, self.previous, self.sha, self.os_image)

    def test_duplicate_and_nonfinite_cases_fail(self):
        self.write(duplicate="adc.rcm.process")
        with self.assertRaisesRegex(ValueError, "Duplicate"):
            verify(self.xml, self.previous, self.sha, self.os_image)
        for time_value in ("nan", "inf", "-1", "invalid"):
            self.write(bad=("adcs.process", {"time": time_value}))
            with self.assertRaisesRegex(ValueError, "missing/failed"):
                verify(self.xml, self.previous, self.sha, self.os_image)

    def test_failure_and_skipped_nodes_cannot_be_overridden_by_run_status(self):
        for name, child in (("adc.sch.process", "failure"),
                            ("adcs.tls_security.process", "error"),
                            ("adc.ctm.process", "skipped")):
            with self.subTest(name=name, child=child):
                root = ET.Element("testsuite")
                for case_name in sorted(self.names):
                    test = ET.SubElement(root, "testcase", {
                        "name": case_name, "status": "run", "time": "0.1"})
                    if case_name == name:
                        ET.SubElement(test, child)
                ET.ElementTree(root).write(self.xml, encoding="unicode")
                with self.assertRaisesRegex(ValueError, "missing/failed"):
                    verify(self.xml, self.previous, self.sha, self.os_image)

    def test_wrong_head_digest_and_client_spoofing_refused(self):
        mutated = copy.deepcopy(self.previous)
        for key, value in (("source_sha", "d" * 40), ("os_image", "ubuntu-22.04"),
                           ("result", "failed"), ("git_checkout_head_verified", False)):
            with self.subTest(key=key):
                mutated = copy.deepcopy(self.previous)
                mutated[key] = value
                with self.assertRaises(ValueError):
                    verify(self.xml, mutated, self.sha, self.os_image)
        for client in ("DC++", "AirDC++"):
            mutated = copy.deepcopy(self.previous)
            mutated["capabilities"][client] = {"state": "passed", "version": "not-executed"}
            with self.assertRaisesRegex(ValueError, "Unexecuted"):
                verify(self.xml, mutated, self.sha, self.os_image)
        mutated = copy.deepcopy(self.previous)
        mutated["client"]["executable_sha256"] = "unverified"
        with self.assertRaisesRegex(ValueError, "hash"):
            verify(self.xml, mutated, self.sha, self.os_image)
        mutated = copy.deepcopy(self.previous)
        mutated["capabilities"]["ncdc"]["adcs_ctm"]["state"] = "skipped"
        with self.assertRaisesRegex(ValueError, "Missing or forged"):
            verify(self.xml, mutated, self.sha, self.os_image)


if __name__ == "__main__":
    unittest.main()
