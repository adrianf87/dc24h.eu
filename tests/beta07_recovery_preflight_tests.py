"""Negative, read-only validation of disposable SQL/private recovery targets."""

import contextlib
import io
import os
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import beta07_recovery_preflight as preflight
from stage1_recovery_bundle_tests import offline_fixture


class StageOnePreflightTests(unittest.TestCase):
    def create(self, root):
        dump, snapshot, pair_dir = offline_fixture(root)
        preflight.pair.create_pair(dump, snapshot, pair_dir, "a" * 40)
        return dump, snapshot, pair_dir

    def test_paired_six_component_preflight_does_not_create_destinations_or_expose_contents(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            dump, snapshot, pair_dir = self.create(root)
            sql, gateway = root / "new-sql", root / "new-gateway"
            result = preflight.preflight(dump, snapshot, pair_dir, "a" * 40, sql, gateway)
            self.assertEqual(result["verified_components"], 6)
            self.assertEqual(result["fresh_targets_checked"], 2)
            self.assertFalse(result["restore_performed"])
            self.assertFalse(result["schema_v11_verified_after_import"])
            self.assertFalse(result["writers_quiescent_verified"])
            self.assertFalse(sql.exists() or gateway.exists())
            self.assertNotIn("CREATE TABLE", str(result))
            self.assertNotIn(str(root), str(result))

    def test_no_existing_database_gateway_symlink_or_nested_target_can_be_modified(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            dump, snapshot, pair_dir = self.create(root)
            sql, gateway = root / "sql", root / "gateway"
            sql.mkdir(mode=0o700)
            sentinel = sql / "existing-data"
            sentinel.write_bytes(b"keep-existing-records")
            for destination in (sql, snapshot / "nested", pair_dir / "nested"):
                with self.subTest(destination=destination):
                    with self.assertRaises(ValueError):
                        preflight.preflight(dump, snapshot, pair_dir, "a" * 40, destination, gateway)
            self.assertEqual(sentinel.read_bytes(), b"keep-existing-records")
            dangling = root / "dangling"
            os.symlink(root / "does-not-exist", dangling)
            with self.assertRaises(ValueError):
                preflight.preflight(dump, snapshot, pair_dir, "a" * 40, dangling, gateway)
            self.assertTrue(dangling.is_symlink())
            with self.assertRaises(ValueError):
                preflight.preflight(dump, snapshot, pair_dir, "a" * 40, gateway, gateway / "inside")

    def test_cross_generation_sha_corrupt_dump_and_partial_private_bundle_refuse(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            dump, snapshot, pair_dir = self.create(root)
            sql, gateway = root / "new-sql", root / "new-gateway"
            with self.assertRaisesRegex(ValueError, "different source commit"):
                preflight.preflight(dump, snapshot, pair_dir, "b" * 40, sql, gateway)
            dump.write_bytes(b"changed SQL generation\n")
            with self.assertRaises(ValueError):
                preflight.preflight(dump, snapshot, pair_dir, "a" * 40, sql, gateway)
            (snapshot / "replay.jsonl").unlink()
            with self.assertRaises((OSError, ValueError)):
                preflight.preflight(dump, snapshot, pair_dir, "a" * 40, sql, gateway)
            self.assertFalse(sql.exists() or gateway.exists())

    def test_cli_refusals_do_not_output_secret_paths_or_sql(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            dump, snapshot, pair_dir = self.create(root)
            result = ["--mariadb-dump", str(dump), "--private-snapshot", str(snapshot),
                      "--pair-directory", str(pair_dir), "--commit-sha", "b" * 40,
                      "--fresh-sql-destination", str(root / "new-sql"),
                      "--fresh-private-destination", str(root / "new-gateway"), "--ack-isolated"]
            err = io.StringIO()
            with contextlib.redirect_stderr(err):
                self.assertEqual(preflight.main(result), 1)
            self.assertNotIn(str(root), err.getvalue())
            self.assertNotIn("CREATE TABLE", err.getvalue())


if __name__ == "__main__":
    unittest.main()
