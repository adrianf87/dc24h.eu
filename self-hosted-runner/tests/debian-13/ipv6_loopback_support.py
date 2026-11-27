"""Explicit IPv4/IPv6 coverage policy for isolated process tests.

An IPv4-only network namespace cannot bind ::1. Keep the IPv4 assertions
active while reporting missing IPv6 coverage, rather than treating a missing
interface as a failure of ADC CTM/RCM routing.
"""

import errno
import os
import socket
import sys


def ipv6_loopback_available():
    if not socket.has_ipv6:
        return False
    try:
        with socket.socket(socket.AF_INET6, socket.SOCK_STREAM) as probe:
            probe.bind(('::1', 0))
    except OSError as error:
        if error.errno in (errno.EADDRNOTAVAIL, errno.EAFNOSUPPORT,
                           errno.EPROTONOSUPPORT):
            return False
        raise
    return True


def local_ip_families():
    """Return tested families; report any omitted IPv6 subcases loudly.

    Set DC24H_REQUIRE_IPV6_LOOPBACK=1 to make full dual-stack qualification
    fail closed on a host without a working ::1 binding.
    """
    if ipv6_loopback_available():
        return (False, True)
    if os.environ.get('DC24H_REQUIRE_IPV6_LOOPBACK') == '1':
        raise RuntimeError('IPv6 ::1 is unavailable: strict dual-stack test coverage required')
    print('SKIP IPv6 CTM/RCM subcases: cannot bind ::1; IPv4 coverage only. '
          'Repeat with DC24H_REQUIRE_IPV6_LOOPBACK=1 on an IPv6-capable host '
          'before full dual-stack acceptance.', file=sys.stderr, flush=True)
    return (False,)
