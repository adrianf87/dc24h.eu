"""Nick changes and collisions through the real mixed ADC/ADCS reactor."""
import pathlib
import tempfile
import unittest

from adc_login_process_tests import Peer
from network_process_tests import Hub, drain_until_eof
from tls_support import credentials, connect_tls


class NickManagerProcessTests(unittest.TestCase):
    def hub(self):
        directory = tempfile.TemporaryDirectory(prefix='dc24h-nicknames-')
        self.addCleanup(directory.cleanup)
        cert, _ = credentials(directory.name)
        ini = pathlib.Path(directory.name) / 'hub.ini'
        ini.write_text('[network]\nlisten=127.0.0.1:0\nadcs_listen=127.0.0.1:0\n'
                       '[tls]\ncertificate=certificate.pem\nprivate_key=private.pem\n')
        return Hub(self, '--config', str(ini)), cert

    def peer(self, hub, cert, encrypted):
        return Peer(self, connect_tls(self, hub, cert) if encrypted else hub.connect())

    def test_rename_reuse_collision_and_cleanup(self):
        for encrypted in (False, True):
            with self.subTest(encrypted=encrypted):
                hub, cert = self.hub()
                first = self.peer(hub, cert, encrypted)
                first.login(b'Original', 1)
                observer = self.peer(hub, cert, not encrypted)
                observer.login(b'Observer', 2)
                self.assertIn(b' NIObserver', first.line())
                name = 'Żółw'.encode()
                for target in (name, name, b'N' * 64):
                    first.send(b'BINF ' + first.sid + b' NI' + target + b' DEUpdated')
                    update = first.line()
                    self.assertEqual(observer.line(), update)
                    self.assertIn(b' NI' + target, update)
                    self.assertIn(b' DEUpdated', update)
                # The old name is reusable while the renamed owner is still live.
                replacement = self.peer(hub, cert, encrypted)
                replacement.login(b'Original', 3)
                self.assertIn(b' NIOriginal', first.line())
                self.assertIn(b' NIOriginal', observer.line())
                replacement.send(b'BINF ' + replacement.sid + b' NI' + b'N' * 64 + b' DEAttack')
                self.assertTrue(replacement.line().startswith(b'ISTA 222 '))
                self.assertEqual(replacement.line().split(b' ')[:2], [b'IQUI', replacement.sid])
                self.assertEqual(drain_until_eof(replacement.peer), b'')
                # Only a quit is relayed after a rejected update, never its fields.
                self.assertEqual(observer.line(), b'IQUI ' + replacement.sid)
                self.assertEqual(first.line(), b'IQUI ' + replacement.sid)
                first.peer.close()
                self.assertEqual(observer.line(), b'IQUI ' + first.sid)
                fresh = self.peer(hub, cert, encrypted)
                listing = fresh.login(b'N' * 64, 1)
                self.assertFalse(any(b' NIOriginal' in line or b'DEAttack' in line for line in listing))
                self.assertIn(b' NI' + b'N' * 64, observer.line())
                hub.stop()

    def test_invalid_names_do_not_reserve_or_relay(self):
        for encrypted in (False, True):
            hub, cert = self.hub()
            observer = self.peer(hub, cert, not encrypted)
            observer.login(b'Observer', 1)
            for offset, invalid in enumerate((b'Bad\\sName', b'Bad\xe2\x80\x8b', b'X' * 65)):
                with self.subTest(encrypted=encrypted, invalid=invalid):
                    peer = self.peer(hub, cert, encrypted)
                    peer.login(b'Candidate', 2 + offset)
                    self.assertIn(b' NICandidate', observer.line())
                    # Non-NFC frames are discarded by the stream parser before
                    # INF validation. A following valid update proves recovery
                    # and that the original name remains authoritative.
                    peer.send(b'BINF ' + peer.sid + b' NIe\xcc\x81')
                    peer.send(b'BINF ' + peer.sid + b' NICandidate DEStillHere')
                    update = peer.line()
                    self.assertEqual(observer.line(), update)
                    self.assertIn(b' NICandidate', update)
                    self.assertIn(b' DEStillHere', update)
                    peer.send(b'BINF ' + peer.sid + b' NI' + invalid)
                    self.assertTrue(peer.line().startswith(b'ISTA 2'))
                    self.assertEqual(peer.line().split(b' ')[:2], [b'IQUI', peer.sid])
                    self.assertEqual(drain_until_eof(peer.peer), b'')
                    self.assertEqual(observer.line(), b'IQUI ' + peer.sid)
            # All invalid attempts released the original name after close.
            fresh = self.peer(hub, cert, encrypted)
            fresh.login(b'Candidate', 9)
            self.assertIn(b' NICandidate', observer.line())
            hub.stop()


if __name__ == '__main__':
    unittest.main()
