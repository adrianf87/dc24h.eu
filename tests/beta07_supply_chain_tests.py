"""Supply-chain fail-closed checks without network or secret-containing logs."""

import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import beta07_supply_chain as supply


ROOT = Path(__file__).resolve().parents[1]
HEAD = subprocess.check_output(["git", "-C", str(ROOT), "rev-parse", "HEAD"]).decode().strip()


class SupplyChainTests(unittest.TestCase):
    def test_exact_source_lock_inventory_and_action_pins(self):
        data = supply.inventory(ROOT, HEAD, "ubuntu-24.04", HEAD)
        self.assertGreater(data["npm_registry_integrity_entries"], 10)
        self.assertGreater(data["npm_direct_dependencies"], 10)
        self.assertGreaterEqual(len(data["pinned_actions"]), 3)
        self.assertEqual(data["npm_transitive_locked_packages"], len(data["sbom_components"]))
        self.assertTrue(all(set(row) == {"package", "version", "integrity_sha256"}
                            for row in data["sbom_components"]))
        self.assertFalse(data["production_release_approved"])
        self.assertNotIn("password", json.dumps(data).lower())
        self.assertNotIn("registry.npmjs.org", json.dumps(data))

    def test_wrong_head_image_or_unpinned_action_fail_closed(self):
        with self.assertRaises(ValueError):
            supply.inventory(ROOT, "b" * 40 if HEAD != "b" * 40 else "a" * 40,
                             "ubuntu-24.04", HEAD)
        with self.assertRaises(ValueError):
            supply.inventory(ROOT, HEAD, "Debian", HEAD)
        with tempfile.TemporaryDirectory() as directory:
            new = Path(directory)
            (new / "webadmin").mkdir()
            (new / ".github/workflows").mkdir(parents=True)
            (new / "webadmin/package-lock.json").write_bytes((ROOT / "webadmin/package-lock.json").read_bytes())
            (new / "webadmin/package.json").write_bytes((ROOT / "webadmin/package.json").read_bytes())
            (new / ".github/workflows/unsafe.yml").write_text("steps:\n  - uses: actions/checkout@v5\n")
            with self.assertRaisesRegex(ValueError, "Unpinned"):
                supply.inventory(new, HEAD, "ubuntu-22.04", HEAD)

    def test_missing_package_integrity_cannot_be_disguised_as_an_omitted_entry(self):
        with tempfile.TemporaryDirectory() as directory:
            new = Path(directory)
            (new / "webadmin").mkdir()
            (new / ".github/workflows").mkdir(parents=True)
            lock = json.loads((ROOT / "webadmin/package-lock.json").read_text())
            target = next(name for name in lock["packages"] if name.startswith("node_modules/"))
            del lock["packages"][target]["integrity"]
            (new / "webadmin/package-lock.json").write_text(json.dumps(lock))
            (new / "webadmin/package.json").write_bytes((ROOT / "webadmin/package.json").read_bytes())
            (new / ".github/workflows/pinned.yml").write_text(
                "steps:\n  - uses: actions/checkout@" + "a" * 40 + "\n")
            with self.assertRaisesRegex(ValueError, "Missing authenticated npm"):
                supply.inventory(new, HEAD, "ubuntu-24.04", HEAD)

    def test_yaml_extension_and_root_lock_version_drift_are_not_invisible(self):
        with tempfile.TemporaryDirectory() as directory:
            new = Path(directory)
            (new / "webadmin").mkdir()
            (new / ".github/workflows").mkdir(parents=True)
            original = json.loads((ROOT / "webadmin/package-lock.json").read_text())
            (new / "webadmin/package.json").write_bytes((ROOT / "webadmin/package.json").read_bytes())
            (new / "webadmin/package-lock.json").write_text(json.dumps(original))
            (new / ".github/workflows/pinned.yml").write_text(
                "steps:\n  - uses: actions/checkout@" + "a" * 40 + "\n")
            (new / ".github/workflows/unpinned.yaml").write_text(
                "steps:\n  - uses: actions/cache@v4\n")
            with self.assertRaisesRegex(ValueError, "Unpinned"):
                supply.inventory(new, HEAD, "ubuntu-24.04", HEAD)
            (new / ".github/workflows/unpinned.yaml").unlink()
            original["packages"][""]["version"] = "0.6.20"
            (new / "webadmin/package-lock.json").write_text(json.dumps(original))
            with self.assertRaisesRegex(ValueError, "Inconsistent npm"):
                supply.inventory(new, HEAD, "ubuntu-24.04", HEAD)


if __name__ == "__main__":
    unittest.main()
