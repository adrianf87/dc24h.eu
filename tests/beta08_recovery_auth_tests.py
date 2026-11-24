"""Synthetic, offline authenticity and non-replay tests (no operator key/data)."""

import contextlib
import io
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import beta08_recovery_auth as auth
from stage1_recovery_bundle_tests import offline_fixture


class AuthenticatedRecoveryTests(unittest.TestCase):
    def setup_bundle(self, root):
        dump, snapshot, pair_dir = offline_fixture(root)
        auth.pair.create_pair(dump, snapshot, pair_dir, "a" * 40)
        trust = root / "independent-trust"; trust.mkdir(mode=0o700)
        key = trust / "key"
        auth.private.write_private(trust, "key", os.urandom(32))
        ledger = trust / "accepted-pairs"
        auth.initialize_ledger(ledger)
        detached = root / "auth"; detached.mkdir(mode=0o700)
        fingerprint = auth.sign(pair_dir, key, detached)
        return dump, snapshot, pair_dir, detached, key, fingerprint, ledger

    def test_six_component_authenticity_once_and_no_changes_to_sources(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            dump, snapshot, pair_dir, detached, key, key_id, ledger = self.setup_bundle(root)
            before = dump.read_bytes(), (pair_dir / auth.pair.NAME).read_bytes()
            args = (dump, snapshot, pair_dir, "a" * 40, detached, key, key_id, ledger)
            result = auth.verify(*args)
            self.assertTrue(result["authenticated"])
            self.assertFalse(result["anti_reuse_recorded"])
            self.assertEqual(result["verified_components"], 6)
            self.assertEqual(ledger.read_bytes(), b"")
            self.assertTrue(auth.verify(*args, consume=True)["anti_reuse_recorded"])
            self.assertEqual((dump.read_bytes(), (pair_dir / auth.pair.NAME).read_bytes()), before)
            self.assertFalse((root / "restore").exists())
            with self.assertRaisesRegex(ValueError, "already accepted"):
                auth.verify(*args)
            with self.assertRaisesRegex(ValueError, "already accepted"):
                auth.verify(*args, consume=True)

    def test_unknown_key_wrong_format_and_modified_pair_fail_closed(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            dump, snapshot, pair_dir, detached, key, key_id, ledger = self.setup_bundle(root)
            args = (dump, snapshot, pair_dir, "a" * 40, detached, key, key_id, ledger)
            with self.assertRaisesRegex(ValueError, "Unknown-key"):
                auth.verify(dump, snapshot, pair_dir, "a" * 40, detached,
                            key, "0" * 32, ledger)
            other_key = key.parent / "other-key"
            auth.private.write_private(key.parent, other_key.name, os.urandom(32))
            with self.assertRaisesRegex(ValueError, "Unknown-key"):
                auth.verify(dump, snapshot, pair_dir, "a" * 40, detached,
                            other_key, key_id, ledger)
            tag = detached / auth.NAME
            saved = tag.read_bytes()
            doc = json.loads(saved)
            doc["schema"] = 2
            tag.write_text(json.dumps(doc) + "\n")
            with self.assertRaisesRegex(ValueError, "Invalid authenticated recovery format"):
                auth.verify(*args)
            tag.write_bytes(saved)
            dump.write_bytes(b"tampered offline SQL")
            with self.assertRaisesRegex(ValueError, "recovery pair mismatch"):
                auth.verify(*args)
            self.assertEqual(ledger.read_bytes(), b"")

    def test_lost_or_corrupt_external_ledger_never_resets_replay_protection(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            dump, snapshot, pair_dir, detached, key, key_id, ledger = self.setup_bundle(root)
            args = (dump, snapshot, pair_dir, "a" * 40, detached, key, key_id, ledger)
            ledger.write_bytes(b"not-a-hex-fingerprint\n")
            with self.assertRaisesRegex(ValueError, "Invalid authenticity anti-reuse ledger"):
                auth.verify(*args)
            ledger.unlink()
            with self.assertRaises(FileNotFoundError):
                auth.verify(*args)
            # Explicit initialization is exclusive and may not silently
            # replace an operator's existing accepted-pair history.
            auth.initialize_ledger(ledger)
            with self.assertRaises(FileExistsError):
                auth.initialize_ledger(ledger)

    def test_cli_refusal_never_prints_private_key_or_sql_paths(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            dump, snapshot, pair_dir, detached, key, key_id, ledger = self.setup_bundle(root)
            err = io.StringIO()
            with contextlib.redirect_stderr(err):
                result = auth.main(["verify", "--mariadb-dump", str(dump),
                                    "--private-snapshot", str(snapshot),
                                    "--pair-directory", str(pair_dir),
                                    "--auth-directory", str(detached),
                                    "--trusted-key", str(key),
                                    "--expected-key-id", "0" * 32,
                                    "--commit-sha", "a" * 40,
                                    "--ledger", str(ledger), "--ack-isolated"])
            self.assertEqual(result, 1)
            self.assertNotIn(str(root), err.getvalue())
            self.assertNotIn(key.read_bytes().hex(), err.getvalue())


if __name__ == "__main__":
    unittest.main()
