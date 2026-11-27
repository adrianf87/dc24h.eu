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
                self.assertIn("`OWNER WEBADMIN PUBLIC ACTIVATION GO`", document)
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
        self.assertIn('spawn(binary, ["--file=" + mailConfiguration, "-t"]', mailer)
        self.assertIn('dedicatedMsmtpConfigReady()', mailer)
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
            "reported operation does not imply final security acceptance",
            "`OWNER STAGE IV ACCEPTANCE`",
            "`OWNER MERGE GO`",
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
        for required in ("User=dc24h-webadmin", "EnvironmentFile=/etc/dc24h/stage4-activation.env",
                         "StateDirectoryMode=0700", "NoNewPrivileges=true", "ProtectSystem=strict"):
            self.assertIn(required, unit)
        self.assertNotIn("Environment=DC24H_API_TOKEN_FILE", unit)
        self.assertIn("DC24H_STAGE4_BRIDGE_SOCKET=", unit)
        self.assertIn("node24/bin/node", unit)
        self.assertNotIn("Environment=DC24H_STAGE4_STARTUP_ACK=", unit)
        self.assertNotIn("ExecStart=/usr/bin/sudo", unit)

    def test_dedicated_bridge_keeps_hub_token_out_of_webadmin(self):
        bridge = (ROOT / "scripts/stage4_webadmin_bridge.py").read_text(encoding="utf-8")
        gateway = (ROOT / "webadmin/server/public-main.ts").read_text(encoding="utf-8")
        adapter = (ROOT / "webadmin/server/public-bridge-credentials.ts").read_text(encoding="utf-8")
        unit = (ROOT / "config/dc24h-twilight-stage4-bridge.service.example").read_text(encoding="utf-8")
        self.assertIn("socket.SO_PEERCRED", bridge)
        self.assertIn("ROUTES =", bridge)
        self.assertIn("User=dc24h-stage3", unit)
        self.assertIn("Group=dc24h-webadmin", unit)
        self.assertIn('publicBridgeCredentials(process.env)', gateway)
        self.assertIn('env.DC24H_API_TOKEN_FILE', adapter)
        self.assertNotIn('credentials(process.env)', gateway)

    def test_stage_iv_ci_does_not_weaken_production_apparmor_reload(self):
        installer = (ROOT / "scripts/stage4_install.sh").read_text(encoding="utf-8")
        workflow = (ROOT / ".github/workflows/public-stage4-design.yml").read_text(
            encoding="utf-8"
        )
        # sudo drops the runner identity unless the disposable CI job passes
        # both variables. Never grant this exception on the production VPS.
        self.assertIn('"${GITHUB_ACTIONS:-}" = "true"', installer)
        self.assertIn('"${RUNNER_ENVIRONMENT:-}" = "github-hosted"', installer)
        self.assertIn('if ! apparmor_parser -r "$profile"; then', installer)
        self.assertIn('exit 1', installer.split('if ! apparmor_parser -r "$profile"; then')[1])
        # The CI exception must short-circuit BEFORE the production host
        # profile directory or rule can be modified.
        self.assertLess(
            installer.index('"${RUNNER_ENVIRONMENT:-}" = "github-hosted"'),
            installer.index('install -d -m 0755 /etc/apparmor.d/local')
        )
        self.assertIn('elif [ -f "$profile" ]', installer)
        self.assertIn('"GITHUB_ACTIONS=$GITHUB_ACTIONS"', workflow)
        self.assertIn('"RUNNER_ENVIRONMENT=$RUNNER_ENVIRONMENT"', workflow)

    def test_ubuntu_2604_disposable_mariadb_respects_apparmor_confinement(self):
        script = (ROOT / "scripts/stage4_test_database.sh").read_text(encoding="utf-8")
        workflow = (ROOT / ".github/workflows/public-stage4-design.yml").read_text(
            encoding="utf-8"
        )
        self.assertIn('"${GITHUB_ACTIONS:-}" = "true"', script)
        self.assertIn('"${RUNNER_ENVIRONMENT:-}" = "github-hosted"', script)
        self.assertIn('[ "$ID" = ubuntu ] && [ "$VERSION_ID" = 26.04 ]', script)
        self.assertIn('scratch=/var/lib/mariadb/stage4-fixture-$fixture_nonce', script)
        self.assertIn('mysql_exec=(setpriv --reuid=mysql --regid=mysql --init-groups)', script)
        self.assertIn('mysql_user_arg=()', script)
        self.assertIn('socket=/run/mysqld/stage4-fixture-$fixture_nonce.sock', script)
        self.assertIn('install -d -m 0755 -o mysql -g mysql /run/mysqld', script)
        self.assertIn('"${mysql_exec[@]}" mariadbd', script)
        self.assertIn('mariadb-install-db --no-defaults', script)
        self.assertIn("sed -n '1,20p' \"$scratch/db-bootstrap.log\"", script)
        self.assertIn('tail -n 25 "$scratch/db-bootstrap.log"', script)
        self.assertNotIn('apparmor_parser', script)
        self.assertNotIn('aa-disable', script)
        self.assertNotIn('aa-complain', script)
        self.assertIn('"GITHUB_ACTIONS=$GITHUB_ACTIONS" "RUNNER_ENVIRONMENT=$RUNNER_ENVIRONMENT"', workflow)
        validator = (ROOT / "webadmin/server/stage4-bootstrap-config.ts").read_text(encoding="utf-8")
        self.assertIn('requireField("MARIADB_SOCKET", /^\\/run\\/', validator)

    def test_hosted_browser_install_skips_unscoped_apt_sources(self):
        for name in ("build.yml", "stage4-ubuntu-vm-validation.yml",
                     "public-stage4-design.yml"):
            workflow = (ROOT / ".github/workflows" / name).read_text(encoding="utf-8")
            with self.subTest(workflow=name):
                self.assertIn("npx playwright install chromium", workflow)
                self.assertNotIn("playwright install --with-deps", workflow)

    def test_stage_iii_apt_uses_only_signed_ubuntu_feed(self):
        workflow = (ROOT / ".github/workflows/public-stage3-qualification.yml").read_text(encoding="utf-8")
        installer = (ROOT / ".github/scripts/install-ci-deps.sh").read_text(encoding="utf-8")
        # All Stage III package installs share one signed Ubuntu-only helper,
        # excluding unrelated third-party feeds on the hosted image.
        self.assertGreaterEqual(workflow.count('bash .github/scripts/install-ci-deps.sh'), 2)
        self.assertNotIn('apt-get ', workflow)
        self.assertIn('Dir::Etc::sourcelist=${ubuntu_apt_source}', installer)
        self.assertIn('Dir::Etc::sourceparts=-', installer)
        self.assertIn('apt-get "${apt_network_options[@]}" update', installer)
        self.assertNotIn("--allow-unauthenticated", workflow)
        self.assertNotIn("--allow-unauthenticated", installer)


if __name__ == "__main__":
    unittest.main()
