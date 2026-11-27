"""Offline checks for checked-in sanitized Debian package inventories."""
from __future__ import annotations

import hashlib
import pathlib
import re
import unittest


RUNNER_ROOT = pathlib.Path(__file__).resolve().parents[1]
CHECKSUM_RE = re.compile(r"^([0-9a-f]{64})  ([A-Za-z0-9_.-]+)$")
REQUIRED_FILES = {
    "system.txt",
    "installed-packages.tsv",
    "apt-manual.txt",
    "dpkg-package-events.tsv",
    "removed-not-installed.tsv",
    "tool-versions.tsv",
    "inventory-warnings.txt",
}


class RunnerInventoryTests(unittest.TestCase):
    def test_snapshot_integrity(self) -> None:
        inventory_dir = RUNNER_ROOT / "27112026" / "inventory"
        snapshots = sorted(
            path for path in inventory_dir.iterdir()
            if path.is_dir() and re.fullmatch(r"\d{8}T\d{6}Z", path.name)
        )
        if not snapshots:
            self.skipTest("No complete inventory imported yet; NOT a host-inventory PASS")
        for snapshot in snapshots:
            with self.subTest(snapshot=snapshot.name):
                checksum_file = snapshot / "SHA256SUMS"
                self.assertTrue(checksum_file.is_file(), "Missing SHA256SUMS")
                observed = set()
                for line in checksum_file.read_text(encoding="utf-8").splitlines():
                    match = CHECKSUM_RE.fullmatch(line)
                    self.assertIsNotNone(match, "Malformed SHA256SUMS entry")
                    expected_digest, filename = match.groups()
                    observed.add(filename)
                    data = (snapshot / filename).read_bytes()
                    self.assertEqual(
                        hashlib.sha256(data).hexdigest(),
                        expected_digest,
                        f"Checksum mismatch: {snapshot.name}/{filename}",
                    )
                self.assertEqual(observed, REQUIRED_FILES)
                packages = (snapshot / "installed-packages.tsv").read_text(
                    encoding="utf-8"
                ).splitlines()
                self.assertTrue(packages, "Installed package inventory is empty")
                self.assertTrue(
                    all("\t" in row for row in packages),
                    "Package rows must include name and version",
                )


if __name__ == "__main__":
    unittest.main()
