"""Hub leaf policy and TLS 1.2/1.3 AEAD on a real listener. No session tickets."""
import socket
import ssl
import subprocess
import tempfile
import unittest

from network_process_tests import BINARY, Hub
from tls_support import TLS_READY, connect_tls, credentials, tls_endpoint


def openssl_cert(directory, name, *extra):
    cert = f'{directory}/{name}.pem'
    key = f'{directory}/{name}.key'
    subprocess.run(['openssl', 'req', '-x509', '-newkey', 'rsa:2048', '-nodes',
                    '-subj', '/CN=localhost', '-keyout', key, '-out', cert, *extra],
                   check=True, capture_output=True, timeout=15)
    return cert, key


class TlsSecurityProcessTests(unittest.TestCase):
    def test_expired_leaf_fails_check_and_listen_without_opening_a_port(self):
        with tempfile.TemporaryDirectory(prefix='dc24h-tls-policy-') as directory:
            cert, key = openssl_cert(
                directory, 'expired',
                '-not_before', '20200101000000Z', '-not_after', '20200102000000Z',
                '-addext', 'keyUsage=digitalSignature,keyEncipherment',
                '-addext', 'extendedKeyUsage=serverAuth')
            checked = subprocess.run(
                [BINARY, '--language', 'pl', '--check-config', '--adcs-listen', '127.0.0.1:0',
                 '--tls-cert', cert, '--tls-key', key],
                capture_output=True, timeout=5, stdin=subprocess.DEVNULL)
            self.assertEqual(checked.returncode, 2, checked.stderr)
            self.assertIn('Certyfikat TLS nie jest ważny w chwili uruchomienia'.encode(), checked.stderr)
            self.assertNotIn(b'BEGIN CERTIFICATE', checked.stderr)
            self.assertNotIn(b'PRIVATE', checked.stderr)
            started = subprocess.run(
                [BINARY, '--adcs-listen', '127.0.0.1:0', '--tls-cert', cert, '--tls-key', key],
                capture_output=True, timeout=5, stdin=subprocess.DEVNULL)
            self.assertEqual(started.returncode, 1, started.stderr)
            self.assertIn(b'TLS certificate is not valid at startup', started.stderr)
            self.assertNotIn(b'listening on', started.stderr)

    def test_current_leaf_checks_and_aead_only_without_resumption(self):
        with tempfile.TemporaryDirectory(prefix='dc24h-tls-aead-') as directory:
            cert, key = credentials(directory)
            checked = subprocess.run(
                [BINARY, '--check-config', '--adcs-listen', '127.0.0.1:0',
                 '--tls-cert', cert, '--tls-key', key],
                capture_output=True, timeout=5, stdin=subprocess.DEVNULL)
            self.assertEqual(checked.returncode, 0, checked.stderr)
            hub = Hub(self, '--adcs-listen', '127.0.0.1:0', '--tls-cert', cert, '--tls-key', key,
                      ready_pattern=TLS_READY)
            endpoint = tls_endpoint(hub)
            legacy = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
            legacy.check_hostname = False
            legacy.verify_mode = ssl.CERT_NONE
            legacy.minimum_version = ssl.TLSVersion.TLSv1_2
            legacy.maximum_version = ssl.TLSVersion.TLSv1_2
            legacy.set_ciphers('AES128-SHA:@SECLEVEL=0')
            with socket.create_connection(endpoint, timeout=3) as raw:
                with self.assertRaises(ssl.SSLError):
                    legacy.wrap_socket(raw, server_hostname='localhost')
            peer = connect_tls(self, hub, cert, ssl.TLSVersion.TLSv1_2)
            self.assertEqual(peer.version(), 'TLSv1.2')
            self.assertIn('AES', peer.cipher()[0])
            self.assertIn('GCM', peer.cipher()[0])
            peer.sendall(b'HSUP ADBASE\n')
            session = peer.session
            peer.close()
            resumed = ssl.create_default_context(cafile=cert)
            raw = socket.create_connection(endpoint, timeout=3)
            self.addCleanup(raw.close)
            second = resumed.wrap_socket(raw, server_hostname='localhost', session=session)
            self.addCleanup(second.close)
            self.assertFalse(second.session_reused)
            self.assertIn(second.version(), ('TLSv1.2', 'TLSv1.3'))
            modern = connect_tls(self, hub, cert, ssl.TLSVersion.TLSv1_3)
            self.assertEqual(modern.version(), 'TLSv1.3')
            modern.sendall(b'HSUP ADBASE\n')
            modern.close()
            hub.stop()
            self.assertIn(b'handshakes=3', hub.log)
