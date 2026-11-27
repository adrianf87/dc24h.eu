"""Negative tests for the real MariaDB read lock used during CI-only capture."""

from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import MagicMock, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[0]))
import stage1_matched_recovery_tests as matched


class FakeInput:
    def __init__(self):
        self.writes = []

    def write(self, data):
        self.writes.append(data)

    def flush(self):
        pass

    def close(self):
        pass


class FakeOutput:
    def __init__(self, marker):
        self.marker = marker

    def readline(self):
        return self.marker

    def close(self):
        pass


class OfflineCaptureTests(unittest.TestCase):
    def capture(self, marker=b"CI_LOCKED\n", ready=True):
        root = tempfile.TemporaryDirectory()
        self.addCleanup(root.cleanup)
        db = matched.IsolatedDatabase(Path(root.name) / "db")
        db.proc = MagicMock()
        db.proc.poll.return_value = None
        client = MagicMock()
        client.poll.return_value = None
        client.stdin = FakeInput()
        client.stdout = FakeOutput(marker)
        selector = MagicMock()
        selector.select.return_value = [object()] if ready else []
        context = MagicMock()
        context.__enter__.return_value = selector
        return db, client, context

    def test_read_lock_held_until_pairing_completes_then_released(self):
        db, client, selector = self.capture()
        with patch.object(matched.subprocess, "Popen", return_value=client), \
             patch.object(matched.selectors, "DefaultSelector", return_value=selector):
            with db.locked_offline_capture():
                self.assertEqual(client.stdin.writes,
                                 [b"FLUSH TABLES WITH READ LOCK;\nSELECT 'CI_LOCKED';\n"])
            self.assertEqual(client.stdin.writes[-1], b"UNLOCK TABLES;\nquit\n")
            client.wait.assert_called_once_with(timeout=5)

    def test_timeout_or_wrong_marker_refuses_capture(self):
        for marker, ready in ((b"CI_LOCKED\n", False), (b"not_locked\n", True)):
            with self.subTest(ready=ready):
                db, client, selector = self.capture(marker, ready)
                with patch.object(matched.subprocess, "Popen", return_value=client), \
                     patch.object(matched.selectors, "DefaultSelector", return_value=selector):
                    with self.assertRaisesRegex(AssertionError, "read lock"):
                        with db.locked_offline_capture():
                            self.fail("Unverified snapshot must not start")
                self.assertFalse(any(b"UNLOCK" in item for item in client.stdin.writes))

    def test_disconnected_database_refuses_before_running_client(self):
        with tempfile.TemporaryDirectory() as root:
            db = matched.IsolatedDatabase(Path(root) / "db")
            with patch.object(matched.subprocess, "Popen") as popen:
                with self.assertRaisesRegex(AssertionError, "not available"):
                    with db.locked_offline_capture():
                        self.fail("No writer state without database")
                popen.assert_not_called()


if __name__ == "__main__":
    unittest.main()
