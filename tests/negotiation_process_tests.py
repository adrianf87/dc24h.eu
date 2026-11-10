"""Negotiation limits on real TCP/TLS: pending sessions, cumulative input, pre-hash quotas."""
import pathlib
import socket
import ssl
import tempfile
import time
import unittest

from network_process_tests import Hub,drain_until_eof
from adc_login_process_tests import Peer,pair
from tls_support import credentials,tls_endpoint

class NegotiationProcessTests(unittest.TestCase):
    def hub(self,**settings):
        directory=tempfile.TemporaryDirectory(prefix='dc24h-negotiate-');self.addCleanup(directory.cleanup)
        cert,key=credentials(directory.name);ini=pathlib.Path(directory.name)/'hub.ini'
        values={'max_frames_per_second':100000,**settings}
        ini.write_text('[network]\nlisten=127.0.0.1:0\nadcs_listen=127.0.0.1:0\n[tls]\n'
                       'certificate=certificate.pem\nprivate_key=private.pem\n[security]\n'+
                       ''.join(f'{k}={v}\n' for k,v in values.items()))
        return Hub(self,'--config',str(ini),'--log-connections','on'),cert
    def raw(self,hub,encrypted=False,source='127.0.0.1'):
        peer=socket.create_connection(tls_endpoint(hub) if encrypted else hub.endpoints[0],timeout=3,source_address=(source,0))
        self.addCleanup(peer.close);return peer
    def peer(self,hub,cert,encrypted=False,source='127.0.0.1'):
        raw=self.raw(hub,encrypted,source)
        peer=ssl.create_default_context(cafile=cert).wrap_socket(raw,server_hostname='localhost') if encrypted else raw
        self.addCleanup(peer.close);return Peer(self,peer)
    def terminal(self,peer,identity=False,assigned=False):
        status=peer.line();self.assertTrue(status.startswith(b'ISTA 240 '),status)
        self.assertIn(b'Identity\\sverification' if identity else b'Negotiation\\sresource',status)
        if assigned:self.assertTrue(peer.line().startswith(b'IQUI '+peer.sid+b' '))
        self.assertEqual(peer.pending,b'');self.assertEqual(peer.peer.recv(1),b'')
    def test_pending_slots_shared_by_tls_tcp_and_ready_release(self):
        hub,cert=self.hub(max_pending_logins=2,max_pending_per_ip=1)
        first=self.raw(hub);self.assertEqual(drain_until_eof(self.raw(hub,True)),b'')
        stalled=self.raw(hub,True,'127.0.0.2')
        # Confirm the second raw TLS socket was accepted before testing global cap.
        hub.wait_for(lambda:hub.log.count(b'event=accepted')==2)
        self.assertEqual(drain_until_eof(self.raw(hub,False,'127.0.0.3')),b'')
        first.shutdown(socket.SHUT_WR);self.assertEqual(drain_until_eof(first),b'')
        healthy=self.peer(hub,cert,True);healthy.login(b'Ready',1)
        waiting=self.peer(hub,cert);waiting.handshake()
        self.assertEqual(drain_until_eof(self.raw(hub)),b'')
        waiting.peer.shutdown(socket.SHUT_WR);drain_until_eof(waiting.peer)
        stalled.shutdown(socket.SHUT_WR);drain_until_eof(stalled)
        other=self.peer(hub,cert,False,'127.0.0.2');other.login(b'Other',2)
        self.assertTrue(healthy.line().startswith(b'BINF '+other.sid))
        healthy.send(b'BINF '+healthy.sid+b' DEAlive');self.assertIn(b'DEAlive',healthy.line())
        hub.stop();self.assertIn(b'rejected=3',hub.log)
        self.assertNotIn(b'127.0.0.2',hub.log);self.assertNotIn(b'127.0.0.3',hub.log)
    def test_cumulative_bytes_and_frames_do_not_reset_on_time_or_protocol_phase(self):
        for encrypted in (False,True):
            with self.subTest(encrypted=encrypted):
                hub,cert=self.hub(max_login_bytes=32)
                peer=self.peer(hub,cert,encrypted);peer.peer.sendall(b'x'*32)
                peer.peer.settimeout(0.05)
                with self.assertRaises(socket.timeout):peer.peer.recv(1)
                peer.peer.settimeout(3);peer.peer.sendall(b'x');self.terminal(peer)
                hub.stop();self.assertIn(b'event=negotiation_limit',hub.log)
                hub,cert=self.hub(max_login_frames=4)
                peer=self.peer(hub,cert,encrypted);peer.peer.sendall(b'\n'*4)
                time.sleep(1.05) # Lifetime budget, independent of the frame-rate window.
                peer.peer.sendall(b'HSUP ADBASE ADTIGR\n');self.terminal(peer)
                hub.stop();self.assertIn(b'event=negotiation_limit',hub.log)
    def test_exact_login_budget_ends_only_at_complete_welcome(self):
        dummy=Peer(self,None);dummy.sid=b'AAAB'
        size=len(b'HSUP ADBASE ADTIGR\n')+len(dummy.identity(b'Exact',3))+1
        hub,cert=self.hub(max_login_bytes=size,max_login_frames=2,max_pending_logins=1)
        first=self.peer(hub,cert,True);first.login(b'Exact',3,True)
        first.peer.sendall(b'\n'*100)
        first.send(b'BINF '+first.sid+b' DEAfterReady');self.assertIn(b'DEAfterReady',first.line())
        second=self.peer(hub,cert);second.login(b'Other',4)
        self.assertTrue(first.line().startswith(b'BINF '+second.sid))
        hub.stop();self.assertNotIn(b'event=negotiation_limit',hub.log)
    def test_identity_budget_precedes_tiger_and_survives_reconnect(self):
        hub,cert=self.hub(max_identity_checks_per_minute=1,max_identity_checks_per_second=100000)
        owner=self.peer(hub,cert);owner.login(b'Owner',1)
        for encrypted in (False,True):
            denied=self.peer(hub,cert,encrypted);denied.handshake()
            denied.send(denied.identity(b'Denied',2).replace(pair(2)[1],pair(3)[1]))
            self.terminal(denied,True,True) # Quota 240, before mismatched Tiger would return 227.
        other=self.peer(hub,cert,True,'127.0.0.2');other.login(b'Other',2)
        self.assertTrue(owner.line().startswith(b'BINF '+other.sid))
        owner.peer.close();self.assertEqual(other.line(),b'IQUI '+owner.sid)
        again=self.peer(hub,cert);again.handshake();again.send(again.identity(b'Owner',1))
        self.terminal(again,True,True)
        other.send(b'BINF '+other.sid+b' DEUnaffected');self.assertIn(b'DEUnaffected',other.line())
        hub.stop();self.assertEqual(hub.log.count(b'event=identity_limit'),3)
        for private in (pair(1)[0],pair(2)[0],b'Denied',b'127.0.0.2'):self.assertNotIn(private,hub.log)
    def test_new_ini_limits_reject_invalid_values_without_startup(self):
        from network_process_tests import BINARY
        import subprocess
        for key in ('max_pending_logins','max_pending_per_ip','max_login_bytes','max_login_frames',
                    'max_identity_checks_per_minute','max_identity_checks_per_second'):
            with tempfile.TemporaryDirectory() as directory:
                path=pathlib.Path(directory)/'hub.ini'
                for value in ('0','-1','999999999999999999999'):
                    path.write_text(f'[security]\n{key}={value}\n')
                    result=subprocess.run([BINARY,'--config',str(path),'--check-config'],capture_output=True,timeout=3)
                    self.assertEqual(result.returncode,2,(key,value,result.stderr))
                    self.assertNotIn(b' listening on ',result.stderr)

if __name__=='__main__':unittest.main()
