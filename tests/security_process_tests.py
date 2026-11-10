"""Security behavior on real TCP/TLS peers; no authenticated ADC session exists."""
import pathlib
import signal
import socket
import ssl
import subprocess
import tempfile
import time
import unittest

from network_process_tests import BINARY, Hub, drain_until_eof
from adc_process_tests import counters
from tls_support import credentials, connect_tls, finish_tls, tls_endpoint


class SecurityProcessTests(unittest.TestCase):
    def hub(self, *args):
        return Hub(self, '--listen', '127.0.0.1:0', *args)

    def closed(self, peer, timeout=4):
        peer.settimeout(timeout)
        try:
            drain_until_eof(peer)
        except ConnectionResetError:
            pass

    def finish(self, peer):
        peer.shutdown(socket.SHUT_WR)
        drain_until_eof(peer)

    def test_per_ip_concurrent_limit_and_reuse(self):
        hub = self.hub('--max-per-ip', '1')
        first = hub.connect()
        self.closed(hub.connect())
        self.finish(first)  # EOF confirms that the slot was released, without a sleep.
        again = hub.connect()
        again.sendall(b'HSUP ADBASE\n')
        self.finish(again)
        hub.stop()
        self.assertIn(b'Security stopped; rejected=1 closed=0', hub.log)
        self.assertEqual(counters(self, hub), (1, 0, 0, 0))

    def test_fixed_window_admissions_and_other_source(self):
        hub = self.hub('--accepts-per-minute', '2')
        for _ in range(2):
            self.finish(hub.connect())
        self.closed(hub.connect())
        # Linux loopback /8 permits another source without using an external host.
        with socket.create_connection(hub.endpoints[0], timeout=3,
                                      source_address=('127.0.0.2', 0)) as independent:
            self.finish(independent)
        hub.stop()
        self.assertIn(b'accepted=3 rejected=1', hub.log)
        self.assertIn(b'Security stopped; rejected=1 closed=0', hub.log)
        self.assertNotIn(b'127.0.0.2', hub.log)

    def test_concurrent_source_isolation(self):
        hub = self.hub('--max-per-ip', '1')
        first = hub.connect()
        first.settimeout(0.1)
        with self.assertRaises(socket.timeout):
            first.recv(1)
        with socket.create_connection(hub.endpoints[0], timeout=3,
                                      source_address=('127.0.0.2', 0)) as independent:
            independent.sendall(b'HSUP ADBASE\n')
            self.finish(independent)
        self.closed(hub.connect())
        self.finish(first)
        hub.stop()
        self.assertIn(b'accepted=2 rejected=1', hub.log)
        self.assertNotIn(b'127.0.0.2', hub.log)

    def test_shared_plain_tls_quota_and_recovery(self):
        with tempfile.TemporaryDirectory(prefix='dc24h-security-tls-') as directory:
            cert, key = credentials(directory)
            hub = self.hub('--adcs-listen', '127.0.0.1:0', '--tls-cert', cert,
                           '--tls-key', key, '--max-per-ip', '1')
            first = hub.connect()
            first.settimeout(0.1)
            with self.assertRaises(socket.timeout):
                first.recv(1)
            with socket.create_connection(tls_endpoint(hub), timeout=3) as excess:
                self.closed(excess)
            self.finish(first)
            secure = connect_tls(self, hub, cert)
            secure.sendall(b'HSUP ADBASE\n')
            finish_tls(self, secure)
            hub.stop()
            self.assertIn(b'Security stopped; rejected=1 closed=0', hub.log)
            self.assertIn(b'handshakes=1 incomplete=0', hub.log)

    def test_frame_rate_and_invalid_threshold(self):
        hub = self.hub('--frames-per-second', '2', '--max-invalid-frames', '2')
        flood = hub.connect()
        flood.sendall(b'HSUP ADBASE\n' * 4)
        self.closed(flood)
        invalid = hub.connect()
        invalid.sendall(b'HMSG invalid\\q\nHMSG invalid\\q\nHSUP IGNORED\n')
        self.closed(invalid)
        healthy = hub.connect()
        healthy.sendall(b'HSUP ADBASE\n')
        self.finish(healthy)
        hub.stop()
        self.assertIn(b'Security stopped; rejected=0 closed=2', hub.log)
        self.assertEqual(counters(self, hub), (3, 0, 2, 0))

    def test_keepalive_consumes_frame_budget(self):
        hub = self.hub('--frames-per-second', '2')
        flood = hub.connect()
        flood.sendall(b'\n\n\nHSUP IGNORED\n')
        self.closed(flood)
        hub.stop()
        self.assertEqual(counters(self, hub), (0, 0, 0, 2))
        self.assertIn(b'Security stopped; rejected=0 closed=1', hub.log)

    def test_partial_frame_deadline_does_not_slide(self):
        hub = self.hub('--partial-frame-timeout', '1', '--idle-timeout', '10',
                       '--log-connections', 'on')
        slow = hub.connect()
        start = time.monotonic()
        slow.sendall(b'HSUP ADB')
        time.sleep(0.45)
        slow.sendall(b'A')
        self.closed(slow)
        self.assertLess(time.monotonic() - start, 2.5)
        healthy = hub.connect()
        healthy.sendall(b'HSUP ADBASE\n')
        self.finish(healthy)
        hub.stop()
        self.assertIn(b'event=partial_frame_timeout', hub.log)
        self.assertIn(b'Security stopped; rejected=0 closed=1', hub.log)
        self.assertEqual(counters(self, hub), (1, 0, 1, 0))

    def test_new_partial_after_completed_frame_gets_its_own_deadline(self):
        hub = self.hub('--partial-frame-timeout', '1', '--idle-timeout', '10')
        peer = hub.connect()
        peer.sendall(b'HSUP AD')
        time.sleep(0.65)
        peer.sendall(b'BASE ADTIGR\nHSUP NEW')
        response = bytearray()
        while response.count(b'\n') < 2:
            response.extend(peer.recv(1024))
        self.assertTrue(response.startswith(b'ISUP ADBASE ADTIGR\nISID '))
        time.sleep(0.5)
        peer.settimeout(0.1)
        with self.assertRaises(socket.timeout):
            peer.recv(1)  # First frame's deadline has passed, second frame's has not.
        self.closed(peer)
        hub.stop()
        self.assertEqual(counters(self, hub), (1, 0, 1, 0))
        self.assertIn(b'Security stopped; rejected=0 closed=1', hub.log)

    def test_oversize_discard_state_keeps_absolute_deadline(self):
        hub = self.hub('--partial-frame-timeout', '1', '--idle-timeout', '10')
        peer = hub.connect()
        peer.sendall(b'x' * 20000)
        time.sleep(0.45)
        peer.sendall(b'more')
        self.closed(peer)
        hub.stop()
        self.assertEqual(counters(self, hub), (0, 0, 1, 0))
        self.assertIn(b'Security stopped; rejected=0 closed=1', hub.log)

    def test_exact_length_and_parameter_limit(self):
        hub = self.hub('--max-invalid-frames', '1')
        valid = hub.connect()
        valid.sendall(b'HMSG ' + b'x' * (16384 - 5) + b'\n')
        self.finish(valid)
        for payload in (b'HMSG ' + b'x' * (16385 - 5) + b'\n',
                        b'HMSG ' + b' '.join([b'x'] * 129) + b'\n', b'HMSG \xff\n'):
            peer = hub.connect()
            peer.sendall(payload)
            self.closed(peer)
        hub.stop()
        self.assertEqual(counters(self, hub), (1, 0, 3, 0))
        self.assertIn(b'Security stopped; rejected=0 closed=3', hub.log)

    def test_tls_handshake_absolute_timeout_and_healthy_recovery(self):
        with tempfile.TemporaryDirectory(prefix='dc24h-security-deadline-') as directory:
            cert, key = credentials(directory)
            hub = self.hub('--adcs-listen', '127.0.0.1:0', '--tls-cert', cert,
                           '--tls-key', key, '--tls-handshake-timeout', '1',
                           '--idle-timeout', '10', '--log-connections', 'on')
            with socket.create_connection(tls_endpoint(hub), timeout=3) as slow:
                start = time.monotonic()
                slow.sendall(b'\x16\x03')
                time.sleep(0.45)
                slow.sendall(b'\x03')
                self.closed(slow)
                self.assertLess(time.monotonic() - start, 2.5)
            peer = connect_tls(self, hub, cert)
            peer.sendall(b'HSUP ADBASE\n')
            finish_tls(self, peer)
            hub.stop()
            self.assertIn(b'event=handshake_timeout', hub.log)
            self.assertIn(b'handshakes=1 incomplete=1', hub.log)
            self.assertIn(b'Security stopped; rejected=0 closed=1', hub.log)

    def test_shipped_example_is_accepted(self):
        example = pathlib.Path(__file__).resolve().parents[1] / 'config' / 'dc24h.ini.example'
        result = subprocess.run([BINARY, '--config', str(example), '--check-config'],
                                capture_output=True, timeout=3)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn(b'Configuration OK', result.stdout)
        self.assertEqual(result.stderr, b'')

    def test_security_config_overrides_and_dry_run(self):
        with tempfile.TemporaryDirectory(prefix='dc24h-security-config-') as directory:
            ini = pathlib.Path(directory) / 'hub.ini'
            ini.write_text('[network]\nlisten=127.0.0.1:0\n[security]\n'
                           'max_connections_per_ip=2\nmax_accepts_per_minute=4\n'
                           'max_frames_per_second=2\nmax_invalid_frames=2\n'
                           'tls_handshake_timeout=2\npartial_frame_timeout=2\n', encoding='utf-8')
            checked = subprocess.run([BINARY, '--config', str(ini), '--check-config',
                                      '--frames-per-second', '3'], capture_output=True, timeout=3)
            self.assertEqual(checked.returncode, 0, checked.stderr)
            hub = Hub(self, '--config', str(ini), '--frames-per-second', '3')
            peer = hub.connect()
            peer.sendall(b'HSUP ADBASE\n' * 4)
            self.closed(peer)
            hub.stop()
            self.assertEqual(counters(self, hub), (3, 0, 0, 0))
            self.assertIn(b'frames_per_second=3', hub.log)
            ini.write_text('[security]\nmax_invalid_frames=SECRET_VALUE\n', encoding='utf-8')
            bad = subprocess.run([BINARY, '--config', str(ini), '--check-config'],
                                 capture_output=True, timeout=3)
            self.assertEqual(bad.returncode, 2)
            self.assertNotIn(b'SECRET_VALUE', bad.stderr)

    def test_cli_rejects_invalid_security_settings(self):
        limits = {'--max-per-ip': 65535, '--accepts-per-minute': 100000,
                  '--frames-per-second': 100000, '--max-invalid-frames': 65535,
                  '--partial-frame-timeout': 3600, '--tls-handshake-timeout': 3600}
        for option, maximum in limits.items():
            for value in ('0', '-1', str(maximum + 1), '1x', ''):
                with self.subTest(option=option, value=value):
                    result = subprocess.run([BINARY, option, value, '--check-config'],
                                            capture_output=True, timeout=3)
                    self.assertEqual(result.returncode, 2, result.stderr)
            result = subprocess.run([BINARY, option, str(maximum), '--check-config'],
                                    capture_output=True, timeout=3)
            self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == '__main__':
    unittest.main()
