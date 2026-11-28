"""Qualify explicit ncdc certificate pins on a disposable ADCS listener."""

from pathlib import Path
import ssl
import sys
import tempfile
import unittest

from network_process_tests import Hub
from tls_support import TLS_READY, credentials

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from public_stage3_qualification_external import Ncdc, keyprint


class NcdcKeyprintTests(unittest.TestCase):
    def test_refuse_a_different_leaf_and_accept_the_expected_leaf(self):
        with tempfile.TemporaryDirectory(prefix="stage5-ncdc-pin-") as directory:
            root = Path(directory)
            other = root / "other"
            other.mkdir()
            certificate, private_key = credentials(root)
            other_certificate, _ = credentials(other)
            expected_pin = keyprint(ssl.PEM_cert_to_DER_cert(Path(certificate).read_text()))
            different_pin = keyprint(ssl.PEM_cert_to_DER_cert(Path(other_certificate).read_text()))
            self.assertNotEqual(different_pin, expected_pin)

            hub = Hub(self, "--adcs-listen", "127.0.0.1:0", "--tls-cert", certificate,
                      "--tls-key", private_key, "--log-connections", "on", ready_pattern=TLS_READY)
            host, port = hub.endpoints[0]
            for name, pin, marker in (
                ("wrong", different_pin, b"Old keyprint:"),
                ("matching", expected_pin, b"Welcome to"),
            ):
                profile = root / name
                profile.mkdir(mode=0o700)
                with Ncdc(str(profile)) as client:
                    client.watch(b"Welcome")
                    client.screen(b"Welcome")
                    client.send("/set nick StageVNcdc")
                    client.watch(marker)
                    client.send(f"/open stage5pin adcs://{host}:{port}/?kp=SHA256/{pin}")
                    client.screen(marker)
                    if name == "matching":
                        hub.wait_for(lambda: b"event=login_ready" in hub.log)
                    client.quit()

            hub.stop()
            self.assertEqual(hub.log.count(b"event=accepted"), 2)
            self.assertEqual(hub.log.count(b"event=login_ready"), 1)


if __name__ == "__main__":
    unittest.main()
