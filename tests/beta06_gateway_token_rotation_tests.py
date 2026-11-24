"""Safe, nonsecret owner-only staged token-rotation regression tests."""

import json
from pathlib import Path
import os
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import beta06_gateway_token_rotation as rotation

NOW = 1_800_000_000_000


class OfflineRotationTests(unittest.TestCase):
    def fixture(self, root, starts):
        root.chmod(0o700)
        token = root / "api.token"
        token.write_text("a" * 64 + "\n")
        token.chmod(0o600)
        limiter = root / "rate-limits.json"
        limiter.write_text(json.dumps({"schema": 1, "buckets": [
            {"key": "f" * 64, "count": 1, "starts": starts}]}) + "\n")
        limiter.chmod(0o600)
        return token, limiter

    def rotate(self, root, action, token, limiter):
        return rotation.rotate(action, token=token, candidate=root / "api.next",
                               backup=root / "api.previous", limiter=limiter, now_ms=NOW)

    def test_prepare_commit_and_recover_old_token_without_resetting_history(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            token, limiter = self.fixture(root, NOW - 120_000)
            old = token.read_bytes()
            old_limiter = limiter.read_bytes()
            self.assertEqual(self.rotate(root, "prepare", token, limiter), "staged")
            self.assertEqual(token.read_bytes(), old)
            self.assertEqual((root / "api.next").stat().st_mode & 0o777, 0o600)
            self.assertEqual(self.rotate(root, "commit", token, limiter), "committed")
            self.assertNotEqual(token.read_bytes(), old)
            self.assertEqual((root / "api.previous").read_bytes(), old)
            self.assertEqual(limiter.read_bytes(), old_limiter)
            self.assertEqual(self.rotate(root, "rollback", token, limiter), "rolled_back")
            self.assertEqual(token.read_bytes(), old)
            self.assertFalse((root / "api.previous").exists())

    def test_active_or_future_budgets_block_all_rotation(self):
        for starts in (NOW, NOW - 60_000, NOW + 1):
            with self.subTest(starts=starts), tempfile.TemporaryDirectory() as temp:
                root = Path(temp)
                token, limiter = self.fixture(root, starts)
                with self.assertRaisesRegex(ValueError, "Active/uncertain"):
                    self.rotate(root, "prepare", token, limiter)
                self.assertFalse((root / "api.next").exists())
                self.assertEqual(token.read_bytes(), b"a" * 64 + b"\n")

    def test_symlinked_token_and_unfinished_rotation_are_refused(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            token, limiter = self.fixture(root, NOW - 70_000)
            (root / "api.next").symlink_to(token)
            with self.assertRaises(ValueError):
                self.rotate(root, "prepare", token, limiter)
            (root / "api.next").unlink()
            self.rotate(root, "prepare", token, limiter)
            with self.assertRaises(ValueError):
                self.rotate(root, "prepare", token, limiter)
            with self.assertRaises((ValueError, FileNotFoundError)):
                self.rotate(root, "rollback", token, limiter)
            token.unlink()
            token.symlink_to(root / "api.next")
            with self.assertRaises(OSError):
                self.rotate(root, "commit", token, limiter)


if __name__ == "__main__":
    unittest.main()
