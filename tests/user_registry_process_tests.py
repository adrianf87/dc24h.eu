"""Public SID/CID ownership, routing and reconnect on real ADC/ADCS sockets."""
import pathlib
import tempfile
import unittest

from adc_login_process_tests import Peer, pair
from network_process_tests import Hub, drain_until_eof
from tls_support import credentials, connect_tls


class UserRegistryProcessTests(unittest.TestCase):
    def hub(self):
        directory = tempfile.TemporaryDirectory(prefix='dc24h-user-registry-')
        self.addCleanup(directory.cleanup)
        cert, _ = credentials(directory.name)
        ini = pathlib.Path(directory.name) / 'hub.ini'
        ini.write_text('[network]\nlisten=127.0.0.1:0\nadcs_listen=127.0.0.1:0\n'
                       '[tls]\ncertificate=certificate.pem\nprivate_key=private.pem\n')
        return Hub(self, '--config', str(ini), '--log-connections', 'on'), cert

    def peer(self, hub, cert, encrypted):
        return Peer(self, connect_tls(self, hub, cert) if encrypted else hub.connect())

    def test_cid_uniqueness_sid_routing_rename_and_reconnect(self):
        for encrypted in (False, True):
            with self.subTest(encrypted=encrypted):
                hub, cert = self.hub()
                owner = self.peer(hub, cert, encrypted)
                owner.login('Żółw'.encode(), 11)
                watcher = self.peer(hub, cert, not encrypted)
                listing = watcher.login(b'Watcher', 22)
                self.assertIn(b' NIWatcher', owner.line())
                public_owner = [line for line in listing if line.startswith(b'BINF ' + owner.sid + b' ')]
                self.assertEqual(len(public_owner), 1)
                self.assertIn(b' ID' + pair(11)[1], public_owner[0])
                for attempt in range(2):
                    duplicate = self.peer(hub, cert, not encrypted)
                    duplicate.handshake()
                    duplicate.send(duplicate.identity(b'Candidate' + str(attempt).encode(), 11))
                    self.assertTrue(duplicate.line().startswith(b'ISTA 224 '))
                    self.assertEqual(duplicate.line().split(b' ')[:2], [b'IQUI', duplicate.sid])
                    self.assertEqual(drain_until_eof(duplicate.peer), b'')
                    # A rejected admission never becomes a user-list entry or
                    # routable SID, and cannot steal the original CID owner.
                    watcher.send(b'DSTA ' + watcher.sid + b' ' + duplicate.sid + b' 100 Missing')
                    self.assertTrue(watcher.line().startswith(b'ISTA 140 '))
                    routed = b'DSTA ' + watcher.sid + b' ' + owner.sid + b' 100 RegisteredOwner'
                    watcher.send(routed)
                    self.assertEqual(owner.line(), routed)
                    owner.send(b'BINF ' + owner.sid + b' NI' + 'Łoś'.encode() + b' DECurrentProfile')
                    update = owner.line()
                    self.assertEqual(watcher.line(), update)
                    self.assertIn(b' ID' + pair(11)[1], update)
                    self.assertIn(b' NI' + 'Łoś'.encode(), update)
                previous_sid = owner.sid
                owner.peer.close()
                self.assertEqual(watcher.line(), b'IQUI ' + previous_sid)
                fresh = self.peer(hub, cert, encrypted)
                fresh.login(b'Reconnected', 11)
                self.assertNotEqual(fresh.sid, previous_sid)
                joined = watcher.line()
                self.assertIn(b' ID' + pair(11)[1], joined)
                self.assertTrue(joined.startswith(b'BINF ' + fresh.sid + b' '))
                watcher.send(b'DSTA ' + watcher.sid + b' ' + previous_sid + b' 100 OldLifetime')
                self.assertTrue(watcher.line().startswith(b'ISTA 140 '))
                routed = b'DSTA ' + watcher.sid + b' ' + fresh.sid + b' 100 NewLifetime'
                watcher.send(routed)
                self.assertEqual(fresh.line(), routed)
                # Changing an admitted CID cannot rebind either public index.
                fresh.send(b'BINF ' + fresh.sid + b' ID' + pair(33)[1])
                self.assertTrue(fresh.line().startswith(b'ISTA 2'))
                self.assertEqual(fresh.line().split(b' ')[:2], [b'IQUI', fresh.sid])
                self.assertEqual(drain_until_eof(fresh.peer), b'')
                self.assertEqual(watcher.line(), b'IQUI ' + fresh.sid)
                final = self.peer(hub, cert, encrypted)
                final.login(b'Reconnected', 11)
                self.assertIn(b' ID' + pair(11)[1], watcher.line())
                hub.stop()
                self.assertEqual(hub.log.count(b'event=login_ready'), 4)
                for private in (pair(11)[0], pair(22)[0], b'CurrentProfile', b'RegisteredOwner', b'NewLifetime'):
                    self.assertNotIn(private, hub.log)


if __name__ == '__main__':
    unittest.main()
