"""Shared ADC/ADCS connection, session and registered-user capacity on real sockets."""
import pathlib
import re
import socket
import tempfile
import time
import unittest

from adc_login_process_tests import Peer
from api_support import enable_api, fetch_status
from network_process_tests import Hub, drain_until_eof
from tls_support import credentials, connect_tls, tls_endpoint


class UserLimitsProcessTests(unittest.TestCase):
    def start(self, directory, connections, sessions, users):
        cert, _ = credentials(directory)
        ini = pathlib.Path(directory) / 'hub.ini'
        ini.write_text('[network]\nlisten=127.0.0.1:0\nadcs_listen=127.0.0.1:0\n'
                       f'max_connections={connections}\n[users]\nmax_sessions={sessions}\nmax_users={users}\n'
                       '[tls]\ncertificate=certificate.pem\nprivate_key=private.pem\n'
                       '[security]\nmax_connections_per_ip=8\nmax_pending_logins=8\n')
        endpoint, token = enable_api(ini, directory)
        hub = Hub(self, '--config', str(ini), '--log-connections', 'on')
        effective_sessions = min(connections, sessions)
        effective_users = min(effective_sessions, users)
        self.assertIn(f'max_connections={connections} max_sessions={effective_sessions} max_users={effective_users}'.encode(), hub.log)
        return hub, cert, endpoint, token

    def peer(self, hub, cert, encrypted):
        return Peer(self, connect_tls(self, hub, cert) if encrypted else hub.connect())

    def wait_active(self, endpoint, token, expected):
        deadline = time.monotonic() + 3
        while True:
            snapshot = fetch_status(endpoint, token)['network']
            if snapshot['active_connections'] == expected:
                return snapshot
            self.assertLess(time.monotonic(), deadline, snapshot)
            time.sleep(0.01)

    def test_connection_and_session_ceiling_precedes_tls_allocation(self):
        for connections, sessions in ((2, 4), (4, 2)):
            with self.subTest(connections=connections, sessions=sessions), tempfile.TemporaryDirectory(prefix='dc24h-limits-') as directory:
                hub, cert, endpoint, token = self.start(directory, connections, sessions, 8)
                first = self.peer(hub, cert, False);first.handshake()
                waiting = socket.create_connection(tls_endpoint(hub), timeout=3);self.addCleanup(waiting.close)
                hub.wait_for(lambda:hub.log.count(b'event=accepted')==2)
                for encrypted in (False, True):
                    target = tls_endpoint(hub) if encrypted else hub.endpoints[0]
                    with socket.create_connection(target, timeout=3) as excess:
                        self.assertEqual(drain_until_eof(excess), b'')
                snapshot = self.wait_active(endpoint, token, 2)
                self.assertEqual(snapshot['accepted_connections'], 2)
                self.assertEqual(snapshot['adcs_handshakes'], 0)
                self.assertEqual(snapshot['security_rejected'], 0)
                # Closing an anonymous TLS model returns the shared session slot.
                waiting.close();self.wait_active(endpoint, token, 1)
                replacement = self.peer(hub, cert, True);replacement.login(b'Replacement', 3)
                self.assertEqual(self.wait_active(endpoint, token, 2)['accepted_connections'], 3)
                first.peer.close();replacement.peer.close();self.wait_active(endpoint, token, 0)
                hub.stop()
                warning = b'Network connection limit reached' if connections < sessions else b'User session limit reached'
                self.assertEqual(hub.log.count(warning), 1)  # Two rejections share the warning budget.
                self.assertRegex(hub.log, rb'accepted=3 rejected=2')

    def test_user_capacity_rejection_no_presence_and_reuse(self):
        for encrypted in (False, True):
            with self.subTest(encrypted=encrypted), tempfile.TemporaryDirectory(prefix='dc24h-users-limit-') as directory:
                hub, cert, endpoint, token = self.start(directory, 8, 6, 2)
                watcher = self.peer(hub, cert, not encrypted);watcher.login(b'Watcher', 1)
                owner = self.peer(hub, cert, encrypted);owner.login(b'Owner', 2)
                self.assertTrue(watcher.line().startswith(b'BINF '+owner.sid+b' '))
                self.wait_active(endpoint, token, 2)
                for cycle in range(3):
                    rejected = self.peer(hub, cert, encrypted);rejected.handshake()
                    rejected.send(rejected.identity(b'OverCapacityPrivate', 3))
                    self.assertEqual(rejected.line(), b'ISTA 211 Hub\\suser\\slimit\\sreached')
                    self.assertEqual(rejected.line().split()[:2], [b'IQUI', rejected.sid])
                    self.assertEqual(drain_until_eof(rejected.peer), b'')
                    self.wait_active(endpoint, token, 2)
                    watcher.send(b'BINF '+watcher.sid+b' DEFence')
                    self.assertEqual(watcher.line(), b'BINF '+watcher.sid+b' DEFence')
                    self.assertEqual(owner.line(), b'BINF '+watcher.sid+b' DEFence')
                # Updates still work at capacity. Cleanup, rather than loss of
                # readiness alone, returns the user slot for a later admission.
                owner.send(b'HSTA 299 Done');self.assertEqual(drain_until_eof(owner.peer), b'')
                self.assertEqual(watcher.line(), b'IQUI '+owner.sid)
                self.wait_active(endpoint, token, 1)
                replacement = self.peer(hub, cert, encrypted)
                frames = replacement.login(b'Owner', 2)
                self.assertEqual(sum(line.startswith(b'BINF ') for line in frames), 2)
                self.assertTrue(watcher.line().startswith(b'BINF '+replacement.sid+b' '))
                self.assertNotEqual(replacement.sid, owner.sid)
                hub.stop()
                self.assertEqual(hub.log.count(b'event=login_ready'), 3)
                self.assertEqual(hub.log.count(b'event=login_rejected'), 3)
                self.assertNotIn(b'OverCapacityPrivate', hub.log)
                records = re.findall(rb'Connection id=(\d+) transport=(ADC|ADCS) event=([a-z_]+)',hub.log)
                accepted = [ident for ident,_,event in records if event==b'accepted']
                self.assertEqual(len(accepted), 6)
                for ident in accepted:
                    terminal = [event for current,_,event in records if current==ident and event not in (b'accepted',b'tls_ready',b'login_ready')]
                    self.assertEqual(len(terminal), 1, terminal)


if __name__ == '__main__':
    unittest.main()
