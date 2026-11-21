#!/usr/bin/env python3
"""Remote beta-0.2 gateway invariants, including public-exposure guardrails."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class RemoteConfiguration(unittest.TestCase):
    def text(self, path):
        return (ROOT / path).read_text(encoding="utf-8")

    def test_separate_https_loopback_only(self):
        config = self.text("config/apache2-webadmin-remote.conf.example")
        self.assertIn("Listen 127.0.0.1:8443", config)
        self.assertIn("<VirtualHost 127.0.0.1:8443>", config)
        self.assertIn("Require local", config)
        self.assertIn("SSLEngine On", config)
        self.assertIn("SSLProtocol -all +TLSv1.2 +TLSv1.3", config)
        self.assertIn("ProxyAddHeaders Off", config)
        self.assertIn("ProxyPassMatch", config)
        self.assertIn("challenge|login|session|summary|logout", config)
        self.assertNotIn("ProxyPass \"/\"", config)
        self.assertIn("Require all denied", config)
        for path in ("admin-api", "api", "management"):
            self.assertIn(path, config)
        for header in ("Authorization", "X-DC24H-Admin-Grant", "X-DC24H-Session",
                       "X-DC24H-Csrf", "Forwarded", "X-Forwarded-For"):
            self.assertIn("RequestHeader unset " + header, config)
        self.assertIn("frame-ancestors 'none'", config)
        self.assertIn("Cache-Control \"no-store\"", config)

    def test_gateway_in_process_permissions_and_boundaries(self):
        code = self.text("webadmin/server/remote-gateway.ts")
        for expected in ("/api/v1/admin/access", "/api/v1/management",
                         "/api/v1/status", '"administrator"', '"owner"',
                         "parseManagement", "sameSecret", "randomBytes",
                         "SameSite=Strict", "HttpOnly; Secure", "maxSessions = 4",
                         "absoluteTime = 10 * 60_000", "idleTime = 2 * 60_000",
                         "api_unavailable", "session_revoked", "login_refused",
                         "no-store", "login:global", "authorization"):
            self.assertIn(expected, code)
        for forbidden in ("/api/v1/management/configuration/write",
                          "/api/v1/management/hub/start",
                          "child_process", "exec(", "localStorage", "sessionStorage"):
            self.assertNotIn(forbidden, code)
        executable = self.text("webadmin/server/remote-main.ts")
        self.assertIn('app.listen(port, "127.0.0.1"', executable)
        self.assertIn("DC24H_REMOTE_ORIGIN", executable)
        self.assertIn("DC24H_REMOTE_PUBLIC", executable)
        self.assertIn("credentials(process.env)", executable)

    def test_no_new_migration_and_separate_ui(self):
        ui = self.text("webadmin/src/remote/RemoteApp.tsx")
        self.assertIn("X-DC24H-Remote-CSRF", ui)
        self.assertIn('call("logout"', ui)
        self.assertIn('call("summary"', ui)
        self.assertNotIn("localStorage", ui)
        self.assertNotIn("sessionStorage", ui)
        self.assertNotIn("Start/Stop/Restart", ui)
        self.assertIn('remote.html', self.text("webadmin/vite.config.ts"))
        self.assertIn('set(DC24H_VERSION "beta-0.2")', self.text("CMakeLists.txt"))
        self.assertIn('WEBADMIN_VERSION = "beta-0.2"', self.text("webadmin/src/lib/status.ts"))
        self.assertIn("beta-0.2", self.text(".github/workflows/build.yml"))
        self.assertIn("remote.gateway.config", self.text("CMakeLists.txt"))
        self.assertIn("apache2.remote.gateway", self.text("CMakeLists.txt"))
        self.assertEqual(len(list((ROOT / "database/migrations").glob("*.sql"))), 11)


if __name__ == "__main__":
    unittest.main()
