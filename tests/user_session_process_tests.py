"""Live connection lifetimes: pending/ready ownership, reconnect IDs, TLS failure and shutdown."""
import pathlib
import re
import socket
import tempfile
import unittest

from network_process_tests import Hub,drain_until_eof
from adc_login_process_tests import Peer
from tls_support import credentials,connect_tls,tls_endpoint

RECORD=re.compile(rb'Connection id=(\d+) transport=(ADC|ADCS) event=([a-z_]+)')

class UserSessionProcessTests(unittest.TestCase):
    def test_lifetimes_keep_tokens_readiness_and_cleanup_consistent(self):
        with tempfile.TemporaryDirectory(prefix='dc24h-session-') as directory:
            cert,key=credentials(directory);ini=pathlib.Path(directory)/'hub.ini'
            ini.write_text('[network]\nlisten=127.0.0.1:0\nadcs_listen=127.0.0.1:0\n'
                '[tls]\ncertificate=certificate.pem\nprivate_key=private.pem\n'
                '[security]\nmax_pending_logins=1\n')
            hub=Hub(self,'--config',str(ini),'--log-connections','on')
            first=Peer(self,hub.connect());first.handshake()
            # The ADC IDENTIFY session owns the pending slot until its complete
            # welcome; a second raw TLS admission cannot use it early.
            with socket.create_connection(tls_endpoint(hub),timeout=3) as excess:
                self.assertEqual(drain_until_eof(excess),b'')
            first.send(first.identity(b'SameIdentity',1))
            self.assertTrue(first.line().startswith(b'IINF '))
            self.assertTrue(first.line().startswith(b'BINF '+first.sid+b' '))
            self.assertTrue(first.line().startswith(b'IMSG Welcome\\sto\\sdc24h.eu.'))
            second=Peer(self,connect_tls(self,hub,cert));second.login(b'TLSUser',2)
            self.assertTrue(first.line().startswith(b'BINF '+second.sid+b' '))
            old_sid=first.sid;first.peer.close()
            self.assertEqual(second.line(),b'IQUI '+old_sid)
            replacement=Peer(self,hub.connect());replacement.login(b'SameIdentity',1)
            self.assertTrue(second.line().startswith(b'BINF '+replacement.sid+b' '))
            self.assertNotEqual(old_sid,replacement.sid)
            second.send(b'HSTA 200 PrivateReason')
            self.assertEqual(drain_until_eof(second.peer),b'')
            self.assertEqual(replacement.line(),b'IQUI '+second.sid)
            # Accepted -> closed without application readiness must also release
            # its pending reservation and leave established sessions usable.
            with socket.create_connection(tls_endpoint(hub),timeout=3) as bad:
                bad.sendall(b'NOT_TLS\n');drain_until_eof(bad)
            fresh=Peer(self,connect_tls(self,hub,cert));fresh.login(b'TLSUser',2)
            self.assertTrue(replacement.line().startswith(b'BINF '+fresh.sid+b' '))
            # Both a ready client and a TLS-accepted client exist at shutdown.
            waiting=socket.create_connection(tls_endpoint(hub),timeout=3);self.addCleanup(waiting.close)
            hub.wait_for(lambda:hub.log.count(b'event=accepted')==6)
            hub.stop()
            records=RECORD.findall(hub.log)
            accepted=[int(ident) for ident,_,event in records if event==b'accepted']
            self.assertEqual(len(accepted),6)
            self.assertEqual(accepted,sorted(set(accepted)))
            groups={ident:[event for current,_,event in records if int(current)==ident] for ident in accepted}
            ready_ids=[]
            for ident,events in groups.items():
                self.assertEqual(events.count(b'accepted'),1)
                self.assertLessEqual(events.count(b'login_ready'),1)
                if b'login_ready' in events:
                    ready_ids.append(ident)
                    self.assertEqual(len(events),3 if b'tls_ready' not in events else 4)
                else:
                    self.assertEqual(len(events),2)
                self.assertIn(events[-1],(b'peer_closed',b'tls_read_error',b'tls_closed',b'peer_status',b'tls_handshake_failed',b'shutdown'))
            self.assertEqual(len(ready_ids),4)
            self.assertIn(b'event=peer_status',hub.log)
            self.assertIn(b'event=tls_handshake_failed',hub.log)
            self.assertEqual(waiting.recv(1),b'')
            for secret in (b'SameIdentity',b'TLSUser',b'PrivateReason'):
                self.assertNotIn(secret,hub.log)

if __name__=='__main__':unittest.main()
