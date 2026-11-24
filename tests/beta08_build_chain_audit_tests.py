"""Synthetic npm audit negative tests for the full build toolchain policy."""

import copy
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import beta08_build_chain_audit as gate
from beta07_npm_audit_gate_tests import example, SHA, OS


class BuildChainAuditTests(unittest.TestCase):
    def test_full_runtime_and_dev_lock_audit_is_redacted(self):
        report = gate.evaluate(example(), SHA, SHA, OS, 0, b"synthetic lock")
        self.assertTrue(report["runtime_and_build_chain_audited"])
        self.assertEqual(report["dependency_exceptions"], [])
        self.assertFalse(report["raw_advisories_exported"])
        self.assertNotIn("private-do-not-log", str(report))

    def test_high_severity_wrong_head_or_error_cannot_be_waived(self):
        vulnerable = copy.deepcopy(example())
        vulnerable["metadata"]["vulnerabilities"].update(high=1, moderate=0)
        vulnerable["vulnerabilities"]["synthetic-pkg"]["severity"] = "high"
        for raw, head, exit_code, lock in ((vulnerable, SHA, 1, b"lock"),
                                           (vulnerable, SHA, 0, b"lock"),
                                           (example(), "b" * 40, 0, b"lock"),
                                           (example(), SHA, 0, b"")):
            with self.subTest(head=head, exit_code=exit_code), self.assertRaises(ValueError):
                gate.evaluate(raw, SHA, head, OS, exit_code, lock)


if __name__ == "__main__":
    unittest.main()
