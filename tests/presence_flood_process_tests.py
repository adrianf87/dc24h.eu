"""Ready-session INF fanout cap on actual ADC/ADCS. Chat stays independent."""
import pathlib
import socket
import tempfile
import unittest

from adc_login_process_tests import Peer
from network_process_tests import Hub
from tls_support import credentials, connect_tls


class PresenceFloodProcessTests(unittest.TestCase):
    def test_ready_inf_cap_notice_and_independent_chat(self):
        for encrypted in (False, True):
            with self.subTest(encrypted=encrypted), tempfile.TemporaryDirectory(prefix='dc24h-presence-') as directory:
                cert, _ = credentials(directory)
                ini = pathlib.Path(directory) / 'hub.ini'
                ini.write_text('[network]\nlisten=127.0.0.1:0\nadcs_listen=127.0.0.1:0\n'
                               '[tls]\ncertificate=certificate.pem\nprivate_key=private.pem\n')
                hub = Hub(self, '--config', str(ini), '--log-connections', 'on')

                def peer(tls):
                    return Peer(self, connect_tls(self, hub, cert) if tls else hub.connect())

                owner, observer = peer(encrypted), peer(not encrypted)
                owner.login(b'Owner', 1)
                observer.login(b'Observer', 2)
                owner.line()
                for index in range(8):
                    frame = b'BINF ' + owner.sid + b' SS' + str(index).encode()
                    owner.send(frame)
                    self.assertEqual(owner.line(), frame)
                    self.assertEqual(observer.line(), frame)
                owner.send(b'BINF ' + owner.sid + b' SS8')
                notice = owner.line()
                self.assertTrue(notice.startswith(b'ISTA 100 Presence\\supdate\\srate\\slimit\\sexceeded FCBINF'), notice)
                self.assertNotIn(b'SS8', notice)
                observer.peer.settimeout(0.2)
                with self.assertRaises(socket.timeout):
                    observer.peer.recv(1)
                owner.send(b'BINF ' + owner.sid + b' SS9')
                owner.peer.settimeout(0.2)
                with self.assertRaises(socket.timeout):
                    owner.peer.recv(1)
                owner.peer.settimeout(None)
                observer.peer.settimeout(None)
                message = b'BMSG ' + owner.sid + b' ValidAfterSyntax'
                owner.send(message)
                self.assertEqual(owner.line(), message)
                self.assertEqual(observer.line(), message)
                hub.stop()


if __name__ == '__main__':
    unittest.main()
