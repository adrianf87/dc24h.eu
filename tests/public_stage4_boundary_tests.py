"""Offline Stage IV design boundary regressions; no network, root or host changes."""

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
FQDN = "server-747585786.dc24h.eu"
ORIGIN = f"https://{FQDN}:7278"


class PublicStageIVBoundaryTests(unittest.TestCase):
    def test_new_gateway_is_dedicated_loopback_only_with_startup_gate(self):
        main = (ROOT / "webadmin/server/public-main.ts").read_text(encoding="utf-8")
        handler = (ROOT / "webadmin/server/public-gateway.ts").read_text(encoding="utf-8")
        self.assertIn('app.listen(4278, "127.0.0.1"', main)
        self.assertIn("OWNER_WEBADMIN_PUBLIC_ACTIVATION_GO", main)
        self.assertIn('const internalHost = "127.0.0.1:4278"', handler)
        self.assertIn('const prefix = "/stage4/v1/"', handler)
        self.assertNotIn('import { remoteGateway }', main)

    def test_public_apache_example_pins_exact_origin_and_only_public_routes(self):
        apache = (ROOT / "config/apache2-webadmin-public.conf.example").read_text(
            encoding="utf-8"
        )
        for required in (
            "Listen 7278", "ServerName server-747585786.dc24h.eu",
            "SSLEngine On", "SSLStrictSNIVHostCheck On",
            "letsencrypt/live/server-747585786.dc24h.eu/fullchain.pem",
            "http://127.0.0.1:4278", "ProxyAddHeaders Off",
            "RequestHeader unset Authorization", "RequestHeader unset X-DC24H-Admin-Grant",
            'DocumentRoot "/usr/local/share/dc24h/stage4"',
        ):
            with self.subTest(required=required):
                self.assertIn(required, apache)
        self.assertNotIn("51.255.80.233", apache)
        self.assertNotIn("ProxyPass /admin-api", apache)
        self.assertNotIn("ProxyPass /remote", apache)

    def test_public_shell_contains_no_privileged_tokens_or_private_gateway(self):
        client = (ROOT / "webadmin/public-stage4/panel.js").read_text(encoding="utf-8")
        html = (ROOT / "webadmin/public-stage4/index.html").read_text(encoding="utf-8")
        self.assertIn('const api = "/stage4/v1/"', client)
        self.assertIn('call("login/send"', client)
        self.assertIn('call("login/verify"', client)
        self.assertIn('id="email-code"', html)
        self.assertNotIn("navigator.credentials", client)
        self.assertNotIn("passkey", client.lower() + html.lower())
        for forbidden in ("/admin-api", "/remote/v1", "DC24H_API_TOKEN_FILE", "localStorage"):
            with self.subTest(forbidden=forbidden):
                self.assertNotIn(forbidden, client + html)

    def test_owner_design_pins_separate_hub_and_webadmin_ports(self):
        for locale in ("us", "pl"):
            document = (ROOT / "doc" / locale / "PUBLIC_STAGE_IV_WEBADMIN_DESIGN.md").read_text(
                encoding="utf-8"
            )
            with self.subTest(locale=locale):
                self.assertIn(f"adcs://{FQDN}:1512", document)
                self.assertIn(ORIGIN, document)
                self.assertIn("62.171.156.32", document)
                self.assertIn("OWNER WEBADMIN PUBLIC ACTIVATION GO", document)
                self.assertNotIn("51.255.80.233", document)

    def test_email_sender_and_public_origin_are_fixed(self):
        document = (ROOT / "doc/us/PUBLIC_STAGE_IV_WEBADMIN_DESIGN.md").read_text(
            encoding="utf-8"
        )
        self.assertIn("Exact browser/API Origin | `" + ORIGIN + "`", document)
        self.assertIn("Fixed sender | `admin@dc24h.eu` via local `msmtp -t`", document)
        self.assertIn("cookies do **not** isolate by TCP port", document)

    def test_mail_transport_has_fixed_sender_and_no_shell(self):
        mailer = (ROOT / "webadmin/server/public-msmtp.ts").read_text(encoding="utf-8")
        self.assertIn('spawn(binary, ["-t"], { shell: false', mailer)
        self.assertIn('From: admin@dc24h.eu', mailer)
        self.assertIn('To: " + recipient', mailer)
        self.assertNotIn('odbiorca@gmail.com', mailer)
        gateway = (ROOT / "webadmin/server/public-gateway.ts").read_text(encoding="utf-8")
        self.assertIn('"login/send"', gateway)
        self.assertIn('"login/verify"', gateway)
        self.assertNotIn('"devices/revoke"', gateway)

    def test_existing_private_gateway_remains_local_only(self):
        gateway = (ROOT / "webadmin/server/remote-main.ts").read_text(encoding="utf-8")
        self.assertIn('throw new Error("Public remote gateway is disabled")', gateway)
        self.assertIn('app.listen(port, "127.0.0.1"', gateway)
        apache = (ROOT / "config/apache2-webadmin-remote.conf.example").read_text(
            encoding="utf-8"
        )
        self.assertIn("Listen 127.0.0.1:8443", apache)
        self.assertIn("<VirtualHost 127.0.0.1:8443>", apache)
        self.assertNotIn("Listen 7278", apache)

    def test_stage_iv_never_infers_production_activation(self):
        document = (ROOT / "doc/us/PUBLIC_STAGE_IV_WEBADMIN_DESIGN.md").read_text(
            encoding="utf-8"
        )
        for condition in (
            "NO PUBLIC WEBADMIN ACTIVATION",
            "Do not remove those safety checks",
            "GitHub-hosted Ubuntu 22.04, 24.04 and 26.04",
            "no CI SSH or remote `sudo`",
        ):
            with self.subTest(condition=condition):
                self.assertIn(condition, document)

    def test_offline_account_enrollment_requires_mailbox_proof_and_denies_replacement(self):
        source = (ROOT / "webadmin/server/public-email-enrollment.ts").read_text(encoding="utf-8")
        cli = (ROOT / "webadmin/server/public-email-enroll-main.ts").read_text(encoding="utf-8")
        for required in ("verifyNamedPassword", "reauthorize", "readCode", "sendCode",
                         "timingSafeEqual", "publishPrivateLeafNoReplace", "O_EXCL"):
            self.assertIn(required, source)
        self.assertIn("STAGE4_EMAIL_ENROLLMENT_GO", cli)
        self.assertIn("stdin.setRawMode(true)", cli)
        self.assertNotIn("console.log", cli)
        self.assertNotIn("admin@dc24h.eu", source)  # Recipient is an operator choice.

    def test_serving_cpp_api_accepts_only_local_password_proof_and_role_discovery(self):
        code = (ROOT / "src/api/accounts.cpp").read_text(encoding="utf-8")
        main = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
        self.assertIn("public_account_access_handler", code)
        self.assertIn('request.method == "POST" && request.target == "/api/v1/admin/access"', code)
        self.assertIn('request.method == "GET" && request.target == "/api/v1/management"', code)
        self.assertIn('return response(404, "{\\"error\\":\\"not_found\\"}")', code)
        self.assertIn("database && stage4_private_auth", main)
        self.assertIn("DC24H_STAGE4_PRIVATE_AUTH_ACK", main)

    def test_disabled_stage4_unit_has_no_baked_activation_or_public_node_listener(self):
        unit = (ROOT / "config/dc24h-twilight-stage4-webadmin.service.example").read_text(
            encoding="utf-8")
        for required in ("User=dc24h", "EnvironmentFile=/etc/dc24h/stage4-activation.env",
                         "StateDirectoryMode=0700", "NoNewPrivileges=true", "ProtectSystem=strict"):
            self.assertIn(required, unit)
        self.assertNotIn("Environment=DC24H_STAGE4_STARTUP_ACK=", unit)
        self.assertNotIn("ExecStart=/usr/bin/sudo", unit)


if __name__ == "__main__":
    unittest.main()
