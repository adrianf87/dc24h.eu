"""Disposable private-ledger backup/restore drills; never touch real services."""

import contextlib
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import re
import tempfile
import unittest
from unittest import mock


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
    def test_audit_event_contract_matches_gateway(self):
        module = SCRIPT.parents[1] / "webadmin/server/remote-audit.ts"
        declaration = module.read_text(encoding="utf8").split("export const remoteAuditEvents = [", 1)[1]
        declaration = declaration.split("] as const", 1)[0]
        self.assertEqual(recovery.AUDIT_EVENTS, set(re.findall(r'"([a-z_]+)"', declaration)))

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
            (source / "mfa.json").unlink()
            os.mkfifo(source / "mfa.json", 0o600)
            with self.assertRaises(ValueError):
                recovery.snapshot(str(source), str(root / "invalid-fifo"))

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

    def test_snapshot_rejects_changed_bytes_between_preflight_and_copy(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = fixture(root)
            original = recovery.read_private
            seen = 0

            def changed(directory, name, maximum, allow_empty=False):
                nonlocal seen
                data = original(directory, name, maximum, allow_empty)
                if name == "mfa.json" and directory == source:
                    seen += 1
                    if seen == 2:
                        data[:] = data.replace(b"A" * 32, b"B" * 32)
                return data

            with mock.patch.object(recovery, "read_private", side_effect=changed):
                with self.assertRaises(ValueError):
                    recovery.snapshot(str(source), str(root / "inconsistent"))

    def test_restore_rejects_backup_changed_after_manifest_check(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = fixture(root)
            backup = root / "backup"
            recovery.snapshot(str(source), str(backup))
            original = recovery.read_private
            seen = 0

            def changed(directory, name, maximum, allow_empty=False):
                nonlocal seen
                data = original(directory, name, maximum, allow_empty)
                if name == "mfa.json" and directory == backup:
                    seen += 1
                    if seen == 2:
                        data[:] = data.replace(b"A" * 32, b"B" * 32)
                return data

            with mock.patch.object(recovery, "read_private", side_effect=changed):
                with self.assertRaises(ValueError):
                    recovery.restore_to_fresh_directory(str(backup), str(root / "restored"))

    def test_restore_rechecks_bytes_written_to_fresh_directory(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            backup = root / "backup"
            recovery.snapshot(str(fixture(root)), str(backup))
            original = recovery.write_private

            def corrupt_written(directory, name, data):
                if name == "mfa.json" and directory == root / "restored":
                    data = data.replace(b"A" * 32, b"B" * 32)
                return original(directory, name, data)

            with mock.patch.object(recovery, "write_private", side_effect=corrupt_written):
                with self.assertRaises(ValueError):
                    recovery.restore_to_fresh_directory(str(backup), str(root / "restored"))

    def test_invalid_gateway_state_is_rejected_before_snapshot(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = fixture(root)
            invalid = [
                ("mfa.json", {"schema": 1, "accounts": {"7": "A" * 32, "8": "A" * 32}}),
                ("replay.jsonl", '{"actor":"7","step":1,"nonce":"bad"}\n'),
                ("passkeys.json", {"schema": 1, "rpID": "localhost", "users": [], "credentials": [{"actor": "7"}]}),
                ("rate-limits.json", {"schema": 1, "buckets": [{"key": "a" * 64, "count": 1, "starts": 1},
                    {"key": "a" * 64, "count": 2, "starts": 2}]}),
            ]
            for name, bad in invalid:
                with self.subTest(name=name):
                    old = (source / name).read_bytes()
                    (source / name).write_text(bad if isinstance(bad, str) else json.dumps(bad), encoding="utf8")
                    destination = root / ("untrusted-" + name)
                    with self.assertRaises(ValueError):
                        recovery.snapshot(str(source), str(destination))
                    self.assertFalse(destination.exists())
                    (source / name).write_bytes(old)
            event, at, prev = "unlisted_event", "2026-11-23T01:02:03.000Z", "0" * 64
            digest = hashlib.sha256(json.dumps([1, 1, prev, event, at], separators=(",", ":")).encode()).hexdigest()
            (source / "audit.jsonl").write_text(json.dumps({"schema": 1, "sequence": 1, "previous": prev,
                "event": event, "at": at, "digest": digest}) + "\n", encoding="utf8")
            with self.assertRaises(ValueError):
                recovery.snapshot(str(source), str(root / "untrusted-audit"))

    def test_valid_existing_passkey_credential_survives_restore(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = fixture(root)
            handle = "a" * 43
            passkeys = {"schema": 1, "rpID": "localhost", "users": [
                {"actor": "7", "handle": handle, "epoch": 1}], "credentials": [
                {"actor": "7", "id": "c" * 32, "userHandle": handle, "publicKey": "b" * 60,
                 "counter": 2, "deviceType": "multiDevice", "backedUp": True,
                 "transports": ["internal", "hybrid"], "name": "Laptop key",
                 "createdAt": 1000, "lastUsedAt": 2000, "revokedAt": None}]}
            (source / "passkeys.json").write_text(json.dumps(passkeys) + "\n", encoding="utf8")
            backup, restored = root / "backup", root / "restored"
            self.assertEqual(recovery.snapshot(str(source), str(backup)), 5)
            self.assertEqual(recovery.restore_to_fresh_directory(str(backup), str(restored)), 5)
            self.assertEqual((source / "passkeys.json").read_bytes(), (restored / "passkeys.json").read_bytes())


if __name__ == "__main__":
    unittest.main()
