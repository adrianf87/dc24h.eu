"""Private DMSG/EMSG on mixed real ADC/ADCS, with an unrelated public observer."""
import pathlib
import tempfile
import unittest

from adc_login_process_tests import Peer
from api_support import enable_api, fetch_status
from network_process_tests import Hub, drain_until_eof
from tls_support import credentials, connect_tls


class PrivateChatProcessTests(unittest.TestCase):
    def test_routing_self_echo_privacy_reconnect_and_errors(self):
        with tempfile.TemporaryDirectory(prefix='dc24h-private-') as directory:
            cert, _ = credentials(directory)
            ini = pathlib.Path(directory)/'hub.ini'
            ini.write_text('[network]\nlisten=127.0.0.1:0\nadcs_listen=127.0.0.1:0\n'
                           '[tls]\ncertificate=certificate.pem\nprivate_key=private.pem\n')
            endpoint, token = enable_api(ini, directory)
            hub = Hub(self, '--config', str(ini), '--log-connections', 'on')
            first = Peer(self, hub.connect());first.login(b'First', 1)
            second = Peer(self, connect_tls(self, hub, cert));second.login(b'Second', 2);first.line()
            observer = Peer(self, hub.connect());observer.login(b'Observer', 3);first.line();second.line()
            pending = Peer(self, connect_tls(self, hub, cert));pending.handshake()
            for sender, target in ((first, second), (second, first)):
                text = r'SecretŻółw\s😀\nline\\path'.encode()
                direct = b'DMSG '+sender.sid+b' '+target.sid+b' '+text+b' PM'+sender.sid
                sender.send(direct+b' XXPrivateUnknown PDPrivatePayload')
                self.assertEqual(target.line(), direct)
                # Fence proves no sender copy and no third-party private frame.
                sender.send(b'BMSG '+sender.sid+b' DirectFence')
                for peer in (first, second, observer):
                    self.assertEqual(peer.line(), b'BMSG '+sender.sid+b' DirectFence')
                echo = b'EMSG '+sender.sid+b' '+target.sid+b' '+text+b' PM'+sender.sid+b' ME1'
                sender.send(echo)
                self.assertEqual(target.line(), echo);self.assertEqual(sender.line(), echo)
                for kind in (b'DMSG', b'EMSG'):
                    self_message = kind+b' '+sender.sid+b' '+sender.sid+b' SecretSelf PM'+sender.sid
                    sender.send(self_message);self.assertEqual(sender.line(), self_message)
                sender.send(b'BMSG '+sender.sid+b' SelfFence')
                for peer in (first, second, observer):
                    self.assertEqual(peer.line(), b'BMSG '+sender.sid+b' SelfFence')
            for sid in (b'ZZZZ', pending.sid):
                for kind in (b'DMSG', b'EMSG'):
                    first.send(kind+b' '+first.sid+b' '+sid+b' SecretUnavailable PM'+first.sid)
                    response = first.line();self.assertTrue(response.startswith(b'ISTA 140 '));self.assertIn(b' FC'+kind, response)
            first.send(b'EMSG '+first.sid+b' '+second.sid+b' SecretGroup PM'+observer.sid)
            self.assertTrue(first.line().startswith(b'ISTA 125 '))
            # Direct routing without PM remains targeted, with PM absence preserved.
            raw = b'DMSG '+second.sid+b' '+first.sid+b' SecretUnflagged ME0'
            second.send(raw);self.assertEqual(first.line(), raw)
            first.send(b'BMSG '+first.sid+b' PrivacyFence')
            for peer in (first, second, observer):
                self.assertEqual(peer.line(), b'BMSG '+first.sid+b' PrivacyFence')
            pending.send(pending.identity(b'Later', 4))
            self.assertTrue(pending.line().startswith(b'IINF '))
            while True:
                line = pending.line();self.assertTrue(line.startswith(b'BINF '), 'Private backlog leaked into welcome')
                if line.startswith(b'BINF '+pending.sid+b' '):break
            self.assertTrue(pending.line().startswith(b'IMSG Welcome\\sto\\sdc24h.eu.'))
            for peer in (first, second, observer):
                self.assertTrue(peer.line().startswith(b'BINF '+pending.sid))
            exact = b'DMSG '+first.sid+b' '+second.sid+b' '+b'x'*(16384-15-7)+b' PM'+first.sid
            first.send(exact);self.assertEqual(second.line(), exact)
            second.peer.close()
            for peer in (first, observer, pending):
                self.assertEqual(peer.line(), b'IQUI '+second.sid)
            first.send(b'EMSG '+first.sid+b' '+second.sid+b' SecretOld PM'+first.sid)
            self.assertTrue(first.line().startswith(b'ISTA 140 '))
            replacement = Peer(self, connect_tls(self, hub, cert));replacement.login(b'Second', 2)
            for peer in (first, observer, pending):peer.line()
            self.assertNotEqual(replacement.sid, second.sid)
            first.send(b'DMSG '+first.sid+b' '+second.sid+b' SecretStale PM'+first.sid)
            self.assertTrue(first.line().startswith(b'ISTA 140 '))
            current = b'EMSG '+first.sid+b' '+replacement.sid+b' SecretCurrent PM'+first.sid
            first.send(current);self.assertEqual(replacement.line(), current);self.assertEqual(first.line(), current)
            # A forged source uses the original fatal source-binding path.
            replacement.send(b'DMSG '+first.sid+b' '+observer.sid+b' SecretSpoof PM'+first.sid)
            self.assertTrue(replacement.line().startswith(b'ISTA 240 '));replacement.line()
            self.assertEqual(drain_until_eof(replacement.peer), b'')
            for peer in (first, observer, pending):self.assertEqual(peer.line(), b'IQUI '+replacement.sid)
            snapshot = str(fetch_status(endpoint, token))
            for text in ('Secret', 'PrivateUnknown', 'PrivatePayload'):
                self.assertNotIn(text, snapshot)
            hub.stop()
            for text in (b'Secret', b'PrivateUnknown', b'PrivatePayload'):
                self.assertNotIn(text, hub.log)


if __name__ == '__main__':
    unittest.main()
