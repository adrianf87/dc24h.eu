"""Offline Stage IV design boundary regressions; no network, root or host changes."""

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
FQDN = "server-747585786.dc24h.eu"
ORIGIN = f"https://{FQDN}:7278"


class PublicStageIVBoundaryTests(unittest.TestCase):
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

    def test_relying_party_id_is_hostname_not_origin(self):
        document = (ROOT / "doc/us/PUBLIC_STAGE_IV_WEBADMIN_DESIGN.md").read_text(
            encoding="utf-8"
        )
        self.assertIn("WebAuthn relying-party ID | `" + FQDN + "`", document)
        self.assertIn("Exact WebAuthn / browser API Origin | `" + ORIGIN + "`", document)
        self.assertIn("cookies do **not** isolate by TCP port", document)

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


if __name__ == "__main__":
    unittest.main()
