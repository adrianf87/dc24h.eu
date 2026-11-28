"""Trust-checked live context replacement using disposable ADC and ADCS clients."""

import hashlib
import os
from pathlib import Path
import socket
import ssl
import sys
import tempfile
import unittest
from unittest.mock import patch

from network_process_tests import Hub
from tls_support import TLS_READY, tls_endpoint

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from stage3.fixtures import authority, issue, loopback_context, write_config
from public_stage3_qualification_external import guest
from stage4_webadmin_bridge import HubConnection, serving_tls


HOST = "tls.test"


def fingerprint(pem):
    return hashlib.sha256(ssl.PEM_cert_to_DER_cert(pem.read_text())).hexdigest()


class PlainContext:
    def wrap_socket(self, raw, *, server_hostname):
        return raw


class ReloadTests(unittest.TestCase):
    def test_context_replacement_and_failed_reloads_keep_existing_sessions(self):
        with tempfile.TemporaryDirectory(prefix="dc24h-adcs-reload-") as temporary:
            root = Path(temporary)
            ca = authority(root / "trusted")
            foreign = authority(root / "foreign")
            old = issue(root, ca, "old", san=HOST)
            current = issue(root, ca, "new", san=HOST)
            wrong = issue(root, ca, "wrong", san="foreign.test")
            expired = issue(root, ca, "expired", san=HOST, start=-3, end=-1)
            untrusted = issue(root, foreign, "untrusted", san=HOST)
            bogus = root / "bogus.pem"
            bogus.write_text("damaged certificate\n")
            private = root / "api"
            private.mkdir(mode=0o700)
            api = private / "hub.sock"
            token = root / "api.token"
            token.write_text("a" * 64 + "\n")
            token.chmod(0o600)
            selected = root / "current"
            selected.mkdir()
            pair = selected / "fullchain.pem", selected / "privkey.pem"

            def select(cert, key):
                pair[0].write_bytes(cert.read_bytes())
                pair[1].write_bytes(key.read_bytes())

            select(*old)
            config = root / "hub.ini"
            write_config(config, pair, idle=95)
            config.write_text(config.read_text().replace("listen=none", "listen=127.0.0.1:0").replace(
                "[api]\nenabled=off", f"[api]\nenabled=on\nsocket={api}\ntoken_file={token}"))
            trust = root / "trusted" / "ca.pem"
            with patch.dict(os.environ, {"DC24H_TLS_RELOAD_HOSTNAME": HOST, "SSL_CERT_FILE": str(trust)}):
                hub = Hub(self, "--config", str(config))
            endpoint = tls_endpoint(hub)
            pid = hub.process.pid
            status = serving_tls("GET", api, token)
            self.assertEqual(status, {"schema": 1, "hostname": HOST, "certificate_chain": str(pair[0]),
                                      "private_key_path": str(pair[1])})
            client = loopback_context(trust)

            def handshake():
                with socket.create_connection(endpoint, timeout=2) as raw:
                    with client.wrap_socket(raw, server_hostname=HOST) as peer:
                        return hashlib.sha256(peer.getpeercert(binary_form=True)).hexdigest()

            def local_post(value):
                call = HubConnection(str(api), timeout=2)
                try:
                    call.request("POST", "/api/v1/tls/reload", headers={"Host": "localhost", "Authorization": "Bearer " + value})
                    result = call.getresponse()
                    result.read()
                    return result.status
                finally:
                    call.close()

            self.assertEqual(handshake(), fingerprint(old[0]))
            with guest(client, b"BeforeTLS", endpoint=endpoint, hostname=HOST) as secure, \
                    guest(PlainContext(), b"BeforeADC", endpoint=hub.endpoints[0], hostname=HOST) as plain:
                select(*current)
                self.assertEqual(local_post("c" * 64), 401)
                self.assertEqual(handshake(), fingerprint(old[0]))
                self.assertEqual(serving_tls("POST", api, token), {"status": "reloaded"})
                self.assertEqual(handshake(), fingerprint(current[0]))
                for index, bad_pair in enumerate(((bogus, old[1]), (current[0], old[1]), wrong, expired, untrusted)):
                    with self.subTest(index=index):
                        select(*bad_pair)
                        self.assertEqual(local_post("a" * 64), 503)
                        self.assertEqual(handshake(), fingerprint(current[0]))
                for source, target in ((secure, plain), (plain, secure)):
                    frame = b"BMSG " + source.sid + b" Reload-kept-session"
                    source.send(frame)
                    self.assertEqual(target.until(lambda line: line == frame, "existing-session chat"), frame)
                self.assertEqual(hub.process.pid, pid)
                self.assertIsNone(hub.process.poll())
            hub.stop()
            self.assertNotIn(b"PRIVATE KEY", hub.log)


if __name__ == "__main__":
    unittest.main()
