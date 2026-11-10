"""Authenticated statistics across anonymous, failed, normal and closed ADC/ADCS lifetimes."""
import json
import pathlib
import socket
import tempfile
import time
import unittest
from concurrent.futures import ThreadPoolExecutor

from adc_login_process_tests import Peer, pair
from api_support import enable_api, fetch_status
from network_process_tests import Hub, drain_until_eof
from tls_support import credentials, connect_tls, tls_endpoint


class StatisticsProcessTests(unittest.TestCase):
    def wait(self, endpoint, token, **expected):
        deadline = time.monotonic() + 4
        while True:
            value = fetch_status(endpoint, token)
            flat = {**value['network'], **value['users']}
            if all(flat[key] == count for key, count in expected.items()):
                return value
            self.assertLess(time.monotonic(), deadline, value)
            time.sleep(0.01)

    def test_application_bytes_exclude_tls_and_api_traffic(self):
        for encrypted in (False, True):
            with self.subTest(encrypted=encrypted), tempfile.TemporaryDirectory(prefix='dc24h-stat-bytes-') as directory:
                cert, _ = credentials(directory)
                ini = pathlib.Path(directory) / 'hub.ini'
                ini.write_text('[network]\nlisten=127.0.0.1:0\nadcs_listen=127.0.0.1:0\n'
                               '[tls]\ncertificate=certificate.pem\nprivate_key=private.pem\n')
                endpoint, token = enable_api(ini, directory)
                hub = Hub(self, '--config', str(ini))
                peer = Peer(self, connect_tls(self, hub, cert) if encrypted else hub.connect())
                before = self.wait(endpoint, token, active_connections=1, online=0,
                                   received_bytes=0, sent_bytes=0, adcs_handshakes=int(encrypted))
                self.assertEqual(before['network']['adcs_handshakes'], int(encrypted))
                wire = b'HSUP ADBASE ADTIGR\n'
                for byte in wire:
                    peer.peer.sendall(bytes([byte]))
                received = len(peer.line()) + len(peer.line()) + 2
                value = self.wait(endpoint, token, received_bytes=len(wire), sent_bytes=received,
                                  successful_logins=0, online=0)
                # Repeated API calls and a keepalive do not create a login.
                peer.peer.sendall(b'\n')
                value = self.wait(endpoint, token, received_bytes=len(wire)+1, sent_bytes=received,
                                  successful_logins=0)
                self.assertEqual(value['network']['adc_frames'], 1)
                peer.peer.close()
                self.wait(endpoint, token, active_connections=0, closed_connections=1)
                hub.stop()

    def test_mixed_lifetimes_counters_and_privacy(self):
        with tempfile.TemporaryDirectory(prefix='dc24h-stats-') as directory:
            cert, _ = credentials(directory)
            ini = pathlib.Path(directory) / 'hub.ini'
            ini.write_text('[network]\nlisten=127.0.0.1:0\nadcs_listen=127.0.0.1:0\n'
                           'max_connections=3\n[tls]\ncertificate=certificate.pem\nprivate_key=private.pem\n'
                           '[security]\nmax_connections_per_ip=8\nmax_pending_logins=8\n')
            endpoint, token = enable_api(ini, directory)
            hub = Hub(self, '--config', str(ini), '--log-connections', 'on')
            initial = self.wait(endpoint, token, active_connections=0, accepted_connections=0, closed_connections=0,
                                rejected_connections=0, online=0, peak_online=0, successful_logins=0)
            self.assertEqual(initial['network']['sent_bytes'], 0)
            first = Peer(self, hub.connect());first.login('PrivateŻółw'.encode(), 1)
            self.wait(endpoint, token, online=1, successful_logins=1)
            second = Peer(self, connect_tls(self, hub, cert));second.login(b'PrivateOther', 2)
            self.assertTrue(first.line().startswith(b'BINF '+second.sid))
            self.wait(endpoint, token, active_connections=2, online=2, peak_online=2, successful_logins=2)
            # An unfinished TLS connection consumes a socket, never an online slot.
            waiting = socket.create_connection(tls_endpoint(hub), timeout=3);self.addCleanup(waiting.close)
            self.wait(endpoint, token, active_connections=3, accepted_connections=3, online=2, peak_connections=3)
            for target in (hub.endpoints[0], tls_endpoint(hub)):
                with socket.create_connection(target, timeout=3) as excess:
                    self.assertEqual(drain_until_eof(excess), b'')
            self.wait(endpoint, token, rejected_connections=2, online=2, closed_connections=0)
            waiting.close()
            self.wait(endpoint, token, active_connections=2, closed_connections=1)
            bad = Peer(self, connect_tls(self, hub, cert));bad.handshake()
            bad.send(bad.identity(b'NeverOnlinePrivate', 3).replace(pair(3)[1], pair(4)[1]))
            self.assertTrue(bad.line().startswith(b'ISTA 227 '));bad.line()
            self.assertEqual(drain_until_eof(bad.peer), b'')
            self.wait(endpoint, token, active_connections=2, accepted_connections=4, closed_connections=2,
                      online=2, successful_logins=2, rejected_connections=2)
            first.send(b'BINF '+first.sid+b' NIRenamedPrivate DEUpdated')
            self.assertEqual(first.line(), second.line())
            # Independent API readers sample only atomics while reactor handles input.
            with ThreadPoolExecutor(max_workers=4) as pool:
                values = list(pool.map(lambda _: fetch_status(endpoint, token), range(12)))
            for value in values:
                self.assertEqual(value['users'], {'online': 2, 'peak_online': 2, 'successful_logins': 2})
                for secret in ('Private', token, pair(1)[0].decode(), pair(1)[1].decode()):
                    self.assertNotIn(secret, json.dumps(value))
            first.peer.close();self.assertEqual(second.line(), b'IQUI '+first.sid)
            self.wait(endpoint, token, active_connections=1, closed_connections=3, online=1)
            replacement = Peer(self, hub.connect());replacement.login(b'RenamedPrivate', 1)
            self.assertTrue(second.line().startswith(b'BINF '+replacement.sid))
            self.wait(endpoint, token, online=2, successful_logins=3, peak_online=2)
            replacement.send(b'HSTA 299 Done');self.assertEqual(drain_until_eof(replacement.peer), b'')
            self.assertEqual(second.line(), b'IQUI '+replacement.sid)
            second.peer.close()
            final = self.wait(endpoint, token, active_connections=0, accepted_connections=5, closed_connections=5,
                              online=0, peak_online=2, successful_logins=3, rejected_connections=2)
            self.assertGreater(final['network']['received_bytes'], initial['network']['received_bytes'])
            self.assertGreater(final['network']['sent_bytes'], 0)
            hub.stop()
            self.assertIn(b'Users stopped; online=0 peak_online=2 successful_logins=3', hub.log)
            self.assertRegex(hub.log, rb'sent_bytes=[0-9]+ closed=5 peak_connections=3')
            self.assertNotIn(b'NeverOnlinePrivate', hub.log)


if __name__ == '__main__':
    unittest.main()
