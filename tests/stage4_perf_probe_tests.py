"""Offline contract tests; no sockets, load, production traffic or secrets."""

import contextlib
import importlib.util
import io
from pathlib import Path
import os
import tempfile
import unittest


SCRIPT = Path(__file__).resolve().parents[1] / "scripts/stage4_perf_probe.py"
spec = importlib.util.spec_from_file_location("stage4_perf_probe", SCRIPT)
probe = importlib.util.module_from_spec(spec)
spec.loader.exec_module(probe)


class StageFourPerfContract(unittest.TestCase):
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


if __name__ == "__main__":
    unittest.main()
