"""Main public chat: mixed ADC/ADCS ordered fanout, exact wire boundary and privacy."""
import pathlib
import tempfile
import unittest

from adc_login_process_tests import Peer
from network_process_tests import Hub
from tls_support import credentials, connect_tls


class PublicChatProcessTests(unittest.TestCase):
    def test_ordered_bursts_large_frames_and_unsupported_private(self):
        with tempfile.TemporaryDirectory(prefix='dc24h-public-') as directory:
            cert, _ = credentials(directory)
            ini = pathlib.Path(directory)/'hub.ini'
            ini.write_text('[network]\nlisten=127.0.0.1:0\nadcs_listen=127.0.0.1:0\n'
                           '[tls]\ncertificate=certificate.pem\nprivate_key=private.pem\n')
            hub = Hub(self, '--config', str(ini), '--log-connections', 'on')
            peers = [Peer(self, hub.connect()), Peer(self, connect_tls(self, hub, cert)), Peer(self, hub.connect())]
            for owner, peer in enumerate(peers):
                peer.login(('Public'+str(owner)).encode(), owner+1)
                for observer in peers[:owner]:
                    self.assertTrue(observer.line().startswith(b'BINF '+peer.sid))
            # AW2 remains a declaration, not a forced public-chat exclusion.
            peers[2].send(b'BINF '+peers[2].sid+b' AW2')
            for peer in peers:
                self.assertEqual(peer.line(), b'BINF '+peers[2].sid+b' AW2')
            for sender in peers[:2]:
                expected = []
                for index in range(20):
                    body = b'BMSG '+sender.sid+b' PublicBurst'+str(index).encode()+r'\sŻółw\s😀\nline\\path'.encode()
                    if index == 10:
                        update = b'BINF '+sender.sid+b' DEOrdered'
                        sender.send(update);expected.append(update)
                    sender.send(body+b' ME01 XXPrivateExtension PDPrivatePayload')
                    expected.append(body+b' ME1')
                for peer in peers:
                    self.assertEqual([peer.line() for _ in expected], expected)
            exact = b'BMSG '+peers[0].sid+b' '+b'x'*(16384-10)
            peers[0].send(exact)
            for peer in peers:
                self.assertEqual(peer.line(), exact)
            # Too-long syntax is discarded; the following body must still fan out.
            peers[1].send(b'BMSG '+peers[1].sid+b' '+b'x'*(16384-9))
            peers[1].send(b'BMSG '+peers[1].sid+b' AfterOversize')
            for peer in peers:
                self.assertEqual(peer.line(), b'BMSG '+peers[1].sid+b' AfterOversize')
            for owner in (0, 1):
                source = peers[owner]
                source.send(b'BMSG '+source.sid+b' PrivateIntent PM'+source.sid)
                self.assertTrue(source.line().startswith(b'ISTA 125 '))
                source.send(b'BMSG '+source.sid+b' PublicFence')
                for peer in peers:
                    self.assertEqual(peer.line(), b'BMSG '+source.sid+b' PublicFence')
            # Escaped whitespace remains one nonempty text value, never trimmed.
            peers[2].send(b'BMSG '+peers[2].sid+b' \\s\\s ME0')
            for peer in peers:
                self.assertEqual(peer.line(), b'BMSG '+peers[2].sid+b' \\s\\s ME0')
            hub.stop()
            for secret in (b'PublicBurst', b'PrivateExtension', b'PrivatePayload', b'PrivateIntent'):
                self.assertNotIn(secret, hub.log)


if __name__ == '__main__':
    unittest.main()
