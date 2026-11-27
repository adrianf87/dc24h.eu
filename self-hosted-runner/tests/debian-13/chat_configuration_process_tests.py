"""Startup chat policy propagation, private isolation and configured welcomes."""
import json
import pathlib
import tempfile
import unittest

from adc_login_process_tests import Peer
from api_support import enable_api, fetch_status
from network_process_tests import Hub, drain_until_eof
from tls_support import credentials, connect_tls


class ChatConfigurationProcessTests(unittest.TestCase):
    def hub(self, policy):
        directory=tempfile.TemporaryDirectory(prefix='dc24h-chat-config-')
        self.addCleanup(directory.cleanup)
        cert,_=credentials(directory.name)
        ini=pathlib.Path(directory.name)/'hub.ini'
        ini.write_text('[network]\nlisten=127.0.0.1:0\nadcs_listen=127.0.0.1:0\n'
                       '[tls]\ncertificate=certificate.pem\nprivate_key=private.pem\n'
                       '[chat]\nmax_messages_per_second=1000\nmessage_burst=1000\n'
                       'max_bytes_per_second=16777216\nbyte_burst=16777216\n'+policy)
        endpoint,token=enable_api(ini,directory.name)
        return Hub(self,'--config',str(ini),'--log-connections','on'),cert,endpoint,token

    def peers(self, hub, cert, encrypted, welcome):
        peers=[]
        for index in range(3):
            peer=Peer(self,connect_tls(self,hub,cert) if (index==0)==encrypted else hub.connect())
            frames=peer.login(f'ConfigUser{index}'.encode(),index+1,welcome=welcome)
            if welcome is None: self.assertTrue(frames[-1].startswith(b'BINF '+peer.sid+b' '))
            for earlier in peers: self.assertTrue(earlier.line().startswith(b'BINF '+peer.sid+b' '))
            peers.append(peer)
        return peers

    def fence(self, peers, source):
        wire=b'BSTA '+source.sid+b' 000 ConfigFence'
        source.send(wire)
        for peer in peers: self.assertEqual(peer.line(),wire)

    def test_public_private_action_switches_and_no_welcome(self):
        for encrypted in (False,True):
            for key in ('public_enabled','private_enabled','actions_enabled'):
                hub,cert,endpoint,token=self.hub(key+'=off\nwelcome_enabled=off\nwelcome_message=\n')
                peers=self.peers(hub,cert,encrypted,None)
                source,target,observer=peers
                for kind in (b'BMSG',b'DMSG',b'EMSG'):
                    for me,canonical in ((b'',b''),(b' ME0',b' ME0'),(b' ME01',b' ME1')):
                        header=kind+b' '+source.sid+(b' '+target.sid if kind!=b'BMSG' else b'')
                        pm=b' PM'+source.sid if kind!=b'BMSG' else b''
                        expected=header+b' CFGSecret'+pm+canonical
                        source.send(header+b' CFGSecret'+me+pm)
                        denied=(key=='public_enabled' and kind==b'BMSG' or
                                key=='private_enabled' and kind!=b'BMSG' or
                                key=='actions_enabled' and canonical==b' ME1')
                        if denied:
                            self.assertEqual(source.line(),b'ISTA 125 Chat\\soperation\\sis\\sdisabled FC'+kind)
                        else:
                            recipients=peers if kind==b'BMSG' else [target] if kind==b'DMSG' else [target,source]
                            for peer in recipients: self.assertEqual(peer.line(),expected)
                        self.fence(peers,source)
                self.assertEqual(fetch_status(endpoint,token)['users']['online'],3)
                source.send(b'BMSG '+observer.sid+b' CFGSecretForged')
                terminal=drain_until_eof(source.peer)
                self.assertTrue(terminal.startswith(b'ISTA 240 ')); self.assertNotIn(b'CFGSecret',terminal)
                for peer in (target,observer): self.assertEqual(peer.line(),b'IQUI '+source.sid)
                self.assertNotIn('CFGSecret',json.dumps(fetch_status(endpoint,token)))
                hub.stop(); self.assertNotIn(b'CFGSecret',hub.log)

    def test_decoded_text_limit_custom_welcome_and_recovery(self):
        welcome_text=r'CFGSecret Żółw\n PMAAAB ME1'
        welcome=b'IMSG '+welcome_text.encode().replace(b'\\',b'\\\\').replace(b' ',b'\\s')
        for encrypted in (False,True):
            hub,cert,endpoint,token=self.hub('max_text_bytes=4\nwelcome_message='+welcome_text+'\n')
            peers=self.peers(hub,cert,encrypted,welcome)
            source,target,_=peers
            for text in ('abcd','😀','    '):
                frame=b'BMSG '+source.sid+b' '+text.encode().replace(b' ',b'\\s')+b' ME0'
                source.send(frame)
                for peer in peers: self.assertEqual(peer.line(),frame)
            for kind in (b'BMSG',b'DMSG',b'EMSG'):
                for text in ('abcde','😀x','Żółw'):
                    frame=kind+b' '+source.sid+(b' '+target.sid if kind!=b'BMSG' else b'')+b' '+text.encode()
                    source.send(frame)
                    self.assertEqual(source.line(),b'ISTA 100 Chat\\stext\\slimit\\sexceeded FC'+kind)
                    self.fence(peers,source)
            recovery=b'BMSG '+source.sid+b' ok'
            source.send(recovery)
            for peer in peers: self.assertEqual(peer.line(),recovery)
            self.assertEqual(fetch_status(endpoint,token)['users']['online'],3)
            self.assertNotIn('CFGSecret',json.dumps(fetch_status(endpoint,token)))
            hub.stop(); self.assertNotIn(b'CFGSecret',hub.log)

    def test_full_size_welcome_is_escaped_and_delivered(self):
        prefix=r'CFGSecret Żółw\n '
        padding=4096-len(prefix.encode())-1
        welcome_text=prefix+'\\ '*(padding//2)+'x'*(padding%2+1)
        self.assertEqual(len(welcome_text.encode()),4096)
        welcome=b'IMSG '+welcome_text.encode().replace(b'\\',b'\\\\').replace(b' ',b'\\s')
        for encrypted in (False,True):
            hub,cert,endpoint,token=self.hub('welcome_message = '+welcome_text+'\n')
            peers=self.peers(hub,cert,encrypted,welcome)
            source=peers[0]
            frame=b'BMSG '+source.sid+b' ready'
            source.send(frame)
            for peer in peers: self.assertEqual(peer.line(),frame)
            self.assertEqual(fetch_status(endpoint,token)['users']['online'],3)
            self.assertNotIn('CFGSecret',json.dumps(fetch_status(endpoint,token)))
            hub.stop(); self.assertNotIn(b'CFGSecret',hub.log)


if __name__ == '__main__':
    unittest.main()
