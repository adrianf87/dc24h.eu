"""Chat text, SID and state validation on both real source transports."""
import json
import pathlib
import tempfile
import unittest

from adc_login_process_tests import Peer
from api_support import enable_api, fetch_status
from network_process_tests import Hub, drain_until_eof
from tls_support import credentials, connect_tls


class ChatValidationProcessTests(unittest.TestCase):
    def test_text_source_state_and_recovery(self):
        with tempfile.TemporaryDirectory(prefix='dc24h-validation-') as directory:
            cert,_=credentials(directory)
            ini=pathlib.Path(directory)/'hub.ini'
            ini.write_text('[network]\nlisten=127.0.0.1:0\nadcs_listen=127.0.0.1:0\n'
                           '[tls]\ncertificate=certificate.pem\nprivate_key=private.pem\n'
                           '[security]\nmax_accepts_per_minute=256\nmax_frames_per_second=10000\n')
            endpoint,token=enable_api(ini,directory)
            # High chat budget isolates this suite's transport/ordering checks.
            with ini.open('a') as config:
                config.write('[chat]\nmax_messages_per_second=10000\nmessage_burst=10000\nmax_bytes_per_second=16777216\nbyte_burst=16777216\n')
            hub=Hub(self,'--config',str(ini),'--log-connections','on')
            peers=[]
            for index in range(3):
                peer=Peer(self,connect_tls(self,hub,cert) if index==1 else hub.connect())
                peer.login(f'Validation{index}'.encode(),index+1)
                for earlier in peers: self.assertTrue(earlier.line().startswith(b'BINF '+peer.sid+b' '))
                peers.append(peer)
            for source in peers[:2]:
                for malformed in (b'ValidationSecret\t',b'ValidationSecret\r',b'ValidationSecret\x00',
                                  b'ValidationSecret\x7f',b'ValidationSecret\xff',b'ValidationSecrete\xcc\x81',b'ValidationSecret\\q',b''):
                    source.send(b'BMSG '+source.sid+b' '+malformed)
                    source.send(b'BMSG '+source.sid+b' RecoveryFence')
                    for peer in peers: self.assertEqual(peer.line(),b'BMSG '+source.sid+b' RecoveryFence')
                source.send(b'DMSG AAA0 '+peers[2].sid+b' ValidationSecret')
                source.send(b'BMSG '+source.sid+b' SIDRecoveryFence')
                for peer in peers: self.assertEqual(peer.line(),b'BMSG '+source.sid+b' SIDRecoveryFence')
                for text in (r'\s\sValidationSecretŻółw\s😀\s\s',r'ValidationSecret\n\nBMSG\sAAAA\sinjected\\path'):
                    wire=b'BMSG '+source.sid+b' '+text.encode()
                    source.send(wire+b' ME0')
                    for peer in peers: self.assertEqual(peer.line(),wire+b' ME0')
                exact=b'BMSG '+source.sid+b' '+b'x'*(16384-10)
                source.send(exact)
                for peer in peers: self.assertEqual(peer.line(),exact)
                source.send(exact+b'x'); source.send(b'BMSG '+source.sid+b' BoundFence')
                for peer in peers: self.assertEqual(peer.line(),b'BMSG '+source.sid+b' BoundFence')
            for encrypted in (False,True):
                for kind in (b'BMSG',b'DMSG',b'EMSG'):
                    early=Peer(self,connect_tls(self,hub,cert) if encrypted else hub.connect()); early.handshake()
                    target=b' '+peers[0].sid if kind!=b'BMSG' else b''
                    early.send(kind+b' '+early.sid+target+b' ValidationSecretEarly')
                    terminal=drain_until_eof(early.peer)
                    self.assertTrue(terminal.startswith(b'ISTA 244 ')); self.assertNotIn(b'ValidationSecret',terminal)
                    bad=Peer(self,connect_tls(self,hub,cert) if encrypted else hub.connect()); bad.login(b'BadValidation',9)
                    for peer in peers: self.assertTrue(peer.line().startswith(b'BINF '+bad.sid+b' '))
                    bad.send(kind+b' '+peers[1].sid+target+b' ValidationSecretForged')
                    terminal=drain_until_eof(bad.peer)
                    self.assertTrue(terminal.startswith(b'ISTA 240 ')); self.assertNotIn(b'ValidationSecret',terminal)
                    for peer in peers: self.assertEqual(peer.line(),b'IQUI '+bad.sid)
            source=peers[0]
            source.send(b'BMSG '+source.sid+b' FinalFence')
            for peer in peers: self.assertEqual(peer.line(),b'BMSG '+source.sid+b' FinalFence')
            self.assertNotIn('ValidationSecret',json.dumps(fetch_status(endpoint,token)))
            hub.stop(); self.assertNotIn(b'ValidationSecret',hub.log); self.assertNotIn(b'injected',hub.log)


if __name__ == '__main__':
    unittest.main()
