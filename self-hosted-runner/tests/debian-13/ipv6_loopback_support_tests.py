"""Unit tests for honest IPv6-coverage reporting in process tests."""

import errno
import io
import os
import unittest
from contextlib import redirect_stderr
from unittest import mock

import ipv6_loopback_support as policy


class Ipv6LoopbackSupportTests(unittest.TestCase):
    @mock.patch.object(policy, 'ipv6_loopback_available', return_value=True)
    def test_dual_stack_runs_both_families(self, _):
        self.assertEqual(policy.local_ip_families(), (False, True))

    @mock.patch.object(policy, 'ipv6_loopback_available', return_value=False)
    def test_ipv4_only_is_reported_not_hidden(self, _):
        message = io.StringIO()
        with mock.patch.dict(os.environ, {'DC24H_REQUIRE_IPV6_LOOPBACK': '0'}), redirect_stderr(message):
            self.assertEqual(policy.local_ip_families(), (False,))
        self.assertIn('SKIP IPv6', message.getvalue())
        self.assertIn('IPv4 coverage only', message.getvalue())

    @mock.patch.object(policy, 'ipv6_loopback_available', return_value=False)
    def test_strict_mode_fails_when_ipv6_is_unavailable(self, _):
        with mock.patch.dict(os.environ, {'DC24H_REQUIRE_IPV6_LOOPBACK': '1'}):
            with self.assertRaisesRegex(RuntimeError, 'strict dual-stack'):
                policy.local_ip_families()

    @mock.patch.object(policy.socket, 'has_ipv6', False)
    def test_no_ipv6_socket_support(self):
        self.assertFalse(policy.ipv6_loopback_available())

    @mock.patch.object(policy.socket, 'socket')
    def test_expected_bind_failure_means_no_ipv6(self, socket_factory):
        socket_factory.return_value.__enter__.return_value.bind.side_effect = OSError(
            errno.EADDRNOTAVAIL, 'Address not available')
        self.assertFalse(policy.ipv6_loopback_available())

    @mock.patch.object(policy.socket, 'socket')
    def test_unexpected_bind_failure_is_not_swallowed(self, socket_factory):
        socket_factory.return_value.__enter__.return_value.bind.side_effect = OSError(
            errno.EPERM, 'Unexpected permission failure')
        with self.assertRaises(OSError):
            policy.ipv6_loopback_available()


if __name__ == '__main__':
    unittest.main()
