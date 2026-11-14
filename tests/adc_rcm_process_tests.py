"""Fragmented IPv4/IPv6 ADC/ADCS reverse requests, CTM reply and privacy."""
import json
import pathlib
import tempfile
import unittest
from adc_login_process_tests import Peer
from transfer_statistics_support import assert_statistics
from api_support import enable_api, fetch_status
from network_process_tests import Hub, drain_until_eof
from tls_support import credentials, connect_tls


class RcmProcessTests(unittest.TestCase):
    def test_reverse_routing_and_ctm_response(self):
        for ipv6 in (False, True):
            for encrypted in (False, True):
                with self.subTest(ipv6=ipv6, encrypted=encrypted), tempfile.TemporaryDirectory(prefix='dc24h-rcm-') as directory:
                    suffix, host = (b'6', '[::1]') if ipv6 else (b'4', '127.0.0.1')
                    cert, _ = credentials(directory)
                    ini = pathlib.Path(directory)/'hub.ini'
                    ini.write_text(f'[network]\nlisten={host}:0\nadcs_listen={host}:0\n[tls]\ncertificate=certificate.pem\nprivate_key=private.pem\n')
                    endpoint, secret = enable_api(ini,directory)
                    hub = Hub(self,'--config',str(ini),'--log-connections','on','--log-chat','on')
                    peers=[]

                    def login(n):
                        peer=Peer(self,connect_tls(self,hub,cert) if encrypted else hub.connect());peer.handshake()
                        peer.send(peer.identity(b'RcmUser'+str(n).encode(),n).replace(b'I40.0.0.0',b'I'+suffix+(b'::' if ipv6 else b'0.0.0.0')))
                        line=peer.line()
                        while not line.startswith(b'BINF '+peer.sid+b' '):line=peer.line()
                        self.assertTrue(peer.line().startswith(b'IMSG '))
                        for old in peers:self.assertTrue(old.line().startswith(b'BINF '+peer.sid+b' '))
                        peers.append(peer);return peer

                    source,target,other=login(1),login(2),login(3)
                    before=fetch_status(endpoint,secret)

                    def update(peer,features):
                        wire=b'BINF '+peer.sid+b' SU'+features;peer.send(wire)
                        for recipient in peers:self.assertEqual(recipient.line(),wire)

                    def fence():
                        wire=b'BINF '+other.sid+b' DERcmFence';other.send(wire)
                        for peer in peers:self.assertEqual(peer.line(),wire)

                    def rcm(kind=b'DRCM',protocol=b'ADC/1.0',token=b'RcmSecret\\stoken',recipient=None):
                        return kind+b' '+source.sid+b' '+(target.sid if recipient is None else recipient)+b' '+protocol+b' '+token

                    denied=b'ISTA 100 RCM\\srequires\\spassive\\ssender\\sand\\sactive\\srecipient FCDRCM'
                    source.send(rcm());self.assertEqual(source.line(),denied);fence()
                    update(target,b'UDP'+suffix);source.send(rcm());self.assertEqual(source.line(),denied);fence()
                    update(target,b'TCP'+suffix)
                    for kind,protocol in ((b'DRCM',b'ADC/1.0'),(b'ERCM',b'ADCS/1.0'),(b'DRCM',b'future/1.0')):
                        wire=rcm(kind,protocol)+b' XXopaque XXrepeat TOextension'
                        for byte in wire+b'\n':source.peer.sendall(bytes([byte]))
                        self.assertEqual(target.line(),wire)
                        if kind==b'ERCM':self.assertEqual(source.line(),wire)
                        # Only the active client supplies CTM; protocol/token are
                        # copied by the client and remain unchanged through hub.
                        response=b'DCTM '+target.sid+b' '+source.sid+b' '+protocol+b' 01515 RcmSecret\\stoken'
                        target.send(response);self.assertEqual(source.line(),response);fence()
                    update(source,b'TCP'+suffix);source.send(rcm());self.assertEqual(source.line(),denied);fence()
                    update(source,b'UDP'+suffix)
                    source.send(rcm());self.assertEqual(target.line(),rcm());fence()
                    for wire in (rcm(protocol=b'bad\\sprotocol'),rcm(token=b'T'*129),rcm()+b' XX'+b'x'*2048,
                                 b'BRCM '+source.sid+b' ADC/1.0 RcmSecret',b'FRCM '+source.sid+b' +TCP4 ADC/1.0 RcmSecret',rcm(recipient=source.sid)):
                        source.send(wire);self.assertTrue(source.line().startswith(b'ISTA 100 Invalid\\sconnection\\srequest FC'));fence()
                    source.send(rcm(b'ERCM',recipient=b'ZZZZ'));self.assertEqual(source.line(),b'ISTA 140 RCM\\srecipient\\sunavailable FCERCM');fence()
                    update(target,b'')
                    source.send(rcm());self.assertEqual(source.line(),denied);fence();update(target,b'TCP'+suffix)
                    after=fetch_status(endpoint,secret)
                    self.assertEqual(after['search'],before['search']);self.assertEqual(after['chat'],before['chat']);assert_statistics(self, after, ctm_received=3, ctm_routed=3, ctm_queued=3, rcm_received=15, rcm_routed=4, rcm_rejected=11, rcm_queued=5)
                    for private in ('RcmSecret','opaque',secret):self.assertNotIn(private,json.dumps(after))
                    old_sid=target.sid;target.peer.close();peers.remove(target)
                    for peer in peers:self.assertEqual(peer.line(),b'IQUI '+old_sid)
                    target=login(4);update(target,b'TCP'+suffix)
                    source.send(rcm(recipient=old_sid));self.assertTrue(source.line().startswith(b'ISTA 140 '));fence()
                    source.send(rcm());self.assertEqual(target.line(),rcm());fence()
                    waiting=Peer(self,connect_tls(self,hub,cert) if encrypted else hub.connect());waiting.handshake()
                    waiting.send(b'DRCM '+waiting.sid+b' '+target.sid+b' ADC/1.0 RcmSecret')
                    self.assertTrue(drain_until_eof(waiting.peer).startswith(b'ISTA 244 '))
                    source.send(b'DRCM '+target.sid+b' '+other.sid+b' ADC/1.0 RcmSecret')
                    self.assertTrue(drain_until_eof(source.peer).startswith(b'ISTA 240 '))
                    for peer in (target,other):self.assertEqual(peer.line(),b'IQUI '+source.sid)
                    target.send(b'BMSG '+target.sid+b' RcmRecoveryChat')
                    for peer in (target,other):self.assertEqual(peer.line(),b'BMSG '+target.sid+b' RcmRecoveryChat')
                    hub.stop();self.assertNotIn(b'RcmSecret',hub.log);self.assertNotIn(b'opaque',hub.log);self.assertNotIn(secret.encode(),hub.log)


if __name__=='__main__':unittest.main()
