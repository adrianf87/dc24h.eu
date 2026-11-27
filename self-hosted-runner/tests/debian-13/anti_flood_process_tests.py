"""Real ADC/ADCS anti-flood enforcement, privacy, recovery and INI policy."""
import json
import math
import pathlib
import tempfile
import time
import unittest

from adc_login_process_tests import Peer
from api_support import enable_api, fetch_status
from network_process_tests import Hub, drain_until_eof
from tls_support import credentials, connect_tls


class AntiFloodProcessTests(unittest.TestCase):
    def hub(self, policy):
        directory=tempfile.TemporaryDirectory(prefix='dc24h-flood-')
        self.addCleanup(directory.cleanup)
        cert,_=credentials(directory.name)
        ini=pathlib.Path(directory.name)/'hub.ini'
        ini.write_text('[network]\nlisten=127.0.0.1:0\nadcs_listen=127.0.0.1:0\n'
                       '[tls]\ncertificate=certificate.pem\nprivate_key=private.pem\n'
                       '[security]\nmax_frames_per_second=10000\n'
                       '[chat]\n'+policy)
        endpoint,token=enable_api(ini,directory.name)
        return Hub(self,'--config',str(ini),'--log-connections','on'),cert,endpoint,token

    def peers(self, hub, cert, encrypted):
        peers=[]
        for index in range(3):
            peer=Peer(self,connect_tls(self,hub,cert) if (index==0)==encrypted else hub.connect())
            peer.login(f'FloodUser{index}'.encode(),index+1)
            for earlier in peers: self.assertTrue(earlier.line().startswith(b'BINF '+peer.sid+b' '))
            peers.append(peer)
        return peers

    def through(self, peer, fence):
        lines=[]
        while True:
            line=peer.line()
            if line==fence: return lines
            lines.append(line)
            self.assertLess(len(lines),200)

    def test_combined_modes_count_suppression_recovery_and_spoof(self):
        for encrypted in (False,True):
            hub,cert,endpoint,token=self.hub('max_messages_per_second=1\nmessage_burst=3\n')
            source,target,observer=self.peers(hub,cert,encrypted)
            frames=[]
            for index in range(50):
                kind=(b'BMSG',b'DMSG',b'EMSG')[index%3]
                frame=kind+b' '+source.sid
                if kind!=b'BMSG': frame+=b' '+target.sid
                frame+=b' '+f'FloodSecret{index}'.encode()
                if kind!=b'BMSG': frame+=b' PM'+source.sid
                if kind==b'EMSG': frame+=b' ME1'
                frames.append(frame)
            started=time.monotonic()
            source.peer.sendall(b''.join(frame+b' XXFloodSecretMetadata\n' for frame in frames)+b'HMSG FloodFence\n')
            responses=[]
            while True:
                line=source.line()
                if line.startswith(b'ISTA 125 ') and b' FCHMSG' in line: break
                responses.append(line)
                self.assertLess(len(responses),100)
            duration=time.monotonic()-started
            fence=b'BMSG '+target.sid+b' HealthyFence'
            target.send(fence)
            accepted=self.through(target,fence)
            observed=self.through(observer,fence)
            self.assertEqual(source.line(),fence)
            self.assertEqual(accepted[:3],frames[:3])
            self.assertLess(len(accepted),len(frames))
            self.assertLessEqual(len(accepted),3+math.ceil(duration))
            self.assertTrue(all(frame in frames for frame in accepted))
            self.assertEqual(observed,[frame for frame in accepted if frame.startswith(b'BMSG ')])
            self.assertEqual([line for line in responses if not line.startswith(b'ISTA ')],
                             [frame for frame in accepted if not frame.startswith(b'DMSG ')])
            notices=[line for line in responses if line.startswith(b'ISTA ')]
            self.assertGreaterEqual(len(notices),1)
            self.assertLessEqual(len(notices),1+math.ceil(duration))
            for notice in notices:
                self.assertTrue(notice.startswith(b'ISTA 100 Chat\\smessage\\srate\\slimit\\sexceeded FC'))
                self.assertNotIn(b'FloodSecret',notice)
            self.assertEqual(fetch_status(endpoint,token)['users']['online'],3)
            # No disconnect or permanent mute: one second earns one new unit.
            time.sleep(1.05)
            recovery=b'EMSG '+source.sid+b' '+target.sid+b' Recovered PM'+source.sid
            source.send(recovery)
            self.assertEqual(source.line(),recovery); self.assertEqual(target.line(),recovery)
            target.send(b'BMSG '+target.sid+b' PrivateFence')
            for peer in (source,target,observer): self.assertEqual(peer.line(),b'BMSG '+target.sid+b' PrivateFence')
            # Exhausted quota cannot mask a fatal source forgery.
            source.send(b'BMSG '+observer.sid+b' FloodSecretForged')
            terminal=drain_until_eof(source.peer)
            self.assertTrue(terminal.startswith(b'ISTA 240 ')); self.assertNotIn(b'FloodSecret',terminal)
            for peer in (target,observer): self.assertEqual(peer.line(),b'IQUI '+source.sid)
            replacement=Peer(self,connect_tls(self,hub,cert) if encrypted else hub.connect())
            replacement.login(b'FloodUser0',1)
            self.assertNotEqual(replacement.sid,source.sid)
            for peer in (target,observer): self.assertTrue(peer.line().startswith(b'BINF '+replacement.sid+b' '))
            replacement.send(b'BMSG '+replacement.sid+b' FreshLifetime')
            for peer in (replacement,target,observer): self.assertEqual(peer.line(),b'BMSG '+replacement.sid+b' FreshLifetime')
            self.assertNotIn('FloodSecret',json.dumps(fetch_status(endpoint,token)))
            hub.stop(); self.assertNotIn(b'FloodSecret',hub.log)

    def test_byte_quota_is_independent_and_counts_decoded_text(self):
        for encrypted in (False,True):
            hub,cert,endpoint,token=self.hub('max_messages_per_second=100\nmessage_burst=100\nmax_bytes_per_second=1024\nbyte_burst=16384\n')
            source,target,observer=self.peers(hub,cert,encrypted)
            first=b'DMSG '+source.sid+b' '+target.sid+b' '+b'x'*16000
            source.send(first); self.assertEqual(target.line(),first)
            rejected=b'EMSG '+source.sid+b' '+target.sid+b' '+b'y'*5120+b' PM'+source.sid
            source.peer.sendall((rejected+b'\n')*10+b'HMSG ByteFence\n')
            notices=[]
            while True:
                line=source.line()
                if line.startswith(b'ISTA 125 '): break
                notices.append(line)
            self.assertGreaterEqual(len(notices),1)
            self.assertTrue(all(line.startswith(b'ISTA 100 ') for line in notices))
            # 384 decoded spaces take 768 wire bytes and still fit the credit
            # remaining from the first message. Header/flag bytes are excluded.
            spaces=b'BMSG '+source.sid+b' '+b'\\s'*384
            source.send(spaces)
            for peer in (source,target,observer): self.assertEqual(peer.line(),spaces)
            time.sleep(1.05)
            recovered=b'BMSG '+source.sid+b' '+b'z'*1024
            source.send(recovered)
            for peer in (source,target,observer): self.assertEqual(peer.line(),recovered)
            self.assertEqual(fetch_status(endpoint,token)['users']['online'],3)
            hub.stop()


if __name__ == '__main__':
    unittest.main()
