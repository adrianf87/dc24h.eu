"""No live database: test binding of disposable SQL and gateway state hashes."""

import importlib.util
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts/stage1_recovery_bundle.py"
sys.path.insert(0, str(SCRIPT.parent))
spec = importlib.util.spec_from_file_location("stage1_recovery_bundle", SCRIPT)
bundle = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bundle)


def offline_fixture(root):
    source = root / "source"
    source.mkdir(mode=0o700)
    records = {
        "mfa.json": {"schema": 1, "accounts": {"7": "A" * 32}},
        "passkeys.json": {"schema": 1, "rpID": "localhost", "users": [], "credentials": []},
        "rate-limits.json": {"schema": 1, "buckets": []},
    }
    for name, record in records.items():
        file = source / name
        file.write_text(json.dumps(record) + "\n", encoding="utf-8")
        file.chmod(0o600)
    for name in ("audit.jsonl", "replay.jsonl"):
        file = source / name
        file.write_bytes(b"")
        file.chmod(0o600)
    snapshot = root / "private-snapshot"
    bundle.private.snapshot(str(source), str(snapshot))
    dump = root / "mariadb.sql"
    dump.write_bytes(b"-- disposable test fixture\nCREATE TABLE fixture (id INT);\n")
    dump.chmod(0o600)
    pair_dir = root / "pair"
    pair_dir.mkdir(mode=0o700)
    return dump, snapshot, pair_dir


class RecoveryBundleTests(unittest.TestCase):
    def test_pair_and_verify_do_not_approve_recovery(self):
        with tempfile.TemporaryDirectory() as directory:
            dump, snapshot, pair_dir = offline_fixture(Path(directory))
            result = bundle.create_pair(dump, snapshot, pair_dir, "a" * 40)
            self.assertEqual(result["verified_files"], 6)
            self.assertFalse(result["mariadb_restored"])
            self.assertFalse(result["rpo_rto_measured"])
            self.assertEqual(result, bundle.verified_pair(dump, snapshot, pair_dir / bundle.NAME))
            metadata = (pair_dir / bundle.NAME).read_text(encoding="utf-8")
            self.assertNotIn(str(dump), metadata)
            self.assertNotIn("CREATE TABLE", metadata)
            self.assertEqual((pair_dir / bundle.NAME).stat().st_mode & 0o777, 0o600)

    def test_sql_or_private_corruption_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            dump, snapshot, pair_dir = offline_fixture(Path(directory))
            bundle.create_pair(dump, snapshot, pair_dir, "a" * 40)
            dump.write_bytes(b"-- corrupted dump\n")
            with self.assertRaises(ValueError):
                bundle.verified_pair(dump, snapshot, pair_dir / bundle.NAME)
            dump.write_bytes(b"-- disposable test fixture\nCREATE TABLE fixture (id INT);\n")
            (snapshot / "mfa.json").write_text("{}", encoding="utf-8")
            with self.assertRaises(ValueError):
                bundle.verified_pair(dump, snapshot, pair_dir / bundle.NAME)

    def test_rejects_public_dumps_symlinks_and_overwrites(self):
        with tempfile.TemporaryDirectory() as directory:
            dump, snapshot, pair_dir = offline_fixture(Path(directory))
            dump.chmod(0o644)
            with self.assertRaises(ValueError):
                bundle.create_pair(dump, snapshot, pair_dir, "a" * 40)
            dump.chmod(0o600)
            link = Path(directory) / "linked.sql"
            os.symlink(dump, link)
            with self.assertRaises(OSError):
                bundle.create_pair(link, snapshot, pair_dir, "a" * 40)
            bundle.create_pair(dump, snapshot, pair_dir, "a" * 40)
            with self.assertRaises(FileExistsError):
                bundle.create_pair(dump, snapshot, pair_dir, "a" * 40)


if __name__ == "__main__":
    unittest.main()
