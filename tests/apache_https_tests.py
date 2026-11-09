"""Disposable Apache HTTPS instance; never loads or changes the host configuration."""
import grp
import http.client
import os
from pathlib import Path
import pwd
import signal
import socket
import ssl
import subprocess
import sys
import tempfile
import time
import unittest
import warnings

APACHE, MODULES, EXAMPLE = sys.argv[1:4]
del sys.argv[1:4]


class ApacheHttpsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix='dc24h-apache-https-')
        cls.addClassCleanup(cls.temp.cleanup)
        cls.folder = Path(cls.temp.name)
        cls.folder.chmod(0o755)  # Static files must be readable by a dropped-privilege worker.
        cls.public = cls.folder / 'public'
        cls.public.mkdir()
        cls.index = '<!doctype html><title>dc24h.eu</title><h1>HTTPS fixture</h1>\n'
        (cls.public / 'index.html').write_text(cls.index)
        (cls.public / 'assets').mkdir()
        (cls.public / 'assets' / 'panel.js').write_text('export const project = "dc24h.eu";\n')
        (cls.public / 'empty').mkdir()
        (cls.public / '.git').mkdir()
        for name in ['.env', '.git/config', 'admin.token', 'backup.ini.copy', 'private.PEM', 'schema.sql', 'hub.log']:
            (cls.public / name).write_text('PRIVATE_FIXTURE_NOT_FOR_HTTP')
        (cls.folder / 'outside.txt').write_text('PRIVATE_FIXTURE_NOT_FOR_HTTP')
        (cls.public / 'link.txt').symlink_to(cls.folder / 'outside.txt')
        cls.cert = cls.folder / 'certificate.pem'
        cls.key = cls.folder / 'private.key'
        subprocess.run(['openssl', 'req', '-x509', '-newkey', 'rsa:2048', '-nodes', '-days', '1',
                        '-subj', '/CN=dc24h.eu', '-addext', 'subjectAltName=DNS:dc24h.eu,IP:127.0.0.1',
                        '-addext', 'extendedKeyUsage=serverAuth', '-keyout', str(cls.key), '-out', str(cls.cert)],
                       check=True, capture_output=True, timeout=15)
        cls.key.chmod(0o600)
        with socket.socket() as first, socket.socket() as second:
            first.bind(('127.0.0.1', 0)); second.bind(('127.0.0.1', 0))
            cls.http_port = first.getsockname()[1]
            cls.https_port = second.getsockname()[1]
        user = pwd.getpwnam('nobody') if os.geteuid() == 0 else pwd.getpwuid(os.geteuid())
        group = grp.getgrgid(user.pw_gid).gr_name
        modules = ['mpm_event', 'authz_core', 'authz_host', 'headers', 'dir', 'mime', 'alias', 'autoindex', 'socache_shmcb', 'ssl']
        cls.prefix = 'ServerRoot "' + str(cls.folder) + '"\nServerName localhost\n'
        for module in modules:
            cls.prefix += 'LoadModule ' + module + '_module "' + str(Path(MODULES) / ('mod_' + module + '.so')) + '"\n'
        cls.prefix += ('PidFile "' + str(cls.folder / 'apache.pid') + '"\nErrorLog "' + str(cls.folder / 'error.log') +
                       '"\nDefaultRuntimeDir "' + str(cls.folder) + '"\nUser ' + user.pw_name + '\nGroup ' + group + '\n' +
                       'StartServers 1\nServerLimit 1\nThreadsPerChild 8\nMaxRequestWorkers 8\nTimeout 5\n' +
                       'SSLSessionCache "shmcb:' + str(cls.folder / 'ssl-cache') + '(512000)"\n' +
                       'Listen 127.0.0.1:' + str(cls.http_port) + '\nListen 127.0.0.1:' + str(cls.https_port) + '\n')
        mime = cls.folder / 'mime.types'
        mime.write_text('text/html html\napplication/javascript js\n')
        cls.prefix += 'TypesConfig "' + str(mime) + '"\n'
        # Only ports, paths and redirect target port change; TLS/access/header policy is the shipped example.
        cls.source = Path(EXAMPLE).read_text(encoding='utf-8')
        cls.vhosts = (cls.source.replace('<VirtualHost *:80>', '<VirtualHost 127.0.0.1:' + str(cls.http_port) + '>')
                      .replace('<VirtualHost *:443>', '<VirtualHost 127.0.0.1:' + str(cls.https_port) + '>')
                      .replace('https://dc24h.eu/', 'https://dc24h.eu:' + str(cls.https_port) + '/')
                      .replace('/usr/local/share/dc24h/webadmin', str(cls.public))
                      .replace('/etc/dc24h/webadmin-tls/fullchain.pem', str(cls.cert))
                      .replace('/etc/dc24h/webadmin-tls/privkey.pem', str(cls.key)))
        cls.config = cls.folder / 'httpd.conf'
        cls.config.write_text(cls.prefix + cls.vhosts)
        check = cls.check_config(cls.config)
        if check.returncode:
            raise AssertionError('Isolated Apache syntax check failed: ' + check.stderr)
        cls.server = subprocess.Popen([APACHE, '-f', str(cls.config), '-DFOREGROUND'],
                                      stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, start_new_session=True)
        cls.addClassCleanup(cls.stop)
        deadline = time.monotonic() + 8
        while time.monotonic() < deadline:
            if cls.server.poll() is not None:
                raise AssertionError('Isolated Apache exited: ' + (cls.folder / 'error.log').read_text()[-6000:])
            try:
                with socket.create_connection(('127.0.0.1', cls.http_port), timeout=.2):
                    break
            except OSError:
                time.sleep(.05)
        else:
            raise AssertionError('Isolated Apache startup timed out')

    @classmethod
    def check_config(cls, path):
        return subprocess.run([APACHE, '-t', '-f', str(path)], capture_output=True, text=True, timeout=10)

    @classmethod
    def stop(cls):
        if cls.server.poll() is None:
            cls.server.terminate()
        try:
            cls.server.communicate(timeout=5)
        except subprocess.TimeoutExpired:
            os.killpg(cls.server.pid, signal.SIGKILL)
            cls.server.communicate(timeout=3)

    def tls(self, version=None, hostname='dc24h.eu', trusted=True):
        context = ssl.create_default_context(cafile=str(self.cert)) if trusted else ssl.create_default_context()
        if version is not None:
            context.minimum_version = context.maximum_version = version
        raw = socket.create_connection(('127.0.0.1', self.https_port), timeout=3)
        try:
            peer = context.wrap_socket(raw, server_hostname=hostname)
        except Exception:
            raw.close(); raise
        self.addCleanup(peer.close)
        return peer

    def get(self, path='/', version=None):
        peer = self.tls(version)
        peer.sendall(('GET ' + path + ' HTTP/1.1\r\nHost: dc24h.eu\r\nConnection: close\r\n\r\n').encode())
        result = http.client.HTTPResponse(peer)
        result.begin()
        return result.status, dict(result.getheaders()), result.read()

    def test_http_redirect_preserves_path_query_and_ignores_untrusted_host(self):
        for host in ['dc24h.eu', 'untrusted.example']:
            client = http.client.HTTPConnection('127.0.0.1', self.http_port, timeout=3)
            self.addCleanup(client.close)
            client.request('GET', '/assets/panel.js?sample=1', headers={'Host': host})
            result = client.getresponse(); body = result.read()
            self.assertEqual(result.status, 301, body)
            self.assertEqual(result.getheader('Location'), 'https://dc24h.eu:' + str(self.https_port) + '/assets/panel.js?sample=1')
            self.assertIsNone(result.getheader('Strict-Transport-Security'))

    def test_verified_tls12_and_tls13_serve_static_assets(self):
        for version, expected in [(ssl.TLSVersion.TLSv1_2, 'TLSv1.2'), (ssl.TLSVersion.TLSv1_3, 'TLSv1.3')]:
            with self.subTest(version=expected):
                peer = self.tls(version)
                self.assertEqual(peer.version(), expected)
                self.assertNotIn('CBC', peer.cipher()[0])
                status, headers, body = self.get('/', version)
                self.assertEqual(status, 200)
                self.assertEqual(body, self.index.encode())
                self.assertTrue(headers['Content-Type'].startswith('text/html'))
        status, headers, body = self.get('/assets/panel.js')
        self.assertEqual(status, 200); self.assertIn(b'dc24h.eu', body)
        self.assertIn('javascript', headers['Content-Type'])

    def test_headers_apply_to_success_and_denied_or_missing_resources(self):
        for path, expected in [('/', 200), ('/admin-api/v1/status', 403), ('/missing', 404)]:
            with self.subTest(path=path):
                status, headers, _ = self.get(path)
                self.assertEqual(status, expected)
                self.assertEqual(headers['Strict-Transport-Security'], 'max-age=300')
                self.assertIn("frame-ancestors 'none'", headers['Content-Security-Policy'])
                self.assertEqual(headers['X-Content-Type-Options'], 'nosniff')
                self.assertEqual(headers['Referrer-Policy'], 'no-referrer')
                self.assertEqual(headers['X-Frame-Options'], 'DENY')

    def test_private_routes_hidden_paths_secrets_indexes_and_symlinks_are_denied(self):
        for path in ['/admin-api', '/admin-api/v1/status', '/api/v1/health', '/.env', '/.git/config',
                     '/admin.token', '/backup.ini.copy', '/private.PEM', '/schema.sql', '/hub.log', '/empty/', '/link.txt']:
            with self.subTest(path=path):
                status, _, body = self.get(path)
                self.assertEqual(status, 403, path)
                self.assertNotIn(b'PRIVATE_FIXTURE_NOT_FOR_HTTP', body)

    def test_untrusted_certificate_and_wrong_hostname_are_rejected(self):
        with self.assertRaises(ssl.SSLCertVerificationError):
            self.tls(trusted=False)
        with self.assertRaises(ssl.SSLCertVerificationError):
            self.tls(hostname='wrong.invalid')

    def test_legacy_tls_versions_are_rejected(self):
        for version in [ssl.TLSVersion.TLSv1, ssl.TLSVersion.TLSv1_1]:
            with self.subTest(version=version), warnings.catch_warnings():
                warnings.simplefilter('ignore', DeprecationWarning)
                context = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
                context.check_hostname = False; context.verify_mode = ssl.CERT_NONE
                context.minimum_version = context.maximum_version = version
                context.set_ciphers('ALL:@SECLEVEL=0')
                with socket.create_connection(('127.0.0.1', self.https_port), timeout=3) as raw:
                    with self.assertRaises(ssl.SSLError) as failure:
                        context.wrap_socket(raw, server_hostname='dc24h.eu')
                    self.assertIn('ALERT_PROTOCOL_VERSION', failure.exception.reason)

    def test_missing_credentials_fail_configcheck_and_mismatched_key_fails_startup(self):
        wrong_key = self.folder / 'wrong.key'
        subprocess.run(['openssl', 'genpkey', '-algorithm', 'RSA', '-pkeyopt', 'rsa_keygen_bits:2048',
                        '-out', str(wrong_key)], check=True, capture_output=True, timeout=15)
        wrong_key.chmod(0o600)
        for original in [str(self.cert), str(self.key)]:
            bad = self.folder / 'bad.conf'
            bad.write_text(self.prefix + self.vhosts.replace(original, str(self.folder / 'absent.pem')))
            self.assertNotEqual(self.check_config(bad).returncode, 0)
        # Apache -t alone does not promise certificate/key correspondence checks.
        # Use distinct resources so a bind/PID/cache collision cannot mask an SSL failure.
        with socket.socket() as first, socket.socket() as second:
            first.bind(('127.0.0.1', 0)); second.bind(('127.0.0.1', 0))
            first_port, second_port = first.getsockname()[1], second.getsockname()[1]
        bad = self.folder / 'mismatched.conf'
        text = (self.prefix + self.vhosts.replace(str(self.key), str(wrong_key)))
        for original, replacement in [(':' + str(self.http_port), ':' + str(first_port)), (':' + str(self.https_port), ':' + str(second_port)),
                                      ('apache.pid', 'bad-apache.pid'), ('error.log', 'bad-error.log'), ('ssl-cache', 'bad-ssl-cache')]:
            text = text.replace(original, replacement)
        bad.write_text(text)
        result = subprocess.run([APACHE, '-f', str(bad), '-DFOREGROUND'], capture_output=True, text=True, timeout=5)
        self.assertNotEqual(result.returncode, 0)
        errors = (self.folder / 'bad-error.log').read_text().lower()
        self.assertTrue('do not match' in errors or 'mismatch' in errors, errors)


if __name__ == '__main__':
    unittest.main()
