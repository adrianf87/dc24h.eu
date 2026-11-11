"""Public Chat Core dispatch through real ADC/ADCS and private authenticated status."""
import pathlib
import tempfile
import unittest

from adc_login_process_tests import Peer
from api_support import enable_api, fetch_status
from network_process_tests import Hub, drain_until_eof
from tls_support import credentials, connect_tls


class ChatProcessTests(unittest.TestCase):
    def test_public_action_sanitization_order_and_private_isolation(self):
        with tempfile.TemporaryDirectory(prefix='dc24h-chat-') as directory:
            cert, _ = credentials(directory)
            ini = pathlib.Path(directory)/'hub.ini'
            ini.write_text('[network]\nlisten=127.0.0.1:0\nadcs_listen=127.0.0.1:0\n'
                           '[tls]\ncertificate=certificate.pem\nprivate_key=private.pem\n')
            api, token = enable_api(ini, directory)
            hub = Hub(self, '--config', str(ini), '--log-connections', 'on')
            first = Peer(self, hub.connect());first.login(b'First', 1)
            second = Peer(self, connect_tls(self, hub, cert));second.login(b'Second', 2);first.line()
            waiting = Peer(self, hub.connect());waiting.handshake()
            for sender in (first, second):
                message = b'BMSG '+sender.sid+b' '+r'ChatPrivateŻółw\s😀\nline\\path'.encode()
                sender.send(message+b' ME01 XXSecretExtension PDSecretPayload')
                expected = message+b' ME1'
                self.assertEqual(first.line(), expected)
                self.assertEqual(second.line(), expected)
            # Existing FIFO orders public INF changes and subsequent chat frames.
            first.send(b'BINF '+first.sid+b' NIRenamed')
            first.send(b'BMSG '+first.sid+b' AfterRename ME0')
            for peer in (first, second):
                self.assertEqual(peer.line(), b'BINF '+first.sid+b' NIRenamed')
                self.assertEqual(peer.line(), b'BMSG '+first.sid+b' AfterRename ME0')
            for message in (b'BMSG '+first.sid+b' PrivateGroup PM'+first.sid,
                            b'FMSG '+first.sid+b' +BASE PrivateFeature', b'HMSG PrivateHub'):
                first.send(message)
                self.assertTrue(first.line().startswith(b'ISTA 125 '))
            direct = b'DMSG '+first.sid+b' '+second.sid+b' PrivateDirect PM'+first.sid
            first.send(direct);self.assertEqual(second.line(), direct)
            echo = b'EMSG '+first.sid+b' '+second.sid+b' PrivateEcho PM'+first.sid
            first.send(echo)
            self.assertEqual(first.line(), echo);self.assertEqual(second.line(), echo)
            # A recovering syntax failure must not break the next valid chat.
            second.send(b'BMSG '+second.sid+b' Bad\\q')
            second.send(b'BMSG '+second.sid+b' AfterSyntax')
            self.assertEqual(first.line(), b'BMSG '+second.sid+b' AfterSyntax')
            self.assertEqual(second.line(), b'BMSG '+second.sid+b' AfterSyntax')
            # Anonymous/pre-NORMAL clients never receive already-published chats.
            waiting.send(waiting.identity(b'Later', 3))
            frames = [waiting.line()]
            self.assertTrue(frames[0].startswith(b'IINF '))
            while not frames[-1].startswith(b'BINF '+waiting.sid+b' '):
                frames.append(waiting.line())
            self.assertTrue(waiting.line().startswith(b'IMSG Welcome\\sto\\sdc24h.eu.'))
            self.assertTrue(all(not line.startswith(b'BMSG ') for line in frames))
            self.assertTrue(first.line().startswith(b'BINF '+waiting.sid+b' '))
            self.assertTrue(second.line().startswith(b'BINF '+waiting.sid+b' '))
            # Disconnect and same-CID reconnect must deliver only to the new SID.
            second.peer.close();self.assertEqual(first.line(), b'IQUI '+second.sid)
            self.assertEqual(waiting.line(), b'IQUI '+second.sid)
            replacement = Peer(self, connect_tls(self, hub, cert));replacement.login(b'Second', 2)
            self.assertNotEqual(replacement.sid, second.sid)
            first.line();waiting.line()
            first.send(b'BMSG '+first.sid+b' Reconnected')
            for peer in (first, waiting, replacement):
                self.assertEqual(peer.line(), b'BMSG '+first.sid+b' Reconnected')
            # A forged source and malformed semantics still use the fatal path.
            replacement.send(b'BMSG '+first.sid+b' SpoofPrivate')
            self.assertTrue(replacement.line().startswith(b'ISTA 240 '));replacement.line()
            self.assertEqual(drain_until_eof(replacement.peer), b'')
            self.assertEqual(first.line(), b'IQUI '+replacement.sid)
            self.assertEqual(waiting.line(), b'IQUI '+replacement.sid)
            status = fetch_status(api, token)
            for private in ('ChatPrivate', 'SecretExtension', 'SecretPayload', 'PrivateDirect'):
                self.assertNotIn(private, str(status))
            hub.stop()
            for private in (b'ChatPrivate', b'SecretExtension', b'SecretPayload', b'PrivateDirect', b'SpoofPrivate'):
                self.assertNotIn(private, hub.log)


if __name__ == '__main__':
    unittest.main()
