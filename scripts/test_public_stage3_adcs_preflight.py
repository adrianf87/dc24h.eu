#!/usr/bin/env python3
"""Fast unit tests for bounded, read-only Stage III checks."""

import subprocess
import unittest

import public_stage3_adcs_preflight as gate


class StageIIIReadOnlyTests(unittest.TestCase):
    def test_dns_requires_exact_ipv4(self):
        answer = subprocess.CompletedProcess(
            [], 0, "62.171.156.32 STREAM fqdn\n62.171.156.32 DGRAM\n", ""
        )
        self.assertTrue(gate.dns_contains_target(answer, "62.171.156.32"))
        self.assertFalse(gate.dns_contains_target(answer, "62.171.156.3"))
        self.assertFalse(gate.dns_contains_target(None, "62.171.156.32"))

    def test_interface_requires_exact_cidr_address(self):
        answer = subprocess.CompletedProcess(
            [], 0, "2: ens3 inet 62.171.156.32/24 brd 62.171.156.255 scope global ens3\n", ""
        )
        self.assertTrue(gate.direct_ipv4_present(answer, "62.171.156.32"))
        self.assertFalse(gate.direct_ipv4_present(answer, "62.171.156.3"))

    def test_listening_port_detection_is_state_bounded(self):
        tcp = (
            "  sl  local_address rem_address   st tx_queue\n"
            "   0: 00000000:05E8 00000000:0000 0A 00000000\n"
            "   1: 00000000:05E8 00000000:0000 01 00000000\n"
        )
        self.assertTrue(gate.listening_tcp_port(tcp, 1512))
        self.assertFalse(gate.listening_tcp_port(tcp, 1511))
        self.assertFalse(gate.listening_tcp_port("  sl local_address rem_address st\n", 1512))

    def test_does_not_accept_failed_dns_or_interface_commands(self):
        failed = subprocess.CompletedProcess([], 1, "62.171.156.32 STREAM\n", "")
        self.assertFalse(gate.dns_contains_target(failed, "62.171.156.32"))
        self.assertFalse(gate.direct_ipv4_present(failed, "62.171.156.32"))


if __name__ == "__main__":
    unittest.main()
