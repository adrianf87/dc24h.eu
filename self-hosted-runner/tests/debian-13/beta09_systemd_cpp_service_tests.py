"""Deterministic negative probes for actual C++ systemd property attestations."""

import os
import unittest
from unittest import mock

import beta09_systemd_cpp_service as fixture
from beta09_systemd_cpp_service import require_sandbox_properties


class SystemdSandboxEvidenceTests(unittest.TestCase):
    def test_transient_service_requires_exact_debian13_runner(self):
        release = {"ID": "debian", "VERSION_ID": "13"}
        runner = type("Runner", (), {"pw_name": "github-runner"})()
        env = {"GITHUB_ACTIONS": "true", "RUNNER_ENVIRONMENT": "self-hosted",
               "RUNNER_OS": "Linux"}
        with mock.patch.dict(os.environ, env), \
             mock.patch.object(fixture.platform, "freedesktop_os_release", return_value=release), \
             mock.patch.object(fixture.socket, "gethostname", return_value="server-747585786.dc24h.eu"), \
             mock.patch.object(fixture.pwd, "getpwuid", return_value=runner), \
             mock.patch.object(fixture.shutil, "which", return_value="/usr/bin/tool"):
            fixture.require_debian13_ci_runner()
            for changed in ({"ID": "ubuntu", "VERSION_ID": "13"},
                            {"ID": "debian", "VERSION_ID": "12"}):
                with self.subTest(os_release=changed), \
                     mock.patch.object(fixture.platform, "freedesktop_os_release", return_value=changed), \
                     self.assertRaises(AssertionError):
                    fixture.require_debian13_ci_runner()
            with mock.patch.object(fixture.socket, "gethostname", return_value="other-runner"), \
                 self.assertRaises(AssertionError):
                fixture.require_debian13_ci_runner()
            with mock.patch.object(fixture.pwd, "getpwuid", return_value=type("U", (), {"pw_name": "other"})()), \
                 self.assertRaises(AssertionError):
                fixture.require_debian13_ci_runner()
            with mock.patch.dict(os.environ, {"RUNNER_ENVIRONMENT": "github-hosted"}), \
                 self.assertRaises(AssertionError):
                fixture.require_debian13_ci_runner()
            with mock.patch.dict(os.environ, {"GITHUB_ACTIONS": "false"}), \
                 self.assertRaises(AssertionError):
                fixture.require_debian13_ci_runner()
            with mock.patch.object(fixture.shutil, "which", return_value=None), \
                 self.assertRaises(AssertionError):
                fixture.require_debian13_ci_runner()

    def test_effective_unprivileged_unix_only_service_is_required(self):
        fields = {
            "User": "ci-runner", "PrivateNetwork": "yes", "NoNewPrivileges": "yes",
            "ProtectSystem": "strict", "ProtectHome": "yes",
            "ProtectKernelTunables": "yes", "ProtectControlGroups": "yes",
            "RestrictAddressFamilies": "AF_UNIX",
        }

        def invoke(args):
            self.assertEqual(args[:2], ["systemctl", "show"])
            self.assertEqual(args[3:], ["--value", "disposable.service"])
            key = args[2].split("=", 1)[1]
            return type("Result", (), {"stdout": fields[key].encode("ascii")})()

        require_sandbox_properties("disposable.service", "ci-runner", invoke)
        for property_name, weakened in (("User", "root"), ("PrivateNetwork", "no"),
                                        ("ProtectHome", "no"),
                                        ("RestrictAddressFamilies", "AF_UNIX AF_INET")):
            with self.subTest(property_name=property_name):
                original = fields[property_name]
                fields[property_name] = weakened
                try:
                    with self.assertRaisesRegex(AssertionError, "systemd"):
                        require_sandbox_properties("disposable.service", "ci-runner", invoke)
                finally:
                    fields[property_name] = original


if __name__ == "__main__":
    unittest.main()
