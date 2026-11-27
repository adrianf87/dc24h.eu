"""Deterministic Fix Pass 3 path-swap and partial-capture negatives.

Only owner-private synthetic directories are opened; no MariaDB service,
operator credentials or production package is touched.
"""

import os
from pathlib import Path
import stat
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import beta08_recovery_auth as auth
import stage1_recovery_bundle as pair
import stage4_private_recovery_drill as private
from stage1_recovery_bundle_tests import offline_fixture


class FixPassThreeRecoveryTests(unittest.TestCase):
    def test_parent_alias_rejected_before_read_and_before_output_creation(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            real = root / "real"
            real.mkdir(mode=0o700)
            state = real / "state"
            state.mkdir(mode=0o700)
            private.write_private(state, "input", b"synthetic\n")
            moved = root / "moved"
            swapped = False
            original_open = os.open

            def replace_ancestor_at_open(path, flags, *args, **kwargs):
                nonlocal swapped
                if not swapped and Path(path) == state and flags & os.O_DIRECTORY:
                    real.rename(moved)
                    real.symlink_to(moved, target_is_directory=True)
                    swapped = True
                return original_open(path, flags, *args, **kwargs)

            with mock.patch.object(private.os, "open", side_effect=replace_ancestor_at_open):
                with self.assertRaisesRegex(ValueError, "Symlink"):
                    private.read_private(state, "input", 128)
            self.assertTrue(swapped)
            self.assertEqual((moved / "state" / "input").read_bytes(), b"synthetic\n")
            with self.assertRaisesRegex(ValueError, "Symlink"):
                private.write_private(state, "must-not-exist", b"test")
            self.assertFalse((moved / "state" / "must-not-exist").exists())

    def test_traversal_alias_and_final_symlink_refused(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            state = root / "state"
            state.mkdir(mode=0o700)
            private.write_private(state, "input", b"example\n")
            with self.assertRaisesRegex(ValueError, "Noncanonical"):
                private.private_directory(state / ".." / "state")
            (state / "alias").symlink_to(state / "input")
            with self.assertRaises(OSError):
                private.read_private(state, "alias", 128)
            with self.assertRaises(ValueError):
                private.read_private(state, "../input", 128)

    def test_offline_sql_hash_rejects_same_size_inode_swap_during_read(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            dump = root / "state.sql"
            dump.write_bytes(b"original synthetic SQL fixture\n")
            dump.chmod(0o600)
            replacement = root / "new.sql"
            replacement.write_bytes(dump.read_bytes())
            replacement.chmod(0o600)
            original_read = os.read
            swapped = False

            def replace_source_after_first_read(fd, length):
                nonlocal swapped
                content = original_read(fd, length)
                if not swapped:
                    replacement.replace(dump)
                    swapped = True
                return content

            with mock.patch.object(pair.os, "read", side_effect=replace_source_after_first_read):
                with self.assertRaisesRegex(ValueError, "changed during hashing"):
                    pair.private_hash(dump, pair.MAX_DUMP)
            self.assertTrue(swapped)

    def test_offline_sql_hash_rejects_in_place_rewrite_with_frozen_file_metadata(self):
        with tempfile.TemporaryDirectory() as directory:
            dump = Path(directory) / "state.sql"
            dump.write_bytes(b"original synthetic SQL fixture\n")
            dump.chmod(0o600)
            frozen = dump.stat()
            original_read, original_fstat = os.read, os.fstat
            original_stat = os.stat
            changed = False

            def frozen_fstat(fd):
                metadata = original_fstat(fd)
                return frozen if stat.S_ISREG(metadata.st_mode) else metadata

            def frozen_stat(path, *args, **kwargs):
                if path == dump.name and kwargs.get("dir_fd") is not None:
                    return frozen
                return original_stat(path, *args, **kwargs)

            def rewrite_after_first_read(fd, length):
                nonlocal changed
                content = original_read(fd, length)
                if not changed:
                    dump.write_bytes(b"modified synthetic SQL fixture\n")
                    changed = True
                return content

            with mock.patch.object(pair.os, "fstat", side_effect=frozen_fstat):
                with mock.patch.object(pair.os, "stat", side_effect=frozen_stat):
                    with mock.patch.object(pair.os, "read", side_effect=rewrite_after_first_read):
                        with self.assertRaisesRegex(ValueError, "changed during hashing"):
                            pair.private_hash(dump, pair.MAX_DUMP)
            self.assertTrue(changed)

    def test_anti_reuse_ledger_replacement_before_consume_is_refused(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            dump, snapshot, pair_dir = offline_fixture(root)
            pair.create_pair(dump, snapshot, pair_dir, "a" * 40)
            trust = root / "trust"
            trust.mkdir(mode=0o700)
            key, ledger = trust / "key", trust / "ledger"
            private.write_private(trust, "key", os.urandom(32))
            auth.initialize_ledger(ledger)
            detached = root / "auth"
            detached.mkdir(mode=0o700)
            pinned = auth.sign(pair_dir, key, detached)
            replacement = trust / "replacement"
            private.write_private(trust, "replacement", b"")
            original_stat = os.stat
            swapped = False

            def replace_ledger_before_identity_check(path, *args, **kwargs):
                nonlocal swapped
                if path == ledger.name and kwargs.get("dir_fd") is not None and not swapped:
                    replacement.replace(ledger)
                    swapped = True
                return original_stat(path, *args, **kwargs)

            with mock.patch.object(auth.os, "stat", side_effect=replace_ledger_before_identity_check):
                with self.assertRaisesRegex(ValueError, "ledger replaced"):
                    auth.verify(dump, snapshot, pair_dir, "a" * 40, detached,
                                key, pinned, ledger, consume=True)
            self.assertTrue(swapped)
            self.assertEqual(ledger.read_bytes(), b"")

    def test_anti_reuse_ledger_replacement_after_append_is_not_acknowledged(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            dump, snapshot, pair_dir = offline_fixture(root)
            pair.create_pair(dump, snapshot, pair_dir, "a" * 40)
            trust = root / "trust"
            trust.mkdir(mode=0o700)
            key, ledger = trust / "key", trust / "ledger"
            private.write_private(trust, "key", os.urandom(32))
            auth.initialize_ledger(ledger)
            detached = root / "auth"
            detached.mkdir(mode=0o700)
            pinned = auth.sign(pair_dir, key, detached)
            replacement = trust / "replacement"
            private.write_private(trust, "replacement", b"")
            original_write = os.write
            swapped = False

            def replace_ledger_after_write(fd, data):
                nonlocal swapped
                written = original_write(fd, data)
                if not swapped and len(data) == 65 and data.endswith(b"\n"):
                    replacement.replace(ledger)
                    swapped = True
                return written

            with mock.patch.object(auth.os, "write", side_effect=replace_ledger_after_write):
                with self.assertRaisesRegex(ValueError, "replaced during append"):
                    auth.verify(dump, snapshot, pair_dir, "a" * 40, detached,
                                key, pinned, ledger, consume=True)
            self.assertTrue(swapped)
            self.assertEqual(ledger.read_bytes(), b"")


if __name__ == "__main__":
    unittest.main()
