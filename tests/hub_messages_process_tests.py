"""System welcome IMSG, NORMAL ordering and client-origin impersonation rejection."""
import json
import pathlib
import tempfile
import unittest

from adc_login_process_tests import Peer, HUB_WELCOME
from api_support import enable_api, fetch_status
from network_process_tests import Hub, drain_until_eof
from tls_support import credentials, connect_tls

WELCOME = HUB_WELCOME


class HubMessagesProcessTests(unittest.TestCase):
    def test_welcome_order_reconnect_spoof_and_privacy(self):
        with tempfile.TemporaryDirectory(prefix='dc24h-system-') as directory:
            cert, _ = credentials(directory)
            ini = pathlib.Path(directory)/'hub.ini'
            ini.write_text('[network]\nlisten=127.0.0.1:0\nadcs_listen=127.0.0.1:0\n'
                           '[tls]\ncertificate=certificate.pem\nprivate_key=private.pem\n')
            endpoint, token = enable_api(ini, directory)
            hub = Hub(self,'--config',str(ini),'--log-connections','on')
            watcher=Peer(self,hub.connect()); watcher.login(b'SystemObserver',1)
            for encrypted in (False,True):
                peer=Peer(self,connect_tls(self,hub,cert) if encrypted else hub.connect())
                peer.handshake(fragmented=True)
                # A valid identity is required; nothing system-like arrives just
                # because a TCP/TLS connection or HSUP/SID exchange completed.
                peer.send(peer.identity(b'SystemUser',2))
                self.assertTrue(peer.line().startswith(b'IINF '))
                seen=[]
                while not seen or not seen[-1].startswith(b'BINF '+peer.sid+b' '):
                    seen.append(peer.line()); self.assertTrue(seen[-1].startswith(b'BINF '))
                self.assertEqual(peer.line(),WELCOME)
                self.assertTrue(watcher.line().startswith(b'BINF '+peer.sid+b' '))
                peer.send(b'BMSG '+peer.sid+b' SystemFence')
                for ready in (peer,watcher): self.assertEqual(ready.line(),b'BMSG '+peer.sid+b' SystemFence')
                peer.send(b'IMSG SystemSpoofSecret PM'+peer.sid+b' ME1')
                terminal=drain_until_eof(peer.peer)
                self.assertTrue(terminal.startswith(b'ISTA 240 ')); self.assertNotIn(b'SystemSpoofSecret',terminal)
                self.assertEqual(watcher.line(),b'IQUI '+peer.sid)
                replacement=Peer(self,connect_tls(self,hub,cert) if encrypted else hub.connect())
                frames=replacement.login(b'SystemUser',2)
                self.assertEqual(frames[-1],WELCOME); self.assertEqual(frames.count(WELCOME),1)
                self.assertNotEqual(peer.sid,replacement.sid)
                self.assertTrue(watcher.line().startswith(b'BINF '+replacement.sid+b' '))
                replacement.peer.close(); self.assertEqual(watcher.line(),b'IQUI '+replacement.sid)
            status=json.dumps(fetch_status(endpoint,token))
            self.assertNotIn('SystemSpoofSecret',status); self.assertNotIn('Guest access is enabled',status)
            hub.stop(); self.assertNotIn(b'SystemSpoofSecret',hub.log); self.assertNotIn(b'Guest access is enabled',hub.log)


if __name__ == '__main__':
    unittest.main()
