"""Synthetic Ubuntu APT/build identity negatives; not a hosted runner result."""

from pathlib import Path
import sys
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import beta09_os_provenance as gate


SHA = "a" * 40
IMAGE = "ubuntu-24.04"
ENV = {"GITHUB_ACTIONS": "true", "RUNNER_ENVIRONMENT": "github-hosted", "RUNNER_OS": "Linux"}
APT = "".join(f"{name}:amd64\t1.2.3-1ubuntu2\n" for name in gate.REQUIRED_APT)


def inventory(_root, _sha, _image, _checked):
    return {"npm_lock_sha256": "b" * 64, "npm_transitive_locked_packages": 100,
            "pinned_actions": ["actions/checkout@" + "c" * 40]}


class HostedPackageIdentityTests(unittest.TestCase):
    def qualified(self, **replacements):
        args = dict(root=Path("/synthetic"), sha=SHA, checked_out=SHA, image=IMAGE,
                    os_version="24.04", environment=ENV, apt_raw=APT,
                    compiler="13.3.0", node="v22.14.0", npm="10.9.0", cmake="3.28.3")
        args.update(replacements)
        with mock.patch.object(gate.supply, "inventory", side_effect=inventory):
            return gate.evaluate(**args)

    def test_bounded_public_inventory_binds_toolchain_and_locked_actions(self):
        report = self.qualified()
        self.assertEqual(report["os_image"], IMAGE)
        self.assertEqual(len(report["apt_full_installed_inventory_sha256"]), 64)
        self.assertEqual(set(report["selected_public_apt_versions"]), set(gate.REQUIRED_APT))
        self.assertEqual(report["pinned_actions_count"], 1)
        self.assertFalse(report["fully_reproducible_os_build"])
        self.assertFalse(report["owner_release_approval"])

    def test_wrong_head_image_host_or_unpinned_versions_are_rejected(self):
        for changes in ({"checked_out": "b" * 40}, {"os_version": "22.04"},
                        {"environment": {**ENV, "RUNNER_ENVIRONMENT": "private"}},
                        {"compiler": "unknown"}, {"node": ""},
                        {"npm": "latest"}, {"cmake": "unknown"}):
            with self.subTest(changes=changes), self.assertRaises(ValueError):
                self.qualified(**changes)

    def test_missing_duplicate_or_forged_apt_components_fail(self):
        for body in (APT.replace("ncdc:amd64\t1.2.3-1ubuntu2\n", ""),
                     APT + "g++:amd64\t1.2.3-1ubuntu2\n",
                     APT.replace("libssl-dev", "../libssl-dev"),
                     APT.replace("cmake:amd64\t1.2.3-1ubuntu2", "cmake:amd64\tunstable version"),
                     APT + "g++:i386\t1.2.3\n"):
            with self.subTest(apt=body[-30:]), self.assertRaises(ValueError):
                self.qualified(apt_raw=body)


if __name__ == "__main__":
    unittest.main()
