"""Disposable private-ledger backup/restore drills; never touch real services."""

import contextlib
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import tempfile
import unittest


SCRIPT = Path(__file__).resolve().parents[1] / "scripts/stage4_private_recovery_drill.py"
spec = importlib.util.spec_from_file_location("stage4_private_recovery_drill", SCRIPT)
recovery = importlib.util.module_from_spec(spec)
spec.loader.exec_module(recovery)


def fixture(root):
    original = root / "private"
    original.mkdir(mode=0o700)
    event, at, prev = "login_ok", "2026-11-23T01:02:03.000Z", "0" * 64
    digest = hashlib.sha256(json.dumps([1, 1, prev, event, at], separators=(",", ":")).encode()).hexdigest()
    files = {
        "mfa.json": json.dumps({"schema": 1, "accounts": {"7": "A" * 32}}) + "\n",
        "replay.jsonl": "",
        "audit.jsonl": json.dumps({"schema": 1, "sequence": 1, "previous": prev,
                                   "event": event, "at": at, "digest": digest}, separators=(",", ":")) + "\n",
        "passkeys.json": json.dumps({"schema": 1, "rpID": "localhost", "users": [], "credentials": []}) + "\n",
        "rate-limits.json": json.dumps({"schema": 1, "buckets": []}) + "\n",
    }
    for name, data in files.items():
        file = original / name
        file.write_text(data, encoding="utf8")
        file.chmod(0o600)
    return original


class StageFourPrivateRecovery(unittest.TestCase):
    def test_snapshot_verification_and_restoration_without_overwriting_live_state(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = fixture(root)
            backup, restored = root / "backup", root / "restored"
            self.assertEqual(recovery.snapshot(str(source), str(backup)), 5)
            self.assertEqual(recovery.verify(str(backup)), 5)
            self.assertEqual(recovery.restore_to_fresh_directory(str(backup), str(restored)), 5)
            self.assertFalse((restored / "manifest.json").exists())
            for name in recovery.FILES:
                self.assertEqual((source / name).read_bytes(), (restored / name).read_bytes())
                self.assertEqual((restored / name).stat().st_mode & 0o777, 0o600)
                self.assertEqual((backup / name).stat().st_mode & 0o777, 0o600)
            self.assertEqual((backup / "manifest.json").stat().st_mode & 0o777, 0o600)
            self.assertEqual(restored.stat().st_mode & 0o777, 0o700)
            with self.assertRaises(FileExistsError):
                recovery.restore_to_fresh_directory(str(backup), str(restored))

    def test_corruption_missing_source_and_symlink_are_refused(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = fixture(root)
            (source / "replay.jsonl").write_bytes(b'{"actor":"7"')
            with self.assertRaises(ValueError):
                recovery.snapshot(str(source), str(root / "no-backup"))
            self.assertFalse((root / "no-backup").exists())
            (source / "replay.jsonl").write_bytes(b"")
            backup = root / "backup"
            recovery.snapshot(str(source), str(backup))
            audit = backup / "audit.jsonl"
            contents = audit.read_bytes()
            audit.write_bytes(contents.replace(b"login_ok", b"logout__"))
            with self.assertRaises(ValueError):
                recovery.verify(str(backup))
            self.assertEqual(source.joinpath("audit.jsonl").read_bytes(), contents,
                             "a corrupt backup may not mutate the original")
            (source / "mfa.json").unlink()
            os.symlink(source / "passkeys.json", source / "mfa.json")
            with self.assertRaises(OSError):
                recovery.snapshot(str(source), str(root / "invalid"))

    def test_cli_needs_offline_acknowledgments_and_does_not_claim_rto(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = fixture(root)
            with self.assertRaises(SystemExit), contextlib.redirect_stderr(io.StringIO()):
                recovery.main(["snapshot", "--source", str(source), "--destination", str(root / "backup")])
            report = io.StringIO()
            with contextlib.redirect_stdout(report):
                self.assertEqual(recovery.main(["snapshot", "--source", str(source),
                    "--destination", str(root / "backup"), "--ack-stopped", "--ack-isolated"]), 0)
            parsed = json.loads(report.getvalue())
            self.assertFalse(parsed["target_recovery_completed"])
            self.assertEqual(parsed["verified_files"], 5)
            self.assertNotIn("AAAAAAAA", report.getvalue())
            self.assertNotIn(str(root), report.getvalue())


if __name__ == "__main__":
    unittest.main()
