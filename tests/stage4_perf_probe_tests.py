"""Offline contract tests; no sockets, load, production traffic or secrets."""

import contextlib
import importlib.util
import io
from pathlib import Path
import os
import tempfile
import unittest
from unittest import mock


SCRIPT = Path(__file__).resolve().parents[1] / "scripts/stage4_perf_probe.py"
spec = importlib.util.spec_from_file_location("stage4_perf_probe", SCRIPT)
probe = importlib.util.module_from_spec(spec)
spec.loader.exec_module(probe)


class StageFourPerfContract(unittest.TestCase):
    def test_protocol_requires_complete_valid_hub_advertisement(self):
        class Socket:
            def __init__(self, parts):
                self.parts = iter(parts)
            def settimeout(self, timeout):
                self.timeout = timeout
            def recv(self, size):
                return next(self.parts, b"")

        self.assertTrue(probe.adc_greeting(Socket([b"IS", b"UP ADBASE ADTIGR\nISID AAAB\n"]), 2))
        for parts in ([b"ISTA 240 Rejected\n"], [b"ISUP ADNONSENSE\n"], [b"ISUP ADBASE"], [b""], [b"ISUP ADBASE " * 200]):
            with self.subTest(parts=parts):
                self.assertFalse(probe.adc_greeting(Socket(parts), 2))

    def test_http_success_requires_private_json_or_real_html_media_type(self):
        class Response:
            status = 200
            def __init__(self, content_type, body):
                self.content_type, self.body = content_type, body
            def getheader(self, name, default=None):
                return self.content_type
            def read(self, limit):
                return self.body[:limit]

        valid = Response("application/json; charset=utf-8",
            b'{"schema":1,"mode":"read-only","server":{"version":"beta-0.4.16","running":true}}')
        self.assertTrue(probe.valid_http_response("api", valid, valid.body))
        for response in (
            Response("text/html", b"<html>Unauthorized</html>"),
            Response("application/json", b'{"error":"unavailable"}'),
            Response("application/json", b'{"schema":1,"mode":"read-only"}'),
            Response("application/json", b'{"schema":1,"mode":"read-only","server":{}}'),
            Response("application/json", b'{"schema":1,"mode":"read-only","server":{"version":"beta-0.4.16","running":false}}'),
            Response("application/json", b"not JSON"),
            Response("application/json-attacker", valid.body),
        ):
            with self.subTest(mime=response.content_type):
                with mock.patch.object(probe.http.client, "HTTPSConnection") as connection:
                    connection.return_value.getresponse.return_value = response
                    ok, _, reason = probe.probe("api", 8443, 2, object(), "dc24h_remote=" + "a" * 64)
                    self.assertFalse(ok)
                    self.assertEqual(reason, "http_unexpected")
        html = Response("text/html; charset=utf-8", b"<!doctype html><html></html>")
        self.assertTrue(probe.valid_http_response("webadmin", html, html.body))
        self.assertFalse(probe.valid_http_response("webadmin", valid, valid.body))

    def test_summary_reports_only_aggregates_and_no_capacity_claim(self):
        report = probe.summarize("api", [(True, 10.0, "ok"), (True, 20.0, "ok"),
                                         (False, 30.0, "tls_failure")], 2.0, 2, 1, 120, 130)
        self.assertEqual(report["requests"], 3)
        self.assertEqual(report["success"], 2)
        self.assertEqual(report["error_kinds"], {"tls_failure": 1})
        self.assertEqual(report["p50_ms"], 10.0)
        self.assertEqual(report["p95_ms"], 20.0)
        self.assertEqual(report["successful_requests_per_second"], 1.0)
        self.assertFalse(report["capacity_claim"])
        self.assertNotIn("cookie", str(report))
        self.assertEqual(probe.percentile([], 0.95), None)

    def test_no_implicit_load_or_unverified_tls(self):
        unsafe = [
            ["--profile", "adc", "--port", "411"],
            ["--profile", "adc", "--port", "411", "--ack-isolated", "--rate", "21"],
            ["--profile", "adc", "--port", "411", "--ack-isolated", "--requests", "1001"],
            ["--profile", "adc", "--port", "411", "--ack-isolated", "--concurrency", "9"],
            ["--profile", "webadmin", "--port", "8443", "--ack-isolated"],
            ["--profile", "api", "--port", "8443", "--ack-isolated", "--ca-file", "ca.pem"],
        ]
        for command in unsafe:
            with self.subTest(command=command), contextlib.redirect_stderr(io.StringIO()):
                with self.assertRaises(SystemExit):
                    probe.arguments(command)
        self.assertEqual(probe.arguments(["--profile", "adc", "--port", "411", "--ack-isolated"]).profile, "adc")

    def test_cookie_is_private_no_symlink_and_identity_never_logged(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "cookie"
            cookie = "dc24h_remote=" + "a" * 64
            path.write_text(cookie + "\n", encoding="ascii")
            path.chmod(0o600)
            self.assertEqual(probe.private_cookie(str(path)), cookie)
            path.chmod(0o644)
            with self.assertRaises(ValueError):
                probe.private_cookie(str(path))
            path.chmod(0o600)
            link = Path(directory) / "link"
            os.symlink(path, link)
            with self.assertRaises(OSError):
                probe.private_cookie(str(link))
            path.write_text("dc24h_remote=" + "Z" * 64, encoding="ascii")
            with self.assertRaises(ValueError):
                probe.private_cookie(str(path))
            path.unlink()
            os.mkfifo(path, 0o600)
            with self.assertRaises(ValueError):
                probe.private_cookie(str(path))


if __name__ == "__main__":
    unittest.main()
