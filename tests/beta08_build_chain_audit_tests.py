"""Synthetic npm audit negative tests for the full build toolchain policy."""

import copy
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import beta08_build_chain_audit as gate
from beta07_npm_audit_gate_tests import example, SHA, OS

LOCK = json.dumps({"packages": {
    "node_modules/runtime": {"version": "1.0.0"},
    "node_modules/ci-build-tool": {"version": "2.0.0", "dev": True}}}).encode()


class BuildChainAuditTests(unittest.TestCase):
    def test_full_runtime_and_dev_lock_audit_is_redacted(self):
        report = gate.evaluate(example(), SHA, SHA, OS, 0, LOCK)
        self.assertTrue(report["runtime_and_build_chain_audited"])
        self.assertTrue(report["scope_enforced_by_invoking_npm"])
        self.assertEqual(report["dependency_exceptions"], [])
        self.assertFalse(report["raw_advisories_exported"])
        self.assertNotIn("private-do-not-log", str(report))

    def test_high_severity_wrong_head_or_error_cannot_be_waived(self):
        vulnerable = copy.deepcopy(example())
        vulnerable["metadata"]["vulnerabilities"].update(high=1, moderate=0)
        vulnerable["vulnerabilities"]["synthetic-pkg"]["severity"] = "high"
        for raw, head, exit_code, lock in ((vulnerable, SHA, 1, LOCK),
                                           (vulnerable, SHA, 0, LOCK),
                                           (example(), "b" * 40, 0, LOCK),
                                           (example(), SHA, 0, b""),
                                           (example(), SHA, 0, json.dumps({"packages": {
                                               "node_modules/runtime": {}}}).encode())):
            with self.subTest(head=head, exit_code=exit_code), self.assertRaises(ValueError):
                gate.evaluate(raw, SHA, head, OS, exit_code, lock)

    def test_invocation_pins_production_and_dev_flags_and_never_prints_raw_report(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)

            def fake_run(command, **kwargs):
                self.assertEqual(command, gate.AUDIT_COMMAND)
                self.assertIn("--include=dev", command)
                self.assertIn("--include=prod", command)
                self.assertFalse(any(arg.startswith("--omit") for arg in command))
                self.assertEqual(kwargs["cwd"], root / "webadmin")
                self.assertEqual(kwargs["timeout"], 180)
                self.assertEqual(kwargs["stderr"], subprocess.DEVNULL)
                kwargs["stdout"].write(json.dumps(example()).encode())
                return subprocess.CompletedProcess(command, 0)

            with mock.patch.object(gate.subprocess, "run", side_effect=fake_run):
                raw, status = gate.collect_full_audit(root)
            self.assertEqual(status, 0)
            self.assertEqual(raw, example())

    def test_oversized_npm_audit_is_not_accepted(self):
        with tempfile.TemporaryDirectory() as directory:
            def fake_run(command, **kwargs):
                kwargs["stdout"].write(b"x" * (gate.MAX_AUDIT_BYTES + 1))
                return subprocess.CompletedProcess(command, 0)

            with mock.patch.object(gate.subprocess, "run", side_effect=fake_run):
                with self.assertRaisesRegex(ValueError, "oversized"):
                    gate.collect_full_audit(Path(directory))


if __name__ == "__main__":
    unittest.main()
