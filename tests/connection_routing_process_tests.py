"""Concurrent SID-isolated CTM/RCM pairs on mixed ADC/ADCS, using identical tokens."""
import json
import pathlib
import tempfile
import unittest
from adc_login_process_tests import Peer
from transfer_statistics_support import assert_statistics
from api_support import enable_api, fetch_status
from network_process_tests import Hub, drain_until_eof
from tls_support import credentials, connect_tls


class ConnectionRoutingProcessTests(unittest.TestCase):
    def test_mixed_pairs_fixed_audience_and_reconnect(self):
        for ipv6 in (False, True):
            with self.subTest(ipv6=ipv6), tempfile.TemporaryDirectory(prefix='dc24h-routing-') as directory:
                suffix,host=(b'6','[::1]') if ipv6 else (b'4','127.0.0.1')
                cert,_=credentials(directory);ini=pathlib.Path(directory)/'hub.ini'
                ini.write_text(f'[network]\nlisten={host}:0\nadcs_listen={host}:0\n[tls]\ncertificate=certificate.pem\nprivate_key=private.pem\n')
                endpoint,secret=enable_api(ini,directory)
                hub=Hub(self,'--config',str(ini),'--log-connections','on','--log-chat','on');peers=[]

                def login(n,encrypted):
                    peer=Peer(self,connect_tls(self,hub,cert) if encrypted else hub.connect());peer.handshake()
                    identity=peer.identity(b'RoutingUser'+str(n).encode(),n).replace(b'I40.0.0.0',b'I'+suffix+(b'::' if ipv6 else b'0.0.0.0'))
                    peer.send(identity);line=peer.line()
                    while not line.startswith(b'BINF '+peer.sid+b' '):line=peer.line()
                    self.assertTrue(peer.line().startswith(b'IMSG '))
                    for old in peers:self.assertTrue(old.line().startswith(b'BINF '+peer.sid+b' '))
                    peers.append(peer);return peer

                active_a,passive_b,passive_c,active_d,bystander=[login(n,n%2==0) for n in range(1,6)]
                for active in (active_a,active_d):
                    update=b'BINF '+active.sid+b' SUTCP'+suffix;active.send(update)
                    for peer in peers:self.assertEqual(peer.line(),update)
                before=fetch_status(endpoint,secret)
                token=b'SameRoutingPrivateToken'

                def wave(first_source,first,second_source,second,expected):
                    fence_a=b'BSTA '+first_source.sid+b' 000 RoutingFenceA'
                    fence_b=b'BSTA '+second_source.sid+b' 000 RoutingFenceB'
                    # Each stream fences its own request; readers allow cross-stream
                    # interleaving while proving both requests finished processing.
                    first_source.peer.sendall(first+b'\n'+fence_a+b'\n')
                    second_source.peer.sendall(second+b'\n'+fence_b+b'\n')
                    for peer in peers:
                        seen=set();messages=[]
                        while len(seen)<2:
                            line=peer.line()
                            if line in (fence_a,fence_b):seen.add(line)
                            else:messages.append(line)
                        self.assertCountEqual(messages,expected.get(peer,[]),f'Wrong SID audience for {peer.sid!r}')

                offer=b'ECTM '+active_a.sid+b' '+passive_b.sid+b' ADC/1.0 01515 '+token+b' XXPairPrivateA'
                reverse=b'ERCM '+passive_c.sid+b' '+active_d.sid+b' ADC/1.0 '+token+b' XXPairPrivateC'
                wave(active_a,offer,passive_c,reverse,{active_a:[offer],passive_b:[offer],passive_c:[reverse],active_d:[reverse]})
                reply=b'DCTM '+active_d.sid+b' '+passive_c.sid+b' ADC/1.0 01516 '+token
                second=b'DCTM '+active_a.sid+b' '+passive_b.sid+b' ADCS/1.0 01517 '+token
                wave(active_d,reply,active_a,second,{passive_c:[reply],passive_b:[second]})
                # Unknown target never falls back to peers sharing the same token.
                active_a.send(b'ECTM '+active_a.sid+b' ZZZZ ADC/1.0 1515 '+token)
                self.assertEqual(active_a.line(),b'ISTA 140 CTM\\srecipient\\sunavailable FCECTM')
                passive_c.send(b'ERCM '+passive_c.sid+b' ZZZZ ADC/1.0 '+token)
                self.assertEqual(passive_c.line(),b'ISTA 140 RCM\\srecipient\\sunavailable FCERCM')
                old_sid=passive_b.sid;passive_b.peer.close();peers.remove(passive_b)
                for peer in peers:self.assertEqual(peer.line(),b'IQUI '+old_sid)
                replacement=login(6,True);self.assertNotEqual(replacement.sid,old_sid)
                active_a.send(b'DCTM '+active_a.sid+b' '+old_sid+b' ADC/1.0 1515 '+token)
                self.assertEqual(active_a.line(),b'ISTA 140 CTM\\srecipient\\sunavailable FCDCTM')
                fresh=b'DCTM '+active_a.sid+b' '+replacement.sid+b' ADC/1.0 1515 '+token
                reverse=b'DRCM '+passive_c.sid+b' '+active_d.sid+b' future/1.0 '+token
                wave(active_a,fresh,passive_c,reverse,{replacement:[fresh],active_d:[reverse]})
                after=fetch_status(endpoint,secret)
                self.assertEqual(after['chat'],before['chat']);self.assertEqual(after['search'],before['search']);assert_statistics(self, after, ctm_received=6, ctm_routed=4, ctm_rejected=2, ctm_queued=5, rcm_received=3, rcm_routed=2, rcm_rejected=1, rcm_queued=3)
                for private in ('RoutingPrivateToken','PairPrivateA','PairPrivateC',secret):self.assertNotIn(private,json.dumps(after))
                # A source SID from the other pair cannot select its recipients.
                active_a.send(b'ECTM '+active_d.sid+b' '+passive_c.sid+b' ADC/1.0 1515 '+token)
                self.assertTrue(drain_until_eof(active_a.peer).startswith(b'ISTA 240 '));peers.remove(active_a)
                for peer in peers:self.assertEqual(peer.line(),b'IQUI '+active_a.sid)
                wire=b'BMSG '+bystander.sid+b' RoutingRecoveryChat';bystander.send(wire)
                for peer in peers:self.assertEqual(peer.line(),wire)
                hub.stop()
                for private in (token,b'PairPrivateA',b'PairPrivateC',secret.encode()):self.assertNotIn(private,hub.log)


if __name__=='__main__':unittest.main()
