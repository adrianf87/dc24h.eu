#!/usr/bin/env python3
"""Check Stage III display branding without widening protocol/security identity."""

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]


def source(relative):
    return (ROOT / relative).read_text(encoding="utf-8")


class TwilightStageThreeBrandingTests(unittest.TestCase):
    def test_operator_text_uses_twilight_without_renaming_executable(self):
        main = source("src/main.cpp")
        self.assertIn('std::cout << "dc24h.eu-twilight "', main)
        self.assertIn('std::string("Starting dc24h.eu-twilight ")', main)
        for language in ("us", "pl"):
            help_text = source(f"languages/{language}/messages.hpp")
            self.assertIn("dc24h.eu-twilight", help_text)
            self.assertIn("dc24h [--language us|pl] --help | --version", help_text)

        welcome = "Welcome to dc24h.eu-twilight. Guest access is enabled."
        self.assertIn(welcome, source("include/dc24h/chat/settings.hpp"))
        self.assertIn(f"welcome_message = {welcome}", source("config/dc24h.ini.example"))
        for name in ("dc24h", "dc24h-control", "dc24h-remote-gateway"):
            unit = source(f"config/{name}.service.example")
            self.assertIn("Description=dc24h.eu-twilight", unit)

    def test_status_api_still_rejects_foreign_names(self):
        # A human-facing display label is not a reason to widen v1 parsing.
        self.assertIn(r'\"name\":\"dc24h.eu\"', source("src/api/server.cpp"))
        status = source("webadmin/src/lib/status.ts")
        self.assertIn('name: "dc24h.eu";', status)
        self.assertEqual(status.count('server.name !== "dc24h.eu"'), 2)
        self.assertNotIn('server.name !== "dc24h.eu-twilight"', status)

    def test_dns_tls_origin_and_passkey_identity_unchanged(self):
        for name in ("apache2-webadmin-https.conf.example", "apache2-webadmin-https-proxy.conf.example"):
            apache = source(f"config/{name}")
            self.assertIn("ServerName dc24h.eu", apache)
            self.assertNotIn("ServerName dc24h.eu-twilight", apache)
        proxy = source("config/apache2-webadmin-https-proxy.conf.example")
        self.assertIn("https://dc24h.eu", proxy)
        passkeys = source("webadmin/server/remote-webauthn.ts")
        self.assertIn('rpName: "dc24h.eu-twilight", rpID', passkeys)
        self.assertIn("expectedOrigin: origin", passkeys)
        self.assertIn("expectedRPID: rpID", passkeys)
        self.assertNotIn('expectedRPID: "dc24h.eu-twilight"', passkeys)


if __name__ == "__main__":
    unittest.main()
