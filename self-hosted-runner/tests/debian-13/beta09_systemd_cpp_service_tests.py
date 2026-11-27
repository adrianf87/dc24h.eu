"""Deterministic negative probes for actual C++ systemd property attestations."""

import unittest

from beta09_systemd_cpp_service import require_sandbox_properties


class SystemdSandboxEvidenceTests(unittest.TestCase):
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
