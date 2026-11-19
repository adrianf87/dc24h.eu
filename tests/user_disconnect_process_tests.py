"""Actual ADC/ADCS disconnect causes, single QUI, slot reuse and shutdown."""
import pathlib
import re
import socket
import struct
import tempfile
import unittest

from adc_login_process_tests import Peer
from network_process_tests import Hub, drain_until_eof
from tls_support import credentials, connect_tls, tls_endpoint, assert_fatal_tls_alert_or_eof


class UserDisconnectProcessTests(unittest.TestCase):
    def test_repeated_close_causes_reuse_quota_and_shutdown(self):
        for encrypted in (False, True):
            with self.subTest(encrypted=encrypted), tempfile.TemporaryDirectory(prefix='dc24h-disconnect-') as directory:
                cert, _ = credentials(directory)
                ini = pathlib.Path(directory) / 'hub.ini'
                ini.write_text('[network]\nlisten=127.0.0.1:0\nadcs_listen=127.0.0.1:0\n'
                               '[tls]\ncertificate=certificate.pem\nprivate_key=private.pem\n'
                               '[security]\nmax_pending_logins=1\nmax_connections_per_ip=3\n')
                hub=Hub(self,'--config',str(ini),'--max-connections','3','--log-connections','on')

                def peer(tls):
                    return Peer(self,connect_tls(self,hub,cert) if tls else hub.connect())

                watcher=peer(not encrypted);watcher.login(b'Watcher',1)
                cycles=8
                for cycle in range(cycles):
                    owner=peer(encrypted);owner.login(b'ReuseIdentity',2)
                    self.assertTrue(watcher.line().startswith(b'BINF '+owner.sid+b' '))
                    if cycle%4==0:
                        owner.peer.close()
                    elif cycle%4==1:
                        owner.peer.setsockopt(socket.SOL_SOCKET,socket.SO_LINGER,struct.pack('ii',1,0));owner.peer.close()
                    elif cycle%4==2:
                        owner.send(b'HSTA 299 PrivateDepartureReason')
                        self.assertEqual(drain_until_eof(owner.peer),b'')
                    else:
                        owner.send(b'BINF '+owner.sid+b' SS-1')
                        self.assertTrue(owner.line().startswith(b'ISTA 243 '))
                        self.assertEqual(owner.line().split(b' ')[:2],[b'IQUI',owner.sid])
                        self.assertEqual(drain_until_eof(owner.peer),b'')
                    self.assertEqual(watcher.line(),b'IQUI '+owner.sid)
                    # A failed anonymous TLS handshake must release its sole
                    # pending slot and emit no user departure before the fence.
                    with socket.create_connection(tls_endpoint(hub),timeout=3) as bad:
                        bad.sendall(b'NOT_TLS\n')
                        assert_fatal_tls_alert_or_eof(self, drain_until_eof(bad))
                    watcher.send(b'BINF '+watcher.sid+b' DEFence')
                    self.assertEqual(watcher.line(),b'BINF '+watcher.sid+b' DEFence')
                final=peer(encrypted);final.login(b'ReuseIdentity',2)
                self.assertTrue(watcher.line().startswith(b'BINF '+final.sid+b' '))
                waiting=socket.create_connection(tls_endpoint(hub),timeout=3);self.addCleanup(waiting.close)
                hub.wait_for(lambda:hub.log.count(b'event=accepted')==2*cycles+3)
                hub.stop()
                self.assertEqual(drain_until_eof(watcher.peer),b'')
                self.assertEqual(drain_until_eof(final.peer),b'')
                self.assertEqual(drain_until_eof(waiting),b'')
                records=re.findall(rb'Connection id=(\d+) transport=(ADC|ADCS) event=([a-z_]+)',hub.log)
                owners=[ident for ident,_,event in records if event==b'accepted']
                self.assertEqual(len(owners),2*cycles+3)
                self.assertEqual(len(set(owners)),len(owners))
                for ident in owners:
                    events=[event for current,_,event in records if current==ident]
                    terminal=[event for event in events if event not in (b'accepted',b'tls_ready',b'login_ready')]
                    self.assertEqual(len(terminal),1,events)
                self.assertEqual(hub.log.count(b'event=login_ready'),cycles+2)
                self.assertNotIn(b'PrivateDepartureReason',hub.log)
                self.assertNotIn(b'ReuseIdentity',hub.log)


if __name__=='__main__':
    unittest.main()
