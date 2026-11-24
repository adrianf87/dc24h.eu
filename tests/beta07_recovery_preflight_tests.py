"""Negative, read-only validation of disposable SQL/private recovery targets."""

import contextlib
import io
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import beta07_recovery_preflight as preflight
from stage1_recovery_bundle_tests import offline_fixture


class StageOnePreflightTests(unittest.TestCase):
    def create(self, root):
        dump, snapshot, pair_dir = offline_fixture(root)
        # Textual unit fixture matches the fixed mariadb-dump migration row
        # shape; it does not claim to be an executable SQL schema. Hosted VM
        # tests separately check an actual MariaDB dump and imported C++ v11.
        rows = ",".join(
            f"({version},'migration_{version}','{'a' * 64}','2026-11-24 00:00:00','applied')"
            for version in range(1, 12))
        dump.write_text("CREATE TABLE `dc24h_schema_migrations` (\n"
                        "  `version` int unsigned NOT NULL\n);\n"
                        f"INSERT INTO `dc24h_schema_migrations` VALUES {rows};\n")
        preflight.pair.create_pair(dump, snapshot, pair_dir, "a" * 40)
        return dump, snapshot, pair_dir

    def test_paired_six_component_preflight_does_not_create_destinations_or_expose_contents(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            dump, snapshot, pair_dir = self.create(root)
            sql, gateway = root / "new-sql", root / "new-gateway"
            result = preflight.preflight(dump, snapshot, pair_dir, "a" * 40, sql, gateway)
            self.assertEqual(result["verified_components"], 6)
            self.assertEqual(result["sql_export_applied_migration_rows"], 11)
            self.assertTrue(result["sql_export_shape_checked"])
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

    def test_matching_hash_does_not_accept_wrong_sql_version_or_missing_rows(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            dump, snapshot, _pair_dir = self.create(root)
            sql, private_target = root / "fresh-sql", root / "fresh-private"
            # Every altered SQL generation gets its own valid *new* hash pair;
            # hash validity must not substitute for a schema-version check.
            for forged in (b"CREATE TABLE `fixture` (id INT);\n",
                           dump.read_bytes().replace(b"(11,'migration_11'", b"(12,'migration_11'"),
                           dump.read_bytes().replace(b"'applied')", b"'applying')", 1)):
                with self.subTest(forged=forged[:16]):
                    dump.write_bytes(forged)
                    fresh_pair = root / ("pair-" + str(len(list(root.glob("pair-*")))))
                    fresh_pair.mkdir(mode=0o700)
                    preflight.pair.create_pair(dump, snapshot, fresh_pair, "a" * 40)
                    with self.assertRaises(ValueError):
                        preflight.preflight(dump, snapshot, fresh_pair, "a" * 40, sql, private_target)
            self.assertFalse(sql.exists() or private_target.exists())

    def test_sql_scan_refuses_symlink_fifo_and_world_readable_input(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            dump, _snapshot, _pair_dir = self.create(root)
            link = root / "linked.sql"
            link.symlink_to(dump)
            with self.assertRaises(OSError):
                preflight.sql_migration_row_count(link)
            fifo = root / "waiting.sql"
            os.mkfifo(fifo, 0o600)
            with self.assertRaises(ValueError):
                preflight.sql_migration_row_count(fifo)
            dump.chmod(0o644)
            with self.assertRaises(ValueError):
                preflight.sql_migration_row_count(dump)

    def test_source_changed_after_valid_pair_refuses_without_creating_targets(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            dump, snapshot, pair_dir = self.create(root)
            sql, gateway = root / "new-sql", root / "new-gateway"

            def replaced_sql(_path):
                dump.write_bytes(dump.read_bytes() + b"-- tampered after pair validation\n")
                return 11

            with mock.patch.object(preflight, "sql_migration_row_count", side_effect=replaced_sql):
                with self.assertRaisesRegex(ValueError, "recovery pair mismatch"):
                    preflight.preflight(dump, snapshot, pair_dir, "a" * 40, sql, gateway)
            self.assertFalse(sql.exists() or gateway.exists())

    def test_mariadb_dump_whitespace_and_fractional_timestamp_are_accepted(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            dump, snapshot, _pair_dir = self.create(root)
            original = dump.read_bytes()
            # Some hosted MariaDB versions separate tuples with spaces and
            # preserve subsecond precision; both are valid SQL-v11 exports.
            changed = original.replace(b"),(2,", b"), (2,")
            changed = changed.replace(b"'2026-11-24 00:00:00'", b"'2026-11-24 00:00:00.123456'")
            changed = changed.replace(b"VALUES (", b"VALUES  ( ")
            dump.write_bytes(changed)
            pair_dir = root / "fractional-pair"
            pair_dir.mkdir(mode=0o700)
            preflight.pair.create_pair(dump, snapshot, pair_dir, "a" * 40)
            self.assertEqual(preflight.sql_migration_row_count(dump), 11)
            self.assertEqual(preflight.preflight(dump, snapshot, pair_dir, "a" * 40,
                                                root / "fresh-db", root / "fresh-private")
                             ["sql_export_applied_migration_rows"], 11)

    def test_mariadb_dump_multiline_values_are_read_within_strict_bounds(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            dump, _snapshot, _pair_dir = self.create(root)
            original = dump.read_bytes()
            self.assertIn(b" VALUES (", original)
            dump.write_bytes(original.replace(b" VALUES (", b" VALUES\n(")
                                  .replace(b"),(2,", b"),\n(2,"))
            self.assertEqual(preflight.sql_migration_row_count(dump), 11)

            # Do not read an unbounded continuation and never treat a
            # missing terminator as a valid migration-history export.
            dump.write_bytes(original.replace(b" VALUES (", b" VALUES\n(")[:-2])
            with self.assertRaisesRegex(ValueError, "Incomplete or oversized"):
                preflight.sql_migration_row_count(dump)

    def test_parser_never_ignores_extra_sql_between_valid_migration_rows(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            dump, _snapshot, _pair_dir = self.create(root)
            dump.write_bytes(dump.read_bytes().replace(b"),(2,", b"),SELECT 1,(2,"))
            with self.assertRaisesRegex(ValueError, "Malformed migration export"):
                preflight.sql_migration_row_count(dump)

    def test_structural_diagnostic_does_not_echo_private_values(self):
        secret = b"PersonallyIdentifyingAccountName"
        shape = preflight._redacted_row_shape(b"(1,'" + secret + b"','abc',NULL,'applied');")
        self.assertEqual(shape, "fields5_nqqzq")
        self.assertNotIn(secret.decode(), shape)
        self.assertEqual(preflight._redacted_row_shape(b"garbage" + secret), "missing_tuple")

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

    def test_oversized_migration_line_is_rejected_without_reading_unbounded_input(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            dump, snapshot, _ = self.create(root)
            dump.write_bytes(b"INSERT INTO `dc24h_schema_migrations` VALUES " +
                             b"X" * (1024 * 1024 + 1) + b"\n")
            pair_dir = root / "large-pair"
            pair_dir.mkdir(mode=0o700)
            preflight.pair.create_pair(dump, snapshot, pair_dir, "a" * 40)
            with self.assertRaisesRegex(ValueError, "Oversized migration export line"):
                preflight.preflight(dump, snapshot, pair_dir, "a" * 40,
                                    root / "fresh-sql", root / "fresh-private")
            self.assertFalse((root / "fresh-sql").exists())


if __name__ == "__main__":
    unittest.main()
