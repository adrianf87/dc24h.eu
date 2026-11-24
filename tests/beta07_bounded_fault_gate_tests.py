"""Synthetic fail-closed JUnit fault evidence tests without network listeners."""

from pathlib import Path
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import beta07_bounded_fault_gate as gate


SHA = "a" * 40
OS = "ubuntu-24.04"


def make_xml(path, missing=None, failed=None, duration="3.14"):
    root = ET.Element("testsuite")
    for name in gate.REQUIRED:
        if name == missing:
            continue
        item = ET.SubElement(root, "testcase", name=name, time=duration)
        if name == failed:
            ET.SubElement(item, "skipped")
    ET.ElementTree(root).write(path)


class BoundedFaultGateTests(unittest.TestCase):
    def test_accepts_exact_hosted_cases_without_raw_logs(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "results.xml"
            make_xml(path)
            report = gate.evaluate(path, SHA, OS)
            self.assertEqual(len(report["required_cases"]), 5)
            self.assertTrue(report["all_required_fault_cases_passed"])
            self.assertFalse(report["production_sla_claim"])

    def test_missing_skipped_wrong_image_or_unbounded_fault_refuse(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "results.xml"
            make_xml(path, missing=gate.REQUIRED[0])
            with self.assertRaises(ValueError):
                gate.evaluate(path, SHA, OS)
            make_xml(path, failed=gate.REQUIRED[2])
            with self.assertRaises(ValueError):
                gate.evaluate(path, SHA, OS)
            make_xml(path, duration="241")
            with self.assertRaises(ValueError):
                gate.evaluate(path, SHA, OS)
            make_xml(path)
            with self.assertRaises(ValueError):
                gate.evaluate(path, SHA, "debian-13")


if __name__ == "__main__":
    unittest.main()
