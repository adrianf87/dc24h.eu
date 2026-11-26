#!/usr/bin/env python3
"""Negative and positive tests for synthetic-only Stage II qualification."""

from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
from stage2_isolated_proof import run, snapshot, synthetic_recovery, systemd_contract


class Stage2IsolatedProofTests(unittest.TestCase):
    def test_synthetic_recovery_isolated_and_reproducible(self):
        with tempfile.TemporaryDirectory() as parent:
            result = run(parent)
            self.assertEqual(result["scope"], "ISOLATED_SYNTHETIC_ONLY")
            self.assertTrue(result["recovery"]["restore_after_injected_failure"])
            self.assertTrue(result["recovery"]["rollback_verified"])
            self.assertFalse(result["production_go"])
            self.assertFalse(result["migration_go"])
            self.assertFalse(result["public_listener_go"])
            self.assertEqual(list(Path(parent).iterdir()), [])

    def test_systemd_contract_never_starts_real_units(self):
        outcome = systemd_contract()
        self.assertTrue(outcome["template_contract_pass"])
        self.assertFalse(outcome["systemd_manager_activated"])
        self.assertFalse(outcome["live_unit_conflict_executed"])

    def test_snapshot_detects_corrupt_content_and_mode(self):
        with tempfile.TemporaryDirectory() as parent:
            root = Path(parent)
            item = root / "test.fixture"
            item.write_bytes(b"initial")
            item.chmod(0o600)
            baseline = snapshot(root)
            item.write_bytes(b"corrupted")
            self.assertNotEqual(snapshot(root), baseline)
            item.write_bytes(b"initial")
            item.chmod(0o644)
            self.assertNotEqual(snapshot(root), baseline)

    def test_runner_temp_must_exist(self):
        with tempfile.TemporaryDirectory() as parent:
            with self.assertRaises(FileNotFoundError):
                run(str(Path(parent) / "does-not-exist"))


if __name__ == "__main__":
    unittest.main()
