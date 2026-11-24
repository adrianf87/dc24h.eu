"""Stage III synthetic v11/six-file, trust and pathname-swap regression.

This is an offline preflight only: the hosted MariaDB/C++/Node/Chromium VM
must separately prove actual service restoration and authentication.
"""

from contextlib import contextmanager
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import beta08_recovery_auth as auth
import beta07_recovery_preflight as preflight
import stage1_recovery_bundle as pair
import stage4_private_recovery_drill as private
from stage1_recovery_bundle_tests import offline_fixture


class StageThreeRecoveryTests(unittest.TestCase):
    def fixture(self, root):
        dump, snapshot, pair_dir = offline_fixture(root)
        rows = ",".join(
            f"({version},'migration_{version}','{'a' * 64}','2026-11-24 00:00:00','applied')"
            for version in range(1, 12))
        dump.write_text("CREATE TABLE `dc24h_schema_migrations` (\n"
                        "  `version` int unsigned NOT NULL\n);\n"
                        f"INSERT INTO `dc24h_schema_migrations` VALUES {rows};\n")
        pair.create_pair(dump, snapshot, pair_dir, "a" * 40)
        trust = root / "trust"
        trust.mkdir(mode=0o700)
        key, ledger = trust / "key", trust / "ledger"
        private.write_private(trust, key.name, os.urandom(32))
        auth.initialize_ledger(ledger)
        detached = root / "auth"
        detached.mkdir(mode=0o700)
        key_id = auth.sign(pair_dir, key, detached)
        return dump, snapshot, pair_dir, key, ledger, detached, key_id

    def test_six_components_preflight_and_authenticated_one_use_restore(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            dump, snapshot, paired, key, ledger, detached, key_id = self.fixture(root)
            target_sql, target_state = root / "new-sql", root / "new-state"
            checked = preflight.preflight(dump, snapshot, paired, "a" * 40,
                                          target_sql, target_state)
            self.assertEqual(checked["verified_components"], 6)
            self.assertEqual(checked["sql_export_applied_migration_rows"], 11)
            authenticated = auth.verify(dump, snapshot, paired, "a" * 40,
                                        detached, key, key_id, ledger, consume=True)
            self.assertTrue(authenticated["anti_reuse_recorded"])
            # The SQL target is deliberately untouched: only a hosted fresh
            # MariaDB instance may import/execute an operator SQL export.
            self.assertFalse(target_sql.exists())
            self.assertEqual(private.restore_to_fresh_directory(snapshot, target_state), 5)
            self.assertEqual(set(item.name for item in target_state.iterdir()), set(private.FILES))
            with self.assertRaisesRegex(ValueError, "already accepted"):
                auth.verify(dump, snapshot, paired, "a" * 40, detached, key, key_id, ledger)
            self.assertEqual(private.verify(snapshot), 5)

    def test_torn_store_or_wrong_generation_refuses_before_ledger_consumption(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            dump, snapshot, paired, key, ledger, detached, key_id = self.fixture(root)
            for sha in ("b" * 40, "c" * 40):
                with self.assertRaisesRegex(ValueError, "different source commit"):
                    preflight.preflight(dump, snapshot, paired, sha,
                                        root / "new-sql", root / "new-state")
            (snapshot / "replay.jsonl").unlink()
            with self.assertRaises((OSError, ValueError)):
                auth.verify(dump, snapshot, paired, "a" * 40,
                            detached, key, key_id, ledger, consume=True)
            self.assertEqual(ledger.read_bytes(), b"")
            self.assertFalse((root / "new-state").exists())

    def test_sql_path_replaced_during_pinned_read_even_with_identical_bytes(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            dump, snapshot, paired, key, ledger, detached, key_id = self.fixture(root)
            replacement = root / "replacement.sql"
            replacement.write_bytes(dump.read_bytes())
            replacement.chmod(0o600)
            source_stream = private.read_private_stream
            swapped = False

            class SwappingReader:
                def __init__(self, reader):
                    self.reader = reader

                def fileno(self):
                    return self.reader.fileno()

                def readline(self, length):
                    nonlocal swapped
                    content = self.reader.readline(length)
                    if not swapped:
                        replacement.replace(dump)
                        swapped = True
                    return content

            @contextmanager
            def swap_during_sql_stream(path):
                with source_stream(path) as reader:
                    yield SwappingReader(reader)

            with mock.patch.object(preflight.private, "read_private_stream", side_effect=swap_during_sql_stream):
                with self.assertRaisesRegex(ValueError, "(stream source replaced|SQL recovery source changed)"):
                    preflight.preflight(dump, snapshot, paired, "a" * 40,
                                        root / "fresh-sql", root / "fresh-state")
            self.assertEqual(ledger.read_bytes(), b"")
            self.assertTrue(swapped)
            self.assertFalse((root / "fresh-sql").exists())
            self.assertFalse((root / "fresh-state").exists())
            # Replacement was byte-identical: the stream's inode/metadata
            # checks refuse the mid-scan rename without relying on a digest.
            self.assertEqual(auth.verify(dump, snapshot, paired, "a" * 40,
                                         detached, key, key_id, ledger)["verified_components"], 6)


if __name__ == "__main__":
    unittest.main()
