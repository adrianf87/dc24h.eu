"""User registration only after valid ADC identity, over mixed TCP/TLS."""
import pathlib
import tempfile
import unittest

from adc_login_process_tests import Peer, pair
from network_process_tests import Hub, drain_until_eof
from tls_support import credentials, connect_tls


class UserLoginProcessTests(unittest.TestCase):
    def test_unidentified_invalid_identified_and_reconnected_users(self):
        for encrypted in (False, True):
            with self.subTest(encrypted=encrypted), tempfile.TemporaryDirectory(prefix='dc24h-user-login-') as directory:
                cert, _ = credentials(directory)
                ini = pathlib.Path(directory) / 'hub.ini'
                ini.write_text('[network]\nlisten=127.0.0.1:0\nadcs_listen=127.0.0.1:0\n'
                               '[tls]\ncertificate=certificate.pem\nprivate_key=private.pem\n')
                hub = Hub(self, '--config', str(ini), '--log-connections', 'on')

                def peer(tls):
                    return Peer(self, connect_tls(self, hub, cert) if tls else hub.connect())

                watcher = peer(not encrypted)
                watcher.login(b'Watcher', 1)
                candidate = peer(encrypted)
                candidate.handshake()
                watcher.send(b'DSTA ' + watcher.sid + b' ' + candidate.sid + b' 100 BeforeIdentity')
                self.assertTrue(watcher.line().startswith(b'ISTA 140 '))
                # Correct syntax and a canonical CID still do not replace a
                # successful PID/CID proof. No public user may appear on failure.
                bad = candidate.identity(b'Unverified', 3).replace(pair(3)[1], pair(4)[1])
                candidate.send(bad)
                self.assertTrue(candidate.line().startswith(b'ISTA 227 '))
                self.assertEqual(candidate.line().split(b' ')[:2], [b'IQUI', candidate.sid])
                self.assertEqual(drain_until_eof(candidate.peer), b'')
                watcher.send(b'BINF ' + watcher.sid + b' DECheckpoint')
                self.assertIn(b'DECheckpoint', watcher.line())  # No fake join/quit before the echo.
                guest = peer(encrypted)
                guest.login('GośćŻółw'.encode(), 3, fragmented=True)
                joined = watcher.line()
                self.assertTrue(joined.startswith(b'BINF ' + guest.sid + b' '))
                self.assertIn(b' ID' + pair(3)[1], joined)
                duplicate = peer(not encrypted)
                duplicate.handshake()
                duplicate.send(duplicate.identity(b'Duplicate', 3))
                self.assertTrue(duplicate.line().startswith(b'ISTA 224 '))
                self.assertEqual(duplicate.line().split(b' ')[:2], [b'IQUI', duplicate.sid])
                self.assertEqual(drain_until_eof(duplicate.peer), b'')
                routed = b'DSTA ' + watcher.sid + b' ' + guest.sid + b' 100 AfterIdentity'
                watcher.send(routed)
                self.assertEqual(guest.line(), routed)
                guest.send(b'HSTA 299 Done')
                self.assertEqual(drain_until_eof(guest.peer), b'')
                self.assertEqual(watcher.line(), b'IQUI ' + guest.sid)
                fresh = peer(encrypted)
                fresh.login('GośćŻółw'.encode(), 3)
                self.assertNotEqual(fresh.sid, guest.sid)
                self.assertIn(b' ID' + pair(3)[1], watcher.line())
                hub.stop()
                self.assertEqual(hub.log.count(b'event=login_ready'), 3)
                self.assertEqual(hub.log.count(b'event=login_rejected'), 2)
                for private in (pair(3)[0], b'Unverified', b'AfterIdentity', 'GośćŻółw'.encode()):
                    self.assertNotIn(private, hub.log)


if __name__ == '__main__':
    unittest.main()
