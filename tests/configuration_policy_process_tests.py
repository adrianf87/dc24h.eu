"""Effective DB/login policy: pure redacted CLI and live startup snapshots."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

BINARY = str(Path(sys.argv.pop(1)).resolve())


class PolicyTests(unittest.TestCase):
    def call(self, *args):
        return subprocess.run([BINARY, *args], capture_output=True, text=True, timeout=5)

    def test_defaults_overrides_and_language_independent_json(self):
        plain = self.call('--show-configuration')
        self.assertEqual(plain.returncode, 0, plain.stderr)
        self.assertEqual(plain.stderr, '')
        data = json.loads(plain.stdout)
        self.assertEqual(data['database'], {'enabled':False, 'transport':'unix', 'timeout_seconds':5,
            'connect_timeout_seconds':5, 'read_timeout_seconds':5, 'write_timeout_seconds':5})
        self.assertEqual(data['mode'], 'hub')
        self.assertFalse(data['authentication']['enabled'])
        self.assertTrue(data['authentication']['require_tls'])
        self.assertEqual(data, json.loads(self.call('--show-configuration', '--language', 'pl').stdout))
        self.assertIn('--show-configuration', self.call('--language', 'pl', '--help').stdout)

    def test_no_secret_read_database_connection_tls_load_log_or_listener(self):
        with tempfile.TemporaryDirectory(prefix='dc24h-policy-') as directory:
            folder = Path(directory)
            ini = folder / 'hub.ini'
            log = folder / 'unused.log'
            # Missing private files and TLS material would fail on real startup.
            ini.write_text('[network]\nlisten=none\nadcs_listen=127.0.0.1:0\n'
                '[database]\nenabled=on\nsocket=missing-private-db.sock\nname=PrivateDatabase\nuser=PrivateUser\n'
                'password_file=missing-password\ntimeout=8\nread_timeout=2\nwrite_timeout=9\n'
                '[authentication]\nenabled=on\nkey_file=missing-adc-key\nrequire_tls=on\nmax_accounts=77\n'
                'attempts_per_ip=19\nattempts_per_account=7\nattempts_per_second=91\nattempt_window_seconds=45\n'
                'max_tracked_attempt_ips=100\nmax_tracked_attempt_accounts=200\n'
                '[tls]\ncertificate=missing-certificate\nprivate_key=missing-tls-key\n'
                '[logging]\nfile=unused.log\n', encoding='utf-8')
            before = set(folder.iterdir())
            result = self.call('--config', str(ini), '--show-configuration')
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(result.stderr, '')
            self.assertEqual(set(folder.iterdir()), before)
            self.assertFalse(log.exists())
            data = json.loads(result.stdout)
            self.assertEqual(data['database']['connect_timeout_seconds'], 8)
            self.assertEqual(data['database']['read_timeout_seconds'], 2)
            self.assertEqual(data['database']['write_timeout_seconds'], 9)
            self.assertEqual(data['authentication']['max_accounts'], 77)
            self.assertEqual(data['authentication']['attempts_per_ip'], 19)
            for secret in (directory, 'PrivateDatabase', 'PrivateUser', 'missing-', 'password_file', 'key_file', 'socket_path'):
                self.assertNotIn(secret, result.stdout)
            self.assertEqual(self.call('--config', str(ini), '--check-config').returncode, 2)
            for other in ('--check-config','--check-database','--migrate-database','--database-status','--help','--version'):
                self.assertEqual(self.call('--config', str(ini), '--show-configuration', other).returncode, 2)

    def test_strict_disabled_login_and_database_options(self):
        with tempfile.TemporaryDirectory(prefix='dc24h-policy-invalid-') as directory:
            ini = Path(directory) / 'hub.ini'
            for text in ('[authentication]\nenabled=off\nmax_accounts=0\n',
                         '[authentication]\nenabled=off\nmax_accounts=4097\n',
                         '[authentication]\nenabled=off\nrequire_tls=yes\n',
                         '[authentication]\npassword=Private\n',
                         '[database]\nenabled=off\nread_timeout=0\n',
                         '[database]\npassword=Private\n',
                         '[database]\ntimeout=3\ntimeout=4\n'):
                ini.write_text(text)
                result = self.call('--config', str(ini), '--show-configuration')
                self.assertEqual(result.returncode, 2)
                self.assertEqual(result.stdout, '')
                self.assertNotIn('Private', result.stderr)
                self.assertNotIn(directory, result.stderr)


if __name__ == '__main__':
    unittest.main()
