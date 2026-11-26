#!/usr/bin/env python3
"""Strict endpoint and no-plaintext regression checks without networking."""

import unittest
from unittest.mock import MagicMock, patch

import public_stage3_external_probe as probe


class StageIIIExternalProbeTests(unittest.TestCase):
    def test_exact_fqdn_and_ipv4_are_fixed(self):
        self.assertEqual(probe.FQDN, "server-747585786.dc24h.eu")
        self.assertEqual(probe.IPV4, "62.171.156.32")
        self.assertEqual(probe.ADCS_PORT, 1512)
        self.assertEqual(probe.PLAINTEXT_ADC_PORT, 1511)

    @patch.object(probe.socket, "getaddrinfo")
    def test_external_dns_rejects_additional_ipv4(self, resolve):
        resolve.return_value = [(None, None, None, None, ("62.171.156.32", 1512)),
                                (None, None, None, None, ("203.0.113.1", 1512))]
        with self.assertRaises(RuntimeError):
            probe.assert_dns()

    @patch.object(probe.socket, "getaddrinfo")
    def test_external_dns_accepts_only_exact_ipv4(self, resolve):
        resolve.return_value = [(None, None, None, None, ("62.171.156.32", 1512))]
        probe.assert_dns()

    @patch.object(probe.socket, "create_connection")
    def test_plaintext_adc_reachable_is_rejected(self, connect):
        connect.return_value = MagicMock()
        with self.assertRaises(RuntimeError):
            probe.assert_plaintext_closed()

    @patch.object(probe.socket, "create_connection", side_effect=ConnectionRefusedError)
    def test_plaintext_adc_refused_is_accepted(self, _connect):
        probe.assert_plaintext_closed()

    def test_bounded_line_parser(self):
        peer = MagicMock()
        peer.recv.return_value = b"ISUP ADBASE ADTIGR\nISID ABCD\n"
        line, remaining = probe.recv_line(peer, b"")
        self.assertEqual(line, b"ISUP ADBASE ADTIGR")
        self.assertEqual(remaining, b"ISID ABCD\n")


if __name__ == "__main__":
    unittest.main()
