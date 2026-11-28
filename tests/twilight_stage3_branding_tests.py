#!/usr/bin/env python3
"""Check Stage III display branding without widening protocol/security identity."""

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]


def source(relative):
    return (ROOT / relative).read_text(encoding="utf-8")


class TwilightStageThreeBrandingTests(unittest.TestCase):
    def test_operator_text_uses_canonical_product_name(self):
        main = source("src/main.cpp")
        self.assertIn('std::cout << "dc24h.eu "', main)
        self.assertIn('std::string("Starting dc24h.eu ")', main)
        for language in ("us", "pl"):
            help_text = source(f"languages/{language}/messages.hpp")
            self.assertIn("dc24h.eu", help_text)
            self.assertIn("dc24h [--language us|pl] --help | --version", help_text)

        welcome = "Welcome to dc24h.eu. Guest access is enabled."
        self.assertIn(welcome, source("include/dc24h/chat/settings.hpp"))
        self.assertIn(f"welcome_message = {welcome}", source("config/dc24h.ini.example"))
        for name in ("dc24h", "dc24h-control", "dc24h-remote-gateway"):
            unit = source(f"config/{name}.service.example")
            self.assertIn("Description=dc24h.eu", unit)

    def test_status_api_and_webadmin_agree_on_exact_owner_approved_name(self):
        # Explicit OWNER cutover: reject even the legacy name, without silently
        # broadening the accepted status-schema identity.
        self.assertIn(r'\"name\":\"dc24h.eu\"', source("src/api/server.cpp"))
        status = source("webadmin/src/lib/status.ts")
        self.assertIn(r'\"product_id\":\"dc24h\"', source("src/api/server.cpp"))
        self.assertIn('server.product_id === PRODUCT_ID && server.name === PRODUCT_NAME', status)
        self.assertEqual(status.count('!hasStatusIdentity(root, server)'), 2)
        self.assertIn("assert.throws(() => parseSnapshot(candidate)",
                      source("webadmin/tests/status.test.ts"))

    def test_dns_tls_origin_and_passkey_identity_unchanged(self):
        for name in ("apache2-webadmin-https.conf.example", "apache2-webadmin-https-proxy.conf.example"):
            apache = source(f"config/{name}")
            self.assertIn("ServerName dc24h.eu", apache)
            self.assertNotIn("ServerName dc24h.eu-twilight", apache)
        proxy = source("config/apache2-webadmin-https-proxy.conf.example")
        self.assertIn("https://dc24h.eu", proxy)
        passkeys = source("webadmin/server/remote-webauthn.ts")
        self.assertIn('rpName: PRODUCT_NAME, rpID', passkeys)
        self.assertIn("expectedOrigin: origin", passkeys)
        self.assertIn("expectedRPID: rpID", passkeys)
        self.assertNotIn('expectedRPID: PRODUCT_NAME', passkeys)

    def test_shared_adc_welcome_fixtures_match_runtime_brand(self):
        # These fixtures feed real C++/Node/VM jobs; one stale literal causes
        # widespread failures even when unrelated protocol logic is correct.
        for path in (
            "tests/adc_login_process_tests.py",
            "tests/private_chat_process_tests.py",
            "tests/message_routing_process_tests.py",
            "tests/user_session_process_tests.py",
            "tests/user_list_process_tests.py",
        ):
            text = source(path)
            self.assertIn("dc24h.eu.", text, path)



if __name__ == "__main__":
    unittest.main()
