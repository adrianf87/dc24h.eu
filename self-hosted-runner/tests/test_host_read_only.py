"""Manual read-only checks: run ONLY on the approved Debian 13 VPS as github-runner.

No package installation, service mutation, database access or outbound requests.
"""
from __future__ import annotations

import os
import pathlib
import pwd
import socket
import subprocess
import unittest


EXPECTED_HOSTNAME = "server-747585786"
EXPECTED_ADCS_IP = "62.171.156.32"


def run_read_only(*command: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        list(command),
        capture_output=True,
        text=True,
        timeout=15,
        check=False,
    )


class Debian13RunnerHostTests(unittest.TestCase):
    def test_expected_hostname_and_debian_13(self) -> None:
        self.assertEqual(socket.gethostname().split(".")[0], EXPECTED_HOSTNAME)
        release = pathlib.Path("/etc/os-release").read_text(encoding="utf-8")
        self.assertRegex(release, r'(?m)^VERSION_ID="?13"?$')
        self.assertRegex(release, r"(?m)^VERSION_CODENAME=trixie$")

    def test_running_as_registered_service_account(self) -> None:
        runner = pwd.getpwnam("github-runner")
        self.assertEqual(os.geteuid(), runner.pw_uid)
        self.assertNotEqual(os.geteuid(), 0, "Do not run runner tests as root")

    def test_passwordless_sudo_does_not_require_prompt(self) -> None:
        # A harmless root-command capability probe; does not change host state.
        result = run_read_only("sudo", "-n", "/usr/bin/true")
        self.assertEqual(result.returncode, 0, "Passwordless sudo capability not available")

    def test_live_service_state(self) -> None:
        services = (
            "dc24h-stage3-adcs.service",
            "dc24h-twilight-stage4-auth.service",
            "dc24h-twilight-stage4-bridge.service",
            "dc24h-twilight-stage4-webadmin.service",
        )
        for service in services:
            with self.subTest(service=service):
                result = run_read_only("systemctl", "is-active", service)
                self.assertEqual(result.stdout.strip(), "active", f"{service} inactive")

    def test_required_listeners_and_forbidden_adc(self) -> None:
        result = run_read_only("ss", "-H", "-lnt")
        self.assertEqual(result.returncode, 0, "Could not inspect TCP listeners")
        listeners: dict[int, set[str]] = {}
        for row in result.stdout.splitlines():
            fields = row.split()
            if len(fields) < 4:
                continue
            address = fields[3]
            host, separator, port_text = address.rpartition(":")
            if separator and port_text.isdecimal():
                listeners.setdefault(int(port_text), set()).add(host)

        self.assertIn(1512, listeners, "Public ADCS TCP 1512 missing")
        self.assertIn(EXPECTED_ADCS_IP, listeners[1512], "ADCS not bound to approved IPv4")
        self.assertIn(7278, listeners, "WebAdmin HTTPS TCP 7278 missing")
        self.assertNotIn(1511, listeners, "Forbidden legacy ADC TCP 1511 is listening")
        for loopback_port in (4278, 3306):
            with self.subTest(port=loopback_port):
                self.assertIn(loopback_port, listeners, "Required private service missing")
                self.assertTrue(
                    listeners[loopback_port].issubset({"127.0.0.1", "[::1]"}),
                    f"TCP {loopback_port} exposed outside loopback",
                )


if __name__ == "__main__":
    unittest.main()
