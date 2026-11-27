"""Synthetic, offline negative coverage of redacted hosted npm advisory gate."""

import copy
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import beta07_npm_audit_gate as gate


SHA = "a" * 40
OS = "debian-13"


def example():
    return {"auditReportVersion": 2, "metadata": {"vulnerabilities": {
        "info": 0, "low": 0, "moderate": 1, "high": 0, "critical": 0, "total": 1}},
        "vulnerabilities": {"synthetic-pkg": {"severity": "moderate", "name": "private-do-not-log"}}}


class NpmAuditGateTests(unittest.TestCase):
    def test_redacted_counts_allow_moderate_only_with_verified_head(self):
        result = gate.evaluate(example(), SHA, SHA, OS, 0)
        self.assertEqual(result["advisory_totals"]["moderate"], 1)
        self.assertFalse(result["raw_advisories_exported"])
        self.assertNotIn("private-do-not-log", str(result))

    def test_high_or_critical_advisories_wrong_head_and_network_failures_refuse(self):
        base = example()
        for level in ("high", "critical"):
            report = copy.deepcopy(base)
            report["metadata"]["vulnerabilities"]["moderate"] = 0
            report["metadata"]["vulnerabilities"][level] = 1
            report["vulnerabilities"]["synthetic-pkg"]["severity"] = level
            with self.assertRaises(ValueError):
                gate.evaluate(report, SHA, SHA, OS, 0)
        with self.assertRaises(ValueError):
            gate.evaluate(base, SHA, "b" * 40, OS, 0)
        with self.assertRaises(ValueError):
            gate.evaluate(base, SHA, SHA, "ubuntu-24.04", 0)
        with self.assertRaises(ValueError):
            gate.evaluate(base, SHA, SHA, OS, 1)

    def test_missing_catalog_inconsistent_severity_or_partial_output_refuse(self):
        base = example()
        for damage in (
            lambda d: d.update(error={"code": "ENOAUDIT"}),
            lambda d: d["metadata"]["vulnerabilities"].update(total=0),
            lambda d: d["vulnerabilities"].clear(),
            lambda d: d["vulnerabilities"]["synthetic-pkg"].update(severity="high"),
            lambda d: d["vulnerabilities"]["synthetic-pkg"].update(severity="unknown"),
            lambda d: d.update(auditReportVersion=1),
            lambda d: d.pop("metadata"),
        ):
            with self.subTest(damage=damage):
                report = copy.deepcopy(base)
                damage(report)
                with self.assertRaises(ValueError):
                    gate.evaluate(report, SHA, SHA, OS, 0)


if __name__ == "__main__":
    unittest.main()
