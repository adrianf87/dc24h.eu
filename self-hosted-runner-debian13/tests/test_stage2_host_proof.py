#!/usr/bin/env python3
"""Static and synthetic regression checks for the privileged Stage II host proof."""

from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
from stage2_host_proof import (CONFIRMATION, PREFIX_PARENT, RUNNER, RUNNER_USER,
                               ProofError, inside, require, snapshot, write_unit)


class Stage2HostProofTests(unittest.TestCase):
    def test_confirmation_and_targets_are_specific(self):
        self.assertEqual(CONFIRMATION, "RUN_STAGE2_SUDO_HOST_TESTS")
        self.assertEqual(RUNNER, "server-747585786")
        self.assertEqual(RUNNER_USER, "github-runner")
        self.assertEqual(PREFIX_PARENT, Path("/opt"))

    def test_target_must_be_strict_descendant(self):
        with tempfile.TemporaryDirectory() as directory:
            parent = Path(directory)
            child = parent / "stage"
            child.mkdir()
            self.assertTrue(inside(child, parent))
            self.assertFalse(inside(parent, parent))
            with tempfile.TemporaryDirectory() as other:
                self.assertFalse(inside(Path(other), parent))
                (parent / "out-of-tree").symlink_to(other)
                self.assertFalse(inside(parent / "out-of-tree", parent))

    def test_fixture_snapshot_detects_content_and_mode_changes(self):
        with tempfile.TemporaryDirectory() as directory:
            item = Path(directory) / "synthetic.fixture"
            item.write_bytes(b"safe\n")
            item.chmod(0o600)
            original = snapshot(Path(directory))
            item.write_bytes(b"broken\n")
            self.assertNotEqual(snapshot(Path(directory)), original)
            item.write_bytes(b"safe\n")
            item.chmod(0o644)
            self.assertNotEqual(snapshot(Path(directory)), original)

    def test_unit_never_overwrites_an_existing_path(self):
        with tempfile.TemporaryDirectory() as directory:
            unit = Path(directory) / "test.service"
            write_unit(unit, "[Unit]\nDescription=synthetic\n")
            with self.assertRaises(FileExistsError):
                write_unit(unit, "[Unit]\nDescription=replace\n")
            self.assertIn("synthetic", unit.read_text(encoding="utf-8"))

    def test_guard_fails_closed(self):
        with self.assertRaises(ProofError):
            require(False, "guard denied")


if __name__ == "__main__":
    unittest.main()
