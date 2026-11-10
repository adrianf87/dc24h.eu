"""Configuration integration tests using real sockets and clean temporary directories."""
import os
import pathlib
import re
import signal
import socket
import subprocess
import sys
import tempfile
import time
import unittest

from network_process_tests import BINARY, Hub, drain_until_eof
from tls_support import credentials, connect_tls, finish_tls, TLS_READY

class ConfigurationProcessTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="dc24h-config-")
        self.addCleanup(self.directory.cleanup)
        self.root = pathlib.Path(self.directory.name)

    def create_config(self, body):
        path = self.root / 'dc24h.ini'
        path.write_text(body, encoding='utf-8')
        return path

    def run_cli(self, *args):
        return subprocess.run([BINARY, *map(str, args)], capture_output=True, timeout=5)

    def test_check_config_and_precedence(self):
        ini = self.create_config('[network]\nlisten = 127.0.0.1:0\n'
                                 'max_connections = 4\n[logging]\nlevel = info\n')
        result = self.run_cli('--config', ini, '--check-config', '--max-connections', '2')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn(b'Configuration OK', result.stdout)
        self.assertNotIn(b'Core running', result.stderr)

    def test_missing_file_unknown_key_and_unsafe_output(self):
        result = self.run_cli('--config', self.root / 'missing.ini')
        self.assertEqual(result.returncode, 2)
        ini = self.create_config('[logging]\nlevel = debug\n')
        self.assertEqual(self.run_cli('--config', ini, '--check-config').returncode, 2)
        ini = self.create_config('[logging]\nfile = missing-directory/log.txt\n')
        result = self.run_cli('--config', ini, '--listen', '127.0.0.1:0')
        self.assertEqual(result.returncode, 1)
        self.assertIn(b'Startup failure', result.stderr)
        self.assertNotIn(b'Network listening', result.stderr)

    def test_loopback_transport_and_file_logging(self):
        log = self.root / 'server.log'
        ini = self.create_config('[network]\nlisten = 127.0.0.1:0\n'
                                 'max_connections = 8\nidle_timeout = 10\n'
                                 '[logging]\nlevel = info\nfile = server.log\n')
        process = subprocess.Popen([BINARY, '--config', str(ini)],
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        self.addCleanup(self.cleanup_process, process)
        deadline = time.monotonic() + 4
        matched = None
        while time.monotonic() < deadline:
            if log.exists():
                matched = re.search(rb'Network listening on 127.0.0.1:(\d+)',
                                    log.read_bytes())
            if matched:
                break
            self.assertIsNone(process.poll(), 'Process died before ready')
            time.sleep(0.02)
        self.assertIsNotNone(matched, 'Missing network readiness log')
        with socket.create_connection(('127.0.0.1', int(matched.group(1))), timeout=3) as client:
            client.sendall(b'HSUP ADBASE\n')
            client.shutdown(socket.SHUT_WR)
            self.assertEqual(client.recv(1), b'')
        process.send_signal(signal.SIGTERM)
        stdout, stderr = process.communicate(timeout=3)
        self.assertEqual(process.returncode, 0, stderr)
        self.assertEqual(stdout, b'')
        self.assertEqual(stderr, b'')
        self.assertEqual(os.stat(log).st_mode & 0o777, 0o600)
        report = log.read_bytes()
        self.assertIn(b'Core stopped', report)
        self.assertIn(b'ADC stopped; frames=1', report)

    @staticmethod
    def cleanup_process(process):
        if process.poll() is None:
            process.kill()
        process.communicate()

    def test_stderr_and_error_level(self):
        ini = self.create_config('[network]\nlisten = 127.0.0.1:0\n'
                                 '[logging]\nlevel = error\nfile = stderr\n')
        process = subprocess.Popen([BINARY, '--config', str(ini)],
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        self.addCleanup(self.cleanup_process, process)
        time.sleep(0.2)
        self.assertIsNone(process.poll())
        process.terminate()
        stdout, stderr = process.communicate(timeout=3)
        self.assertEqual(process.returncode, 0, stderr)
        self.assertEqual(stderr, b'')
        self.assertEqual(stdout, b'')

    def test_check_does_not_bind_or_create_log(self):
        with socket.socket() as occupied:
            occupied.bind(('127.0.0.1', 0))
            occupied.listen()
            ini = self.create_config(f'[network]\nlisten=127.0.0.1:{occupied.getsockname()[1]}\n'
                                     '[logging]\nfile=not-created.log\n')
            result = self.run_cli('--config', ini, '--check-config')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertFalse((self.root / 'not-created.log').exists())
        self.assertEqual(result.stderr, b'')

    def test_cli_limits_really_override_file(self):
        ini = self.create_config('[network]\nlisten=127.0.0.1:0\nmax_connections=8\nidle_timeout=20\n')
        hub = Hub(self, '--max-connections', '1', '--config', str(ini), '--idle-timeout', '1')
        first = hub.connect()
        first.settimeout(0.1)
        with self.assertRaises(socket.timeout):
            first.recv(1)
        excess = hub.connect()
        self.assertEqual(excess.recv(1), b'')
        first.settimeout(3)
        drain_until_eof(first)
        next_peer = hub.connect()
        next_peer.sendall(b'HSUP ADBASE\n')
        next_peer.shutdown(socket.SHUT_WR)
        drain_until_eof(next_peer)
        hub.stop()
        self.assertIn(b'accepted=2 rejected=1', hub.log)

    def test_tls_file_paths_and_partial_cli_settings(self):
        cert, key = credentials(self.root)
        ini = self.create_config('[network]\nadcs_listen=127.0.0.1:0\n'
                                 '[tls]\ncertificate=certificate.pem\n')
        checked = self.run_cli('--config', ini, '--tls-key', key, '--check-config')
        self.assertEqual(checked.returncode, 0, checked.stderr)
        hub = Hub(self, '--config', str(ini), '--tls-key', key, ready_pattern=TLS_READY)
        peer = connect_tls(self, hub, cert)
        peer.sendall(b'HSUP ADBASE\n')
        finish_tls(self, peer)
        hub.stop()
        self.assertNotIn(b'Network listening on', hub.log)
        self.assertIn(b'handshakes=1 incomplete=0', hub.log)
        # Malformed credentials must fail the dry-run as well as actual startup.
        (self.root / 'certificate.pem').write_text('INVALID_CERTIFICATE_SECRET')
        checked = self.run_cli('--config', ini, '--tls-key', key, '--check-config')
        self.assertEqual(checked.returncode, 2)
        self.assertNotIn(b'INVALID_CERTIFICATE_SECRET', checked.stderr)

    def test_files_cannot_be_overwritten_through_log_aliases(self):
        cert, key = credentials(self.root)
        ini = self.create_config('[network]\nadcs_listen=127.0.0.1:0\n'
                                 '[tls]\ncertificate=certificate.pem\nprivate_key=private.pem\n')
        alias = self.root / 'key-alias'
        os.link(key, alias)
        directory_alias = self.root / 'alias'
        directory_alias.symlink_to(self.root, target_is_directory=True)
        for target in (ini, cert, key, alias, directory_alias / 'private.pem'):
            with self.subTest(target=target):
                before = pathlib.Path(target).read_bytes()
                result = self.run_cli('--config', ini, '--log-file', target)
                self.assertEqual(result.returncode, 2, result.stderr)
                self.assertEqual(pathlib.Path(target).read_bytes(), before)
        # A non-TLS log hardlinked to any existing file is rejected by fstat too.
        target = self.root / 'plain-data'
        target.write_text('KEEP_ME')
        hardlink = self.root / 'plain-link'
        os.link(target, hardlink)
        result = self.run_cli('--listen', '127.0.0.1:0', '--log-file', hardlink)
        self.assertEqual(result.returncode, 1)
        self.assertEqual(target.read_text(), 'KEEP_ME')

    def test_fifo_symlink_and_directory_rejected_without_hanging(self):
        fifo = self.root / 'pipe'
        os.mkfifo(fifo)
        ini = self.create_config('[network]\nlisten=127.0.0.1:0\n')
        symlink = self.root / 'linked.ini'
        symlink.symlink_to(ini)
        for path in (fifo, symlink, self.root):
            with self.subTest(config=path):
                self.assertEqual(self.run_cli('--config', path, '--check-config').returncode, 2)
        for path in (fifo, symlink, self.root):
            with self.subTest(log=path):
                result = self.run_cli('--listen', '127.0.0.1:0', '--log-file', path)
                self.assertEqual(result.returncode, 1, result.stderr)
                self.assertNotIn(b'Network listening', result.stderr)

    def test_no_untrusted_configuration_in_diagnostics_and_polish_selection(self):
        ini = self.create_config('[logging]\nlevel=SECRET_UNTRUSTED_VALUE\n')
        result = self.run_cli('--config', ini, '--check-config')
        self.assertEqual(result.returncode, 2)
        self.assertIn(b'line 2', result.stderr)
        self.assertNotIn(b'SECRET_UNTRUSTED_VALUE', result.stderr)
        ini = self.create_config('[general]\nlanguage=pl\n')
        result = self.run_cli('--config', ini, '--check-config')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('Konfiguracja poprawna'.encode(), result.stdout)
        result = self.run_cli('--language', 'us', '--config', ini, '--check-config')
        self.assertIn(b'Configuration OK', result.stdout)

if __name__ == '__main__':
    unittest.main()
