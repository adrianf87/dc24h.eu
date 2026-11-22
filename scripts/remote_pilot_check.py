#!/usr/bin/env python3
"""Opt-in, no-credential HTTPS verification for a local SSH tunnel.

Never probes an Internet hostname, bypasses certificate verification, sends
cookies or exercises a mutable gateway route. Run with a deliberately trusted
local CA/certificate file on the pilot operator's workstation.
"""

import argparse
import http.client
from pathlib import Path
import ssl
import sys


class PilotCheckError(Exception):
    """An expected security invariant was not met."""


# Exact route list, never derived from untrusted host/path input.
CHECKS = (
    ("/remote.html", frozenset({200})),
    ("/remote/v1/session", frozenset({401})),
    ("/remote/v1/summary", frozenset({401})),
    ("/admin-api", frozenset({403, 404})),
    ("/index.html", frozenset({403, 404})),
)


def check_endpoints(context, connector=http.client.HTTPSConnection):
    """Check only public portal availability and unauthenticated denials.

    The transport is HTTPS to a fixed loopback address and port. The operator
    starts any SSH tunnel separately, after independently checking its host key.
    """
    context.check_hostname = True
    context.verify_mode = ssl.CERT_REQUIRED
    context.minimum_version = ssl.TLSVersion.TLSv1_2
    for path, acceptable in CHECKS:
        connection = connector("localhost", 8443, timeout=4, context=context)
        try:
            connection.request("GET", path, headers={"Accept": "text/html, application/json"})
            response = connection.getresponse()
            # Never follow a redirect, read private data or print a response body.
            if response.status not in acceptable:
                raise PilotCheckError(f"{path}: refused unexpected HTTP status {response.status}")
            if "no-store" not in (response.getheader("Cache-Control") or "").lower():
                raise PilotCheckError(f"{path}: missing no-store cache policy")
            if (response.getheader("X-Content-Type-Options") or "").lower() != "nosniff":
                raise PilotCheckError(f"{path}: missing nosniff header")
            # HTTP headers are sufficient for this trust-boundary smoke check.
        finally:
            connection.close()
    return len(CHECKS)


def main(argv=None):
    parser = argparse.ArgumentParser(description="Verify a LOCAL SSH-forwarded HTTPS read-only portal")
    parser.add_argument("--ca-file", type=Path, required=True,
                        help="Private, deliberately trusted CA or localhost server certificate file")
    args = parser.parse_args(argv)
    if not args.ca_file.is_file():
        print("Pilot denied: trusted HTTPS certificate file missing", file=sys.stderr)
        return 2
    try:
        context = ssl.create_default_context(cafile=str(args.ca_file))
        checks = check_endpoints(context)
    except (OSError, ValueError, ssl.SSLError, PilotCheckError):
        print("Pilot denied: HTTPS trust or read-only boundary check failed", file=sys.stderr)
        return 1
    print(f"Pilot preflight passed: {checks} HTTPS/loopback/read-only boundary checks")
    return 0


if __name__ == "__main__":
    sys.exit(main())
