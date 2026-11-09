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
from tls_support import credentials, connect_tls, tls_endpoint, finish_tls, TLS_READY


class AdcsProcessTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix='dc24h-adcs-')
        self.addCleanup(self.directory.cleanup)
        self.cert, self.key = credentials(self.directory.name)

    def hub(self, *extra):
        return Hub(self, '--listen', '127.0.0.1:0', '--adcs-listen', '127.0.0.1:0',
                   '--tls-cert', self.cert, '--tls-key', self.key,
                   *extra, listeners=1)

    def tls_endpoint(self, hub):
        return tls_endpoint(hub)

    def connect_tls(self, hub, version=None):
        return connect_tls(self, hub, self.cert, version)

    def test_tls12_or_later_and_adc_parser(self):
        hub = self.hub()
        peer = self.connect_tls(hub)
        self.assertIn(peer.version(), ('TLSv1.2', 'TLSv1.3'))
        peer.sendall('HSUP ADBASE\nBMSG ABC2 Cześć\\s😀\n'.encode())
        finish_tls(self, peer)
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
        finish_tls(self, peer)
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

    def test_explicit_tls12_and_tls13(self):
        for version in (ssl.TLSVersion.TLSv1_2, ssl.TLSVersion.TLSv1_3):
            with self.subTest(version=version):
                hub = self.hub()
                peer = self.connect_tls(hub, version)
                self.assertEqual(peer.version(), 'TLSv1.2' if version == ssl.TLSVersion.TLSv1_2 else 'TLSv1.3')
                peer.sendall(b'HSUP ADBASE\n')
                finish_tls(self, peer)
                hub.stop()
                self.assertEqual(counters(self, hub), (1, 0, 0, 0))

    def test_tls_only_and_fragmented_frames(self):
        hub = Hub(self, '--adcs-listen', '127.0.0.1:0', '--tls-cert', self.cert,
                  '--tls-key', self.key, ready_pattern=TLS_READY)
        peer = self.connect_tls(hub)
        for byte in b'HSUP ADBASE\n':
            peer.sendall(bytes([byte]))
        finish_tls(self, peer)
        hub.stop()
        self.assertNotIn(b'Network listening on', hub.log)
        self.assertEqual(counters(self, hub), (1, 0, 0, 0))

    def test_stalled_handshake_timeout_and_recovery(self):
        hub = self.hub('--idle-timeout', '1')
        with socket.create_connection(tls_endpoint(hub), timeout=3) as stalled:
            stalled.sendall(b'\x16')  # Incomplete ClientHello must not refresh the deadline.
            self.assertEqual(stalled.recv(1), b'')
        peer = self.connect_tls(hub)
        peer.sendall(b'HSUP ADBASE\n')
        finish_tls(self, peer)
        hub.stop()
        self.assertIn(b'handshakes=1 incomplete=1', hub.log)
        self.assertEqual(counters(self, hub), (1, 0, 0, 0))

    def test_legacy_tls_rejected_and_next_client_survives(self):
        hub = self.hub()
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
        context.check_hostname = False
        context.verify_mode = ssl.CERT_NONE
        context.minimum_version = ssl.TLSVersion.TLSv1
        context.maximum_version = ssl.TLSVersion.TLSv1_1
        context.set_ciphers('ALL:@SECLEVEL=0')
        with socket.create_connection(tls_endpoint(hub), timeout=3) as raw:
            with self.assertRaises(ssl.SSLError):
                context.wrap_socket(raw, server_hostname='localhost')
        peer = self.connect_tls(hub)
        peer.sendall(b'HSUP ADBASE\n')
        finish_tls(self, peer)
        hub.stop()
        self.assertIn(b'handshakes=1 incomplete=1', hub.log)

    def test_bad_key_mismatch_encryption_and_missing_file_fail_before_ready(self):
        other = pathlib.Path(self.directory.name) / 'other'
        other.mkdir()
        _, other_key = credentials(other)
        encrypted = str(other / 'encrypted.pem')
        subprocess.run(['openssl', 'pkey', '-in', self.key, '-aes-256-cbc',
                        '-passout', 'pass:temporary-test-only', '-out', encrypted],
                       check=True, timeout=5, capture_output=True)
        for key in (other_key, encrypted, str(other / 'missing.pem')):
            with self.subTest(key=key):
                result = subprocess.run([BINARY, '--adcs-listen', '127.0.0.1:0',
                                         '--tls-cert', self.cert, '--tls-key', key],
                                        capture_output=True, timeout=5, stdin=subprocess.DEVNULL)
                self.assertEqual(result.returncode, 1, result.stderr)
                self.assertNotIn(b' listening on ', result.stderr)
                self.assertNotIn(b'Enter PEM pass phrase', result.stderr)

    def test_resets_during_handshake_do_not_raise_sigpipe(self):
        import struct
        hub = self.hub()
        hello = ssl.MemoryBIO()
        context = ssl.create_default_context(cafile=self.cert)
        client = context.wrap_bio(ssl.MemoryBIO(), hello, server_hostname='localhost')
        with self.assertRaises(ssl.SSLWantReadError):
            client.do_handshake()
        wire = hello.read()
        for _ in range(32):
            with socket.create_connection(tls_endpoint(hub), timeout=3) as peer:
                peer.sendall(wire)
                peer.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack('ii', 1, 0))
        peer = self.connect_tls(hub)
        peer.sendall(b'HSUP ADBASE\n')
        finish_tls(self, peer)
        hub.stop()
        self.assertEqual(counters(self, hub), (1, 0, 0, 0))

    def test_simultaneous_large_tls_streams_and_shutdown(self):
        hub = self.hub()
        peers = [self.connect_tls(hub) for _ in range(12)]
        payload = b'HSUP ADBASE\n' * 20000
        for peer in peers:
            peer.sendall(payload)
            finish_tls(self, peer)
        stalled = socket.create_connection(tls_endpoint(hub), timeout=3)
        self.addCleanup(stalled.close)
        active = self.connect_tls(hub)
        hub.stop()
        self.assertEqual(counters(self, hub), (240000, 0, 0, 0))
        self.assertEqual(active.recv(1), b'')
        self.assertEqual(stalled.recv(1), b'')

if __name__ == '__main__':
    unittest.main()
