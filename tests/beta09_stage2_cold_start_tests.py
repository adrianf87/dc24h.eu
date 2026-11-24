"""Cold-start negatives for the EXISTING stopped-writer backup MAC only.

These are not runtime private-store v2 startup tests. They deliberately use
synthetic accounts, quotas, replay steps, passkeys and a separate test key.
"""

import hashlib
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from stage1_recovery_bundle_tests import offline_fixture
import beta08_recovery_auth as auth


class ExistingOfflineAuthenticatedPair(unittest.TestCase):
    def test_same_length_mtime_restored_five_store_rewrites_refused_after_cold_open(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            dump, _, pair_dir = offline_fixture(root)
            source = root / "source"
            (source / "rate-limits.json").write_text(
                '{"schema":1,"buckets":[{"key":"' + "a" * 64 + '","count":4,"starts":1000}]}\n')
            (source / "replay.jsonl").write_text('{"actor":"7","step":4}\n')
            timestamp = "2026-01-01T00:00:00.000Z"
            previous = "0" * 64
            event = "login_refused"
            digest = hashlib.sha256(json.dumps([1, 1, previous, event, timestamp],
                           separators=(",", ":")).encode()).hexdigest()
            (source / "audit.jsonl").write_text(json.dumps({"schema": 1, "sequence": 1,
                "previous": previous, "event": event, "at": timestamp, "digest": digest}) + "\n")
            new_snapshot = root / "matched-private"
            auth.private.snapshot(str(source), str(new_snapshot))
            auth.pair.create_pair(dump, new_snapshot, pair_dir, "a" * 40)
            trusted = root / "trusted"
            trusted.mkdir(mode=0o700)
            key = trusted / "key"
            auth.private.write_private(trusted, "key", b"K" * 32)
            ledger = trusted / "accepted"
            auth.initialize_ledger(ledger)
            detached = root / "detached"
            detached.mkdir(mode=0o700)
            key_id = auth.sign(pair_dir, key, detached)
            args = (dump, new_snapshot, pair_dir, "a" * 40, detached, key, key_id, ledger)
            self.assertTrue(auth.verify(*args)["authenticated"])
            for filename, old, new in (
                ("mfa.json", b"A" * 32, b"B" * 32),
                ("replay.jsonl", b'"step":4', b'"step":1'),
                ("audit.jsonl", b"login_refused", b"route_refused"),
                ("passkeys.json", b"localhost", b"localhosu"),
                ("rate-limits.json", b'"count":4', b'"count":1'),
            ):
                file = new_snapshot / filename
                saved = file.read_bytes()
                before = file.stat()
                forged = saved.replace(old, new)
                self.assertEqual(len(forged), len(saved))
                self.assertNotEqual(forged, saved)
                try:
                    file.write_bytes(forged)
                    os.utime(file, ns=(before.st_atime_ns, before.st_mtime_ns))
                    self.assertEqual(file.stat().st_mtime_ns, before.st_mtime_ns)
                    with self.subTest(file=filename), self.assertRaises(ValueError):
                        auth.verify(*args)
                finally:
                    file.write_bytes(saved)
                    os.utime(file, ns=(before.st_atime_ns, before.st_mtime_ns))
                self.assertTrue(auth.verify(*args)["authenticated"])
            self.assertEqual(ledger.read_bytes(), b"", "read-only proof never accepts/restores")


if __name__ == "__main__":
    unittest.main()
