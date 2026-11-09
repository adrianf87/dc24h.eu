"""ADCS integration using actual TLS sockets and disposable credentials."""
import pathlib
import signal
import socket
import ssl
import subprocess
import sys
import tempfile
import time
import unittest

from network_process_tests import BINARY, Hub
from adc_process_tests import counters


class AdcsProcessTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix='dc24h-adcs-')
        self.addCleanup(self.directory.cleanup)
        self.cert = str(pathlib.Path(self.directory.name) / 'certificate.pem')
        self.key = str(pathlib.Path(self.directory.name) / 'private.pem')
        subprocess.run(['openssl', 'req', '-x509', '-newkey', 'rsa:2048', '-nodes',
                        '-days', '1', '-subj', '/CN=localhost',
                        '-keyout', self.key, '-out', self.cert],
                       check=True, timeout=15, capture_output=True)

    def hub(self, *extra):
        return Hub(self, '--listen', '127.0.0.1:0', '--adcs-listen', '127.0.0.1:0',
                   '--tls-cert', self.cert, '--tls-key', self.key,
                   *extra, listeners=1)

    def tls_endpoint(self, hub):
        import re
        hub.wait_for(lambda: b'ADCS listening on' in hub.log)
        port = int(re.search(rb'ADCS listening on 127.0.0.1:(\d+)', hub.log).group(1))
        return ('127.0.0.1', port)

    def connect_tls(self, hub):
        # Disable trust verification ONLY for the ephemeral test certificate.
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
        context.check_hostname = False
        context.verify_mode = ssl.CERT_NONE
        peer = context.wrap_socket(socket.create_connection(self.tls_endpoint(hub), timeout=3),
                                   server_hostname='localhost')
        self.addCleanup(peer.close)
        return peer

    def test_tls12_or_later_and_adc_parser(self):
        hub = self.hub()
        peer = self.connect_tls(hub)
        self.assertIn(peer.version(), ('TLSv1.2', 'TLSv1.3'))
        peer.sendall('HSUP ADBASE\nBMSG ABC2 Cześć\\s😀\n'.encode())
        peer.close()
        time.sleep(0.2)
        hub.stop()
        self.assertEqual(counters(self, hub), (2, 0, 0, 0))
        self.assertIn(b'ADCS stopped; handshakes=1', hub.log)

    def test_mixed_listeners_and_bad_handshakes(self):
        hub = self.hub()
        with socket.create_connection(self.tls_endpoint(hub), timeout=3) as invalid:
            invalid.sendall(b'NOT TLS\n')
            invalid.shutdown(socket.SHUT_WR)
            invalid.settimeout(3)
            try:
                invalid.recv(1024)
            except ConnectionResetError:
                pass
        with socket.create_connection(hub.endpoints[0], timeout=3) as plain:
            plain.sendall(b'HSUP ADBASE\n')
            plain.shutdown(socket.SHUT_WR)
            self.assertEqual(plain.recv(1), b'')
        peer = self.connect_tls(hub)
        peer.sendall(b'HSUP ADTIGR\n')
        peer.close()
        time.sleep(0.2)
        hub.stop(signal.SIGINT)
        self.assertEqual(counters(self, hub)[0], 2)
        self.assertIn(b'ADCS stopped; handshakes=1 incomplete=1', hub.log)

    def test_missing_and_invalid_credentials(self):
        for args in (
            ('--adcs-listen', '127.0.0.1:0'),
            ('--tls-cert', self.cert),
            ('--tls-key', self.key),
        ):
            with self.subTest(args=args):
                result = subprocess.run([BINARY, *args], capture_output=True, timeout=5)
                self.assertEqual(result.returncode, 2, result.stderr)
        result = subprocess.run([BINARY, '--listen', '127.0.0.1:0',
                                 '--adcs-listen', '127.0.0.1:0',
                                 '--tls-cert', self.cert, '--tls-key', self.cert],
                                capture_output=True, timeout=5)
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertNotIn(b' listening on ', result.stderr)

if __name__ == '__main__':
    unittest.main()
