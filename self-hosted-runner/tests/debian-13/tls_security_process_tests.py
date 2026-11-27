"""Hub leaf policy and TLS 1.2/1.3 AEAD on a real listener. No session tickets."""
import pathlib
import socket
import ssl
import subprocess
import tempfile
import unittest

from network_process_tests import BINARY, Hub
from tls_support import (TLS_READY, assert_fatal_tls_alert_or_eof,
                         connect_tls, credentials, secure_client_context, tls_endpoint)


def openssl_cert(directory, name, *extra):
    cert = f'{directory}/{name}.pem'
    key = f'{directory}/{name}.key'
    subprocess.run(['openssl', 'req', '-x509', '-newkey', 'rsa:2048', '-nodes',
                    '-subj', '/CN=localhost', '-keyout', key, '-out', cert, *extra],
                   check=True, capture_output=True, timeout=15)
    return cert, key


class TlsSecurityProcessTests(unittest.TestCase):
    def test_beta09_ca_chain_sni_and_hostname_trust_on_tls12_and_tls13(self):
        """A CA-signed ADCS leaf is accepted only under the pinned trust/host.

        This exercises real OpenSSL chain verification by the client. It does
        not claim that the hub itself rejects arbitrary SNI names: the remote
        client must independently authenticate the expected DNS identity.
        """
        with tempfile.TemporaryDirectory(prefix='dc24h-beta09-chain-') as directory:
            root = pathlib.Path(directory)

            def ca(name):
                key, cert = root / (name + '.key'), root / (name + '.pem')
                subprocess.run(['openssl', 'req', '-x509', '-newkey', 'rsa:2048',
                                '-nodes', '-sha256', '-days', '2', '-subj', '/CN=Disposable Stage I CA',
                                '-keyout', str(key), '-out', str(cert),
                                '-addext', 'basicConstraints=critical,CA:TRUE',
                                '-addext', 'keyUsage=critical,keyCertSign,cRLSign'],
                               check=True, capture_output=True, timeout=20)
                return key, cert

            ca_key, ca_cert = ca('trusted')
            _, stale_ca = ca('untrusted-same-name')
            key, csr, cert = root / 'server.key', root / 'server.csr', root / 'server.pem'
            subprocess.run(['openssl', 'req', '-new', '-newkey', 'rsa:2048', '-nodes',
                            '-subj', '/CN=localhost', '-keyout', str(key), '-out', str(csr)],
                           check=True, capture_output=True, timeout=20)
            extensions = root / 'server.ext'
            extensions.write_text('basicConstraints=critical,CA:FALSE\n'
                                  'keyUsage=critical,digitalSignature,keyEncipherment\n'
                                  'extendedKeyUsage=serverAuth\n'
                                  'subjectAltName=DNS:localhost\n')
            subprocess.run(['openssl', 'x509', '-req', '-in', str(csr),
                            '-CA', str(ca_cert), '-CAkey', str(ca_key),
                            '-CAcreateserial', '-out', str(cert), '-days', '2',
                            '-sha256', '-extfile', str(extensions)],
                           check=True, capture_output=True, timeout=20)
            hub = Hub(self, '--adcs-listen', '127.0.0.1:0',
                      '--tls-cert', str(cert), '--tls-key', str(key),
                      ready_pattern=TLS_READY)
            endpoint = tls_endpoint(hub)
            for version in (ssl.TLSVersion.TLSv1_2, ssl.TLSVersion.TLSv1_3):
                with self.subTest(version=version.name):
                    trust = secure_client_context(str(ca_cert))
                    trust.minimum_version = version
                    trust.maximum_version = version
                    with socket.create_connection(endpoint, timeout=3) as raw:
                        with trust.wrap_socket(raw, server_hostname='localhost') as peer:
                            self.assertEqual(peer.version(), 'TLSv1.2' if version == ssl.TLSVersion.TLSv1_2 else 'TLSv1.3')
                            self.assertFalse(peer.session_reused)
                            peer.sendall(b'HSUP ADBASE\n')
                    for hostile_host in ('untrusted.invalid', '127.0.0.1'):
                        with socket.create_connection(endpoint, timeout=3) as raw:
                            with self.assertRaises(ssl.SSLCertVerificationError):
                                trust.wrap_socket(raw, server_hostname=hostile_host)
                    untrusted = secure_client_context(str(stale_ca))
                    untrusted.minimum_version = version
                    untrusted.maximum_version = version
                    with socket.create_connection(endpoint, timeout=3) as raw:
                        with self.assertRaises(ssl.SSLCertVerificationError):
                            untrusted.wrap_socket(raw, server_hostname='localhost')
            hub.stop()
            for private in (ca_key.read_bytes(), key.read_bytes(), csr.read_bytes()):
                self.assertNotIn(private, hub.log)

    def test_beta08_certificate_cutover_blocks_stale_trust_and_plaintext_on_both_versions(self):
        """Pin a new disposable ADCS leaf; never silently fall back to ADC."""
        with tempfile.TemporaryDirectory(prefix='dc24h-beta08-tls-') as directory:
            root = pathlib.Path(directory)
            first = root / 'first'
            second = root / 'second'
            first.mkdir()
            second.mkdir()
            old_cert, old_key = credentials(first)
            new_cert, new_key = credentials(second)
            old_hub = Hub(self, '--adcs-listen', '127.0.0.1:0',
                          '--tls-cert', old_cert, '--tls-key', old_key,
                          ready_pattern=TLS_READY)
            old_peer = connect_tls(self, old_hub, old_cert)
            old_peer.sendall(b'HSUP ADBASE\n')
            old_peer.close()
            old_hub.stop()

            new_hub = Hub(self, '--adcs-listen', '127.0.0.1:0',
                          '--tls-cert', new_cert, '--tls-key', new_key,
                          ready_pattern=TLS_READY)
            endpoint = tls_endpoint(new_hub)
            for version in (ssl.TLSVersion.TLSv1_2, ssl.TLSVersion.TLSv1_3):
                with self.subTest(version=version.name):
                    stale = secure_client_context(old_cert)
                    stale.minimum_version = version
                    stale.maximum_version = version
                    with socket.create_connection(endpoint, timeout=3) as raw:
                        with self.assertRaises(ssl.SSLCertVerificationError):
                            stale.wrap_socket(raw, server_hostname='localhost')
                    valid = connect_tls(self, new_hub, new_cert, version)
                    self.assertEqual(valid.version(),
                                     'TLSv1.2' if version == ssl.TLSVersion.TLSv1_2 else 'TLSv1.3')
                    self.assertFalse(valid.session_reused)
                    valid.sendall(b'HSUP ADBASE\n')
                    valid.close()

            with socket.create_connection(endpoint, timeout=3) as plain:
                plain.sendall(b'HSUP ADBASE\n')
                plain.shutdown(socket.SHUT_WR)
                plain.settimeout(3)
                try:
                    wire = bytearray()
                    while len(wire) < 7:
                        chunk = plain.recv(7 - len(wire))
                        if not chunk:
                            break
                        wire.extend(chunk)
                except ConnectionResetError:
                    pass  # Reset is equivalent to a fatal rejection.
                # An empty response or a single fatal TLS alert is acceptable.
                # A plaintext application reply or arbitrary bytes are not.
                assert_fatal_tls_alert_or_eof(self, bytes(wire))
            new_hub.stop()
            self.assertIn(b'ADCS stopped; handshakes=', new_hub.log)

    def test_client_context_never_allows_tls_10_or_11(self):
        with tempfile.TemporaryDirectory(prefix='dc24h-tls-client-') as directory:
            cert, _ = credentials(directory)
            self.assertGreaterEqual(secure_client_context(cert).minimum_version, ssl.TLSVersion.TLSv1_2)

    def test_beta06_certificate_replacement_requires_restart_and_rejects_stale_trust(self):
        """Disposable ADCS certificate generation, cutover, and negative host/key checks."""
        with tempfile.TemporaryDirectory(prefix='dc24h-tls-rotation-') as directory:
            root = pathlib.Path(directory)
            old_cert, key = credentials(directory)
            old_ca = root / 'trusted-before.pem'
            old_ca.write_bytes(pathlib.Path(old_cert).read_bytes())
            old_key = root / 'private-before.pem'
            old_key.write_bytes(pathlib.Path(key).read_bytes())
            replacement = root / 'replacement'
            replacement.mkdir()
            newer_cert, newer_key = credentials(replacement)
            hub = Hub(self, '--adcs-listen', '127.0.0.1:0', '--tls-cert', old_cert,
                      '--tls-key', key, ready_pattern=TLS_READY)
            first = connect_tls(self, hub, str(old_ca))
            self.assertEqual(first.getpeercert(binary_form=True), ssl.PEM_cert_to_DER_cert(old_ca.read_text()))
            first.close()

            # Swapping the files alone does not reload a live SSL_CTX.
            pathlib.Path(old_cert).write_bytes(pathlib.Path(newer_cert).read_bytes())
            pathlib.Path(key).write_bytes(pathlib.Path(newer_key).read_bytes())
            still_old = connect_tls(self, hub, str(old_ca))
            still_old.close()
            hub.stop()

            next_hub = Hub(self, '--adcs-listen', '127.0.0.1:0', '--tls-cert', old_cert,
                           '--tls-key', key, ready_pattern=TLS_READY)
            endpoint = tls_endpoint(next_hub)
            with socket.create_connection(endpoint, timeout=3) as raw:
                with self.assertRaises(ssl.SSLError):
                    secure_client_context(old_ca).wrap_socket(raw, server_hostname='localhost')
            replacement_peer = connect_tls(self, next_hub, old_cert)
            replacement_peer.close()
            with socket.create_connection(endpoint, timeout=3) as raw:
                with self.assertRaises(ssl.SSLCertVerificationError):
                    secure_client_context(old_cert).wrap_socket(raw, server_hostname='wrong.example')
            next_hub.stop()

            # Mispaired replacement must fail config validation, not enter service.
            checked = subprocess.run([BINARY, '--check-config', '--adcs-listen', '127.0.0.1:0',
                                      '--tls-cert', old_cert, '--tls-key', str(old_key)],
                                     capture_output=True, timeout=5, stdin=subprocess.DEVNULL)
            self.assertNotEqual(checked.returncode, 0)
            self.assertNotIn(b'BEGIN PRIVATE KEY', checked.stderr)

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

    def test_ca_leaf_and_non_signing_usage_fail_before_adcs_listen(self):
        """A CA private key or non-signing ECDHE leaf is never used as a hub server identity."""
        with tempfile.TemporaryDirectory(prefix='dc24h-tls-leaf-role-') as directory:
            for name, extensions in (
                ('ca-leaf', ('basicConstraints=critical,CA:TRUE',
                             'keyUsage=digitalSignature,keyEncipherment', 'extendedKeyUsage=serverAuth')),
                ('encipher-only', ('basicConstraints=critical,CA:FALSE',
                                   'keyUsage=keyEncipherment', 'extendedKeyUsage=serverAuth')),
            ):
                with self.subTest(name=name):
                    cert, key = openssl_cert(directory, name, '-days', '1',
                                             *(item for ext in extensions for item in ('-addext', ext)))
                    checked = subprocess.run(
                        [BINARY, '--check-config', '--adcs-listen', '127.0.0.1:0',
                         '--tls-cert', cert, '--tls-key', key],
                        capture_output=True, timeout=5, stdin=subprocess.DEVNULL)
                    self.assertNotEqual(checked.returncode, 0)
                    self.assertNotIn(b'listening on', checked.stderr)
                    self.assertNotIn(b'BEGIN PRIVATE KEY', checked.stderr)
                    self.assertNotIn(b'BEGIN CERTIFICATE', checked.stderr)

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
            resumed = secure_client_context(cert)
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

    def test_beta07_version_specific_hostname_trust_refuses_both_tls_versions(self):
        """An independent validating client cannot replace hostname trust with encryption."""
        with tempfile.TemporaryDirectory(prefix='dc24h-tls-hostname-') as directory:
            cert, key = credentials(directory)
            hub = Hub(self, '--adcs-listen', '127.0.0.1:0', '--tls-cert', cert,
                      '--tls-key', key, ready_pattern=TLS_READY)
            endpoint = tls_endpoint(hub)
            for version in (ssl.TLSVersion.TLSv1_2, ssl.TLSVersion.TLSv1_3):
                with self.subTest(version=version.name):
                    context = secure_client_context(cert)
                    context.minimum_version = version
                    context.maximum_version = version
                    with socket.create_connection(endpoint, timeout=3) as raw:
                        with self.assertRaises(ssl.SSLCertVerificationError):
                            context.wrap_socket(raw, server_hostname='wrong.example')
                    legitimate = connect_tls(self, hub, cert, version)
                    self.assertEqual(legitimate.version(), 'TLSv1.2' if version == ssl.TLSVersion.TLSv1_2 else 'TLSv1.3')
                    legitimate.close()
            hub.stop()
