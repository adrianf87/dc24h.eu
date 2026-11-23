"""Offline safety tests for the optional SSH-forwarded read-only pilot."""

import importlib.util
from pathlib import Path
import ssl
import unittest


SOURCE = Path(__file__).resolve().parents[1] / "scripts" / "remote_pilot_check.py"
spec = importlib.util.spec_from_file_location("remote_pilot_check", SOURCE)
pilot = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pilot)


class Response:
    def __init__(self, status, headers=None):
        self.status = status
        self.headers = headers if headers is not None else {
            "Cache-Control": "no-store", "X-Content-Type-Options": "nosniff"}

    def getheader(self, name):
        return self.headers.get(name)


class Connection:
    seen = []
    statuses = {
        "/remote.html": 200,
        "/remote/v1/session": 401,
        "/remote/v1/summary": 401,
        "/admin-api": 403,
        "/index.html": 403,
    }
    headers_override = None

    def __init__(self, host, port, timeout, context):
        if host != "localhost" or port != 8443 or timeout != 4:
            raise AssertionError("Pilot traffic escaped the fixed local HTTPS origin")
        self.path = None

    def request(self, method, path, headers):
        if method != "GET" or list(headers) != ["Accept"]:
            raise AssertionError("Pilot request is not credential-free/read-only")
        self.path = path
        self.seen.append(path)

    def getresponse(self):
        headers = self.headers_override
        if headers is None:
            headers = {"Cache-Control": "no-store", "X-Content-Type-Options": "nosniff"}
            if self.path == "/remote.html":
                headers["Content-Type"] = "text/html; charset=utf-8"
        return Response(self.statuses[self.path], headers)

    def close(self):
        pass


class PilotTests(unittest.TestCase):
    def setUp(self):
        Connection.seen = []
        Connection.statuses = {
            "/remote.html": 200, "/remote/v1/session": 401,
            "/remote/v1/summary": 401, "/admin-api": 403, "/index.html": 403,
        }
        Connection.headers_override = None

    def context(self):
        return ssl.create_default_context()

    def test_five_fixed_credential_free_https_reads(self):
        self.assertEqual(pilot.check_endpoints(self.context(), Connection), 5)
        self.assertEqual(Connection.seen, [path for path, _ in pilot.CHECKS])

    def test_redirects_or_exposed_admin_endpoints_fail_closed(self):
        for path, result in (("/remote/v1/session", 302), ("/admin-api", 200),
                             ("/remote/v1/summary", 200)):
            with self.subTest(path=path):
                Connection.statuses[path] = result
                with self.assertRaises(pilot.PilotCheckError):
                    pilot.check_endpoints(self.context(), Connection)
                self.setUp()

    def test_missing_security_headers_fail_closed(self):
        for headers in ({"Cache-Control": "private", "X-Content-Type-Options": "nosniff"},
                        {"Cache-Control": "no-store"}):
            with self.subTest(headers=headers):
                Connection.headers_override = headers
                with self.assertRaises(pilot.PilotCheckError):
                    pilot.check_endpoints(self.context(), Connection)

    def test_wrong_portal_content_type_fails_closed(self):
        for content_type in ("application/json", "text/html-malformed", "", "text/plain"):
            with self.subTest(content_type=content_type):
                Connection.headers_override = {
                    "Cache-Control": "no-store", "X-Content-Type-Options": "nosniff",
                    "Content-Type": content_type,
                }
                with self.assertRaises(pilot.PilotCheckError):
                    pilot.check_endpoints(self.context(), Connection)

    def test_cannot_bypass_https_trust_or_supply_remote_hostname(self):
        self.assertEqual(pilot.main(["--ca-file", "/nonexistent/dc24h-test-ca.pem"]), 2)
        source = SOURCE.read_text(encoding="utf-8")
        self.assertIn("ssl.create_default_context(cafile=", source)
        self.assertNotIn("CERT_NONE", source)
        self.assertNotIn("https://0.0.0.0", source)


if __name__ == "__main__":
    unittest.main()
