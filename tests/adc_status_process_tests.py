"""STA/QUI on the real C++ reactor: terminal order, peer reports, relays and timeouts."""
import pathlib
import socket
import tempfile
import time
import unittest

from network_process_tests import Hub, drain_until_eof
from adc_login_process_tests import Peer, pair
from tls_support import credentials, connect_tls


class StatusProcessTests(unittest.TestCase):
    def hub(self, idle_timeout=30, **security):
        directory=tempfile.TemporaryDirectory(prefix='dc24h-status-')
        self.addCleanup(directory.cleanup)
        cert, key=credentials(directory.name)
        ini=pathlib.Path(directory.name)/'hub.ini'
        limits={'max_frames_per_second':100000, 'close_timeout':1, **security}
        ini.write_text('[network]\nlisten=127.0.0.1:0\nadcs_listen=127.0.0.1:0\n'
                       f'idle_timeout={idle_timeout}\n[tls]\ncertificate=certificate.pem\nprivate_key=private.pem\n'
                       '[security]\n'+''.join(f'{k}={v}\n' for k,v in limits.items()))
        return Hub(self,'--config',str(ini),'--log-connections','on'),cert

    def peer(self, hub, cert, encrypted=False):
        return Peer(self,connect_tls(self,hub,cert) if encrypted else hub.connect())

    def terminal(self, peer, code, own=True, command=None):
        status=peer.line()
        self.assertTrue(status.startswith(b'ISTA '+code+b' '),status)
        if command:
            self.assertIn(b' FC'+command,status)
        if own:
            quit=peer.line()
            self.assertTrue(quit.startswith(b'IQUI '+peer.sid+b' MS'),quit)
            self.assertNotIn(b' RD',quit)
            self.assertNotIn(b' TL',quit)  # No ban/reconnect policy is inferred.
        self.assertEqual(peer.pending,b'')
        self.assertEqual(peer.peer.recv(1024),b'', 'Application data followed final QUI')
        return status

    def test_rejections_self_quit_and_foreign_quit_forgery(self):
        hub,cert=self.hub()
        owner=self.peer(hub,cert);owner.login(b'Owner',1)
        for encrypted in (False,True):
            duplicate=self.peer(hub,cert,encrypted);duplicate.handshake()
            duplicate.send(duplicate.identity(b'Owner',2))
            self.assertIn(b'Nickname\\salready\\sin\\suse',self.terminal(duplicate,b'222'))
            spoof=self.peer(hub,cert,encrypted);spoof.handshake()
            spoof.send(b'HQUI '+owner.sid+b' MSUntrustedReason RDadcs://evil.invalid/ TL-1 DI')
            response=self.terminal(spoof,b'240',command=b'HQUI')
            self.assertNotIn(b'UntrustedReason',response)
            bad=self.peer(hub,cert,encrypted);bad.handshake()
            bad.send(b'HSTA 999 UntrustedStatus PDPrivateStatus')
            self.terminal(bad,b'240',command=b'HSTA')
        owner.send(b'BINF '+owner.sid+b' DEStillHere')
        self.assertIn(b'DEStillHere',owner.line())
        early=self.peer(hub,cert);early.send(b'BMSG AAAB Early')
        self.terminal(early,b'244',False,b'BMSG')
        hub.stop()
        for secret in (b'UntrustedReason',b'UntrustedStatus',b'PrivateStatus',b'evil.invalid',pair(1)[0]):
            self.assertNotIn(secret,hub.log)

    def test_peer_success_recoverable_fatal_and_reconnect(self):
        hub,cert=self.hub()
        observer=self.peer(hub,cert);observer.login(b'Observer',1)
        for encrypted in (False,True):
            peer=self.peer(hub,cert,encrypted);peer.login(b'Peer',2)
            self.assertTrue(observer.line().startswith(b'BINF '+peer.sid+b' '))
            peer.send(b'HSTA 000 Confirm FCHSUP')
            peer.send(b'HSTA 199 UnknownFutureError')
            peer.send(b'BINF '+peer.sid+b' DECheckpoint')
            self.assertIn(b'DECheckpoint',peer.line())
            self.assertIn(b'DECheckpoint',observer.line())
            peer.send(b'HSTA 299 PrivateFatalReason')
            self.assertEqual(drain_until_eof(peer.peer),b'')  # No status reply loop.
            self.assertEqual(observer.line(),b'IQUI '+peer.sid)
            replacement=self.peer(hub,cert,encrypted);replacement.login(b'Peer',2)
            self.assertTrue(observer.line().startswith(b'BINF '+replacement.sid+b' '))
            replacement.peer.close()
            self.assertEqual(observer.line(),b'IQUI '+replacement.sid)
        hub.stop()
        self.assertEqual(hub.log.count(b'event=peer_status'),2)
        self.assertNotIn(b'PrivateFatalReason',hub.log)

    def test_direct_echo_broadcast_feature_statuses_keep_hub_sessions(self):
        hub,cert=self.hub()
        first=self.peer(hub,cert);first.login(b'First',1)
        second=self.peer(hub,cert,True);second.login(b'Second',2);first.line()
        third=self.peer(hub,cert);third.login(b'Third',3);first.line();second.line()
        peers=(first,second,third)
        for peer in (first,third):
            peer.send(b'BINF '+peer.sid+b' SUBASE,TCP4')
            for recipient in peers:
                self.assertIn(b'SUBASE,TCP4',recipient.line())
        first.send(b'DSTA '+first.sid+b' '+second.sid+b' 299 PrivateTarget FCDMSG XXdiscard PDdiscard')
        self.assertEqual(second.line(),b'DSTA '+first.sid+b' '+second.sid+b' 299 PrivateTarget FCDMSG')
        first.send(b'ESTA '+first.sid+b' '+second.sid+b' 100 Echo')
        self.assertEqual(first.line(),second.line())
        first.send(b'FSTA '+first.sid+b' -TCP4 100 Feature')
        self.assertTrue(second.line().startswith(b'FSTA '+first.sid))
        first.send(b'BSTA '+first.sid+b' 199 Broadcast')
        for peer in peers:
            self.assertTrue(peer.line().startswith(b'BSTA '+first.sid))
        first.send(b'DSTA '+first.sid+b' AAAA 100 Missing')
        self.assertTrue(first.line().startswith(b'ISTA 140 '))
        # Client source spoofing cannot deliver a fake status or close a victim.
        second.send(b'DSTA '+first.sid+b' '+third.sid+b' 200 Forged')
        self.terminal(second,b'240')
        self.assertEqual(first.line(),b'IQUI '+second.sid)
        self.assertEqual(third.line(),b'IQUI '+second.sid)
        first.send(b'BINF '+first.sid+b' DEAlive')
        self.assertIn(b'DEAlive',first.line());self.assertIn(b'DEAlive',third.line())
        hub.stop()
        self.assertNotIn(b'PrivateTarget',hub.log)
        self.assertNotIn(b'Forged',hub.log)

    def test_deadlines_and_quotas_notify_before_closing(self):
        for encrypted in (False,True):
            with self.subTest(encrypted=encrypted):
                hub,cert=self.hub(login_timeout=1,partial_frame_timeout=1,idle_timeout=5)
                identify=self.peer(hub,cert,encrypted);identify.handshake()
                start=time.monotonic();self.terminal(identify,b'240')
                self.assertLess(time.monotonic()-start,2.5)
                normal=self.peer(hub,cert,encrypted);normal.login(b'Partial',3)
                normal.peer.sendall(b'BINF '+normal.sid+b' DEunfinished')
                self.terminal(normal,b'240')
                hub.stop()
                self.assertIn(b'event=login_timeout',hub.log)
                self.assertIn(b'event=partial_frame_timeout',hub.log)
                self.assertIn(b'Security stopped; rejected=0 closed=2',hub.log)
                idle_hub,cert=self.hub(idle_timeout=1)
                idle=self.peer(idle_hub,cert,encrypted);idle.login(b'Idle',4)
                self.terminal(idle,b'230');idle_hub.stop()
                self.assertIn(b'event=idle_timeout',idle_hub.log)
                quota_hub,cert=self.hub(max_invalid_frames=2)
                quota=self.peer(quota_hub,cert,encrypted);quota.login(b'Quota',5)
                quota.peer.sendall(b'HMSG bad\\q\nHMSG bad\\q\n')
                self.terminal(quota,b'240');quota_hub.stop()
                self.assertIn(b'event=security_quota',quota_hub.log)
                self.assertIn(b'Security stopped; rejected=0 closed=1',quota_hub.log)


if __name__=='__main__':
    unittest.main()
