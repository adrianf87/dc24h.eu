"""Local read-only inspection of inert .deb layout and refused overwrites."""

import hashlib
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import beta07_ci_deb as deb


class DebPrototypeTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which("dpkg-deb") and shutil.which("dpkg"), "requires Debian dpkg tools")
    def test_distinct_legacy_to_twilight_packages_refuse_implicit_replacement(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            old = root / "old"; old.write_bytes(b"#!/bin/sh\necho old\n"); old.chmod(0o700)
            new = root / "new"; new.write_bytes(b"#!/bin/sh\necho new\n"); new.chmod(0o700)
            if subprocess.check_output(["dpkg", "--print-architecture"]).strip() != b"amd64":
                self.skipTest("CI fixture architecture is amd64")
            legacy = deb.build(old, root / "legacy-stage", deb.VERSIONS["previous"], "a" * 40,
                               account_admin=old, legacy=True)
            twilight = deb.build(new, root / "twilight-stage", deb.VERSIONS["current"], "b" * 40,
                                 account_admin=new)
            for archive, name, conflict, bin_path, admin_path, unit_path, doc_path, contents in (
                    (legacy, deb.LEGACY_PACKAGE, deb.PACKAGE, deb.LEGACY_BIN,
                     deb.LEGACY_ADMIN_BIN, deb.LEGACY_UNIT_PATH, deb.LEGACY_DOC_PATH, old.read_bytes()),
                    (twilight, deb.PACKAGE, deb.LEGACY_PACKAGE, deb.BIN,
                     deb.ADMIN_BIN, deb.UNIT_PATH, deb.DOC_PATH, new.read_bytes())):
                control = subprocess.check_output(["dpkg-deb", "--field", str(archive)]).decode()
                self.assertIn("Package: " + name + "\n", control)
                self.assertIn("Conflicts: " + conflict + "\n", control)
                self.assertNotIn("Replaces:", control)
                self.assertNotIn("Provides:", control)
                self.assertNotIn("Breaks:", control)
                self.assertNotIn(b"preinst", subprocess.check_output([
                    "dpkg-deb", "--ctrl-tarfile", str(archive)]))
                self.assertNotIn(b"postinst", subprocess.check_output([
                    "dpkg-deb", "--ctrl-tarfile", str(archive)]))
                extracted = root / (name + "-extract")
                subprocess.run(["dpkg-deb", "--extract", str(archive), str(extracted)], check=True)
                self.assertEqual((extracted / bin_path).read_bytes(), contents)
                self.assertEqual((extracted / admin_path).read_bytes(), contents)
                unit = (extracted / unit_path).read_text(encoding="ascii")
                self.assertIn("ConditionPathExists=/__dc24h_ci_fixture_never_start__", unit)
                self.assertIn("ExecStart=/" + bin_path + " --version", unit)
                self.assertNotIn("[Install]", unit)
                self.assertIn("source-sha=", (extracted / doc_path).read_text(encoding="ascii"))
            for unsupported in (deb.VERSIONS["current"], deb.FAILED_UPGRADE_VERSION):
                with self.assertRaises(ValueError):
                    deb.build(old, root / "rejected-legacy", unsupported, "a" * 40, legacy=True)
            with self.assertRaises(ValueError):
                deb.build(old, root / "rejected-legacy", deb.VERSIONS["previous"],
                          "a" * 40, legacy=True, fail_postinst=True)
            self.assertFalse((root / "rejected-legacy").exists())

    @unittest.skipUnless(shutil.which("dpkg-deb") and shutil.which("dpkg"), "requires Debian dpkg tools")
    def test_manifest_unit_ownership_and_reproducible_fixture_contents(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            binary = root / "fake-binary"
            binary.write_bytes(b"#!/bin/sh\necho test-only\n")
            binary.chmod(0o700)
            if subprocess.check_output(["dpkg", "--print-architecture"]).strip() != b"amd64":
                self.skipTest("CI fixture architecture is amd64")
            sha = "a" * 40
            archive = deb.build(binary, root / "stage", deb.VERSIONS["current"], sha)
            self.assertTrue(archive.is_file())
            self.assertIn(deb.PACKAGE, subprocess.check_output(["dpkg-deb", "--field", str(archive), "Package"]).decode())
            self.assertIn("Depends: libc6, libstdc++6", subprocess.check_output([
                "dpkg-deb", "--field", str(archive)]).decode())
            control = subprocess.check_output(["dpkg-deb", "--field", str(archive)]).decode()
            self.assertIn("Conflicts: " + deb.LEGACY_PACKAGE, control)
            self.assertNotIn("Replaces:", control, "CI fixture must not silently replace the legacy package")
            self.assertEqual(subprocess.check_output([
                "dpkg-deb", "--ctrl-tarfile", str(archive)]).count(b"#!/bin/sh"), 0)
            extracted = root / "extracted"
            subprocess.run(["dpkg-deb", "--extract", str(archive), str(extracted)], check=True,
                           stdout=subprocess.DEVNULL)
            self.assertEqual((extracted / deb.BIN).read_bytes(), binary.read_bytes())
            unit = (extracted / deb.UNIT_PATH).read_text(encoding="ascii")
            self.assertIn("ConditionPathExists=/__dc24h_ci_fixture_never_start__", unit)
            self.assertNotIn("WantedBy", unit)
            provenance = (extracted / deb.DOC_PATH).read_text(encoding="ascii")
            self.assertIn(f"source-sha={sha}", provenance)
            self.assertIn(f"binary-sha256={hashlib.sha256(binary.read_bytes()).hexdigest()}", provenance)
            with self.assertRaises(ValueError):
                deb.build(binary, root / "stage", deb.VERSIONS["current"], sha)

    @unittest.skipUnless(shutil.which("dpkg-deb") and shutil.which("dpkg"), "requires Debian dpkg tools")
    def test_valid_failed_upgrade_injects_only_preunpack_abort(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            binary = root / "fake-binary"
            binary.write_bytes(b"#!/bin/sh\necho test-only\n")
            binary.chmod(0o700)
            if subprocess.check_output(["dpkg", "--print-architecture"]).strip() != b"amd64":
                self.skipTest("CI fixture architecture is amd64")
            package = deb.build(binary, root / "failed-stage", deb.FAILED_UPGRADE_VERSION,
                                "b" * 40, fail_preinst=True)
            listing = subprocess.check_output(["dpkg-deb", "--ctrl-tarfile", str(package)])
            self.assertIn(b"preinst", listing)
            self.assertIn(b"#!/bin/sh\nexit 41\n", listing)
            self.assertEqual(subprocess.check_output([
                "dpkg-deb", "--field", str(package), "Version"]).decode().strip(),
                deb.FAILED_UPGRADE_VERSION)
            with self.assertRaises(ValueError):
                deb.build(binary, root / "unsafe", deb.FAILED_UPGRADE_VERSION, "b" * 40)
            with self.assertRaises(ValueError):
                deb.build(binary, root / "unsafe", deb.VERSIONS["current"], "b" * 40,
                          fail_preinst=True)
            self.assertFalse((root / "unsafe").exists())

    @unittest.skipUnless(shutil.which("dpkg-deb") and shutil.which("dpkg"), "requires Debian dpkg tools")
    def test_historical_maintenance_binary_is_packaged_and_hashed_independently(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            hub, maintenance = root / "hub", root / "maintenance"
            hub.write_bytes(b"#!/bin/sh\nexit 0\n")
            maintenance.write_bytes(b"#!/bin/sh\nexit 1\n")
            hub.chmod(0o700); maintenance.chmod(0o700)
            if subprocess.check_output(["dpkg", "--print-architecture"]).strip() != b"amd64":
                self.skipTest("CI fixture architecture is amd64")
            package = deb.build(hub, root / "previous", deb.VERSIONS["previous"],
                                "a" * 40, account_admin=maintenance)
            extracted = root / "extracted"
            subprocess.run(["dpkg-deb", "--extract", str(package), str(extracted)], check=True,
                           stdout=subprocess.DEVNULL)
            self.assertEqual((extracted / deb.ADMIN_BIN).read_bytes(), maintenance.read_bytes())
            provenance = (extracted / deb.DOC_PATH).read_text(encoding="ascii")
            self.assertIn("account-admin-sha256=" + hashlib.sha256(maintenance.read_bytes()).hexdigest(),
                          provenance)
            with self.assertRaises(ValueError):
                deb.build(hub, root / "rejected", deb.VERSIONS["previous"],
                          "a" * 40, account_admin=root / "missing")
            self.assertFalse((root / "rejected").exists())

    @unittest.skipUnless(shutil.which("dpkg-deb") and shutil.which("dpkg"), "requires Debian dpkg tools")
    def test_postunpack_failure_is_real_configuration_script_not_normal_package(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            binary = root / "fake-binary"
            binary.write_bytes(b"#!/bin/sh\necho synthetic\n")
            binary.chmod(0o700)
            if subprocess.check_output(["dpkg", "--print-architecture"]).strip() != b"amd64":
                self.skipTest("CI fixture architecture is amd64")
            archive = deb.build(binary, root / "post-stage", deb.FAILED_POSTINST_VERSION,
                                "b" * 40, fail_postinst=True)
            control = subprocess.check_output(["dpkg-deb", "--ctrl-tarfile", str(archive)])
            self.assertIn(b"postinst", control)
            self.assertIn(b"#!/bin/sh\nexit 42\n", control)
            with self.assertRaises(ValueError):
                deb.build(binary, root / "invalid-stage", deb.FAILED_POSTINST_VERSION, "b" * 40)
            with self.assertRaises(ValueError):
                deb.build(binary, root / "invalid-stage", deb.FAILED_POSTINST_VERSION,
                          "b" * 40, fail_postinst=True, fail_preinst=True)
            self.assertFalse((root / "invalid-stage").exists())

    def test_refuses_unknown_version_sha_and_missing_binary_without_writes(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for version, sha in [("0.7.99", "a" * 40), (deb.VERSIONS["previous"], "no-sha")]:
                with self.assertRaises(ValueError):
                    deb.build(root / "missing", root / "staging", version, sha)
            self.assertFalse((root / "staging").exists())


if __name__ == "__main__":
    unittest.main()
