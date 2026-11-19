"""Real mixed ADC/ADCS SCH fanout, scopes, TO admission and private diagnostics."""
import json
import pathlib
import tempfile
import unittest

from adc_login_process_tests import Peer
from api_support import enable_api, fetch_status
from network_process_tests import Hub, drain_until_eof
from tls_support import credentials, connect_tls


class RoutingProcessTests(unittest.TestCase):
    def test_scopes_features_tokens_reconnect_and_privacy(self):
        for encrypted in (False, True):
            with tempfile.TemporaryDirectory(prefix='dc24h-routing-') as directory:
                cert, _ = credentials(directory)
                ini = pathlib.Path(directory) / 'hub.ini'
                ini.write_text('[network]\nlisten=127.0.0.1:0\nadcs_listen=127.0.0.1:0\n'
                               '[tls]\ncertificate=certificate.pem\nprivate_key=private.pem\n'
                               '[security]\nmax_frames_per_second=10000\n')
                endpoint, secret = enable_api(ini, directory)
                hub = Hub(self, '--config', str(ini), '--log-connections', 'on', '--log-chat', 'on')
                source = Peer(self, connect_tls(self, hub, cert) if encrypted else hub.connect()); source.login(b'RouteSource', 1)
                second = Peer(self, hub.connect() if encrypted else connect_tls(self, hub, cert)); second.login(b'RouteSecond', 2)
                self.assertTrue(source.line().startswith(b'BINF ' + second.sid + b' '))
                third = Peer(self, hub.connect()); third.login(b'RouteThird', 3)
                for peer in (source, second): self.assertTrue(peer.line().startswith(b'BINF ' + third.sid + b' '))
                peers = [source, second, third]
                for peer, features in ((source,b'TCP4'),(second,b'TCP4'),(third,b'UDP4')):
                    frame = b'BINF ' + peer.sid + b' SU' + features
                    peer.send(frame)
                    for receiver in peers: self.assertEqual(receiver.line(), frame)
                waiting = Peer(self, hub.connect()); waiting.handshake()
                before = fetch_status(endpoint, secret)['chat']

                def fence():
                    frame = b'BSTA ' + source.sid + b' 000 RouteFence'
                    source.send(frame)
                    for peer in peers: self.assertEqual(peer.line(), frame)

                def route(frame, recipients, origin=None):
                    origin = source if origin is None else origin
                    wire = frame + b'\n'
                    origin.peer.sendall(wire[:9]); origin.peer.sendall(wire[9:])
                    for peer in recipients: self.assertEqual(peer.line(), frame)
                    fence()

                common = ' ANSearchRoutingPrivate\\sŻółw EXISO GE-0 TY01 XXopaque'.encode()
                route(b'BSCH ' + source.sid + common, peers)
                feature = b'FSCH ' + source.sid + b' +TCP4-UDP4' + common + b' TOPrivateFeat'
                route(feature, [source, second])
                route(b'FSCH ' + source.sid + b' -TCP4' + common + b' TOPrivateNegative', [third])
                route(b'DSCH ' + source.sid + b' ' + second.sid + common + b' TOPrivateDirect', [second])
                route(b'ESCH ' + source.sid + b' ' + third.sid + common + b' TOPrivateEcho', [source, third])
                route(b'ESCH ' + source.sid + b' ' + source.sid + common + b' TOPrivateSelf', [source])
                for _ in range(3): route(b'FSCH ' + source.sid + b' +ZZZ0' + common + b' TOPrivateEmpty', [])
                route(b'BSCH ' + source.sid + b' TR' + b'A'*39, peers)
                source.send(feature)
                self.assertEqual(source.line(), b'ISTA 100 Search\\stoken\\salready\\sactive FCFSCH'); fence()
                route(b'BSCH ' + second.sid + common + b' TOPrivateFeat', peers, second)
                for n in range(3): route(b'BSCH ' + source.sid + common + b' TOPrivateCap' + str(n).encode(), peers)
                source.send(b'BSCH ' + source.sid + common + b' TOPrivateOverflow')
                self.assertEqual(source.line(), b'ISTA 100 Active\\ssearch\\slimit\\sreached FCBSCH'); fence()
                source.send(b'DSCH ' + source.sid + b' AAAZ' + common + b' TOPrivateMissing')
                self.assertEqual(source.line(), b'ISTA 140 Search\\srecipient\\sunavailable FCDSCH'); fence()
                source.send(b'BSCH ' + source.sid + b' TOPrivateOnly'); fence()
                waiting.send(b'BSCH ' + waiting.sid + b' ANx')
                self.assertTrue(drain_until_eof(waiting.peer).startswith(b'ISTA 244 '))
                old_sid = source.sid; source.peer.close()
                for peer in (second, third): self.assertEqual(peer.line(), b'IQUI ' + old_sid)
                source = Peer(self, connect_tls(self, hub, cert) if encrypted else hub.connect()); source.login(b'RouteSource', 1)
                for peer in (second, third): self.assertTrue(peer.line().startswith(b'BINF ' + source.sid + b' '))
                peers = [source, second, third]
                route(b'BSCH ' + source.sid + common + b' TOPrivateFeat', peers)
                source.send(b'DRES ' + source.sid + b' ' + second.sid + b' FNSearchRoutingPrivate SI0 SL1 TOPrivateFeat')
                self.assertEqual(source.line(), b'ISTA 100 Invalid\\ssearch\\sresult FCDRES'); fence()
                self.assertEqual(fetch_status(endpoint, secret)['chat'], before)
                source.send(b'BSCH ' + second.sid + b' GEbad')
                self.assertTrue(drain_until_eof(source.peer).startswith(b'ISTA 240 '))
                for peer in (second, third): self.assertEqual(peer.line(), b'IQUI ' + source.sid)
                status = json.dumps(fetch_status(endpoint, secret))
                for value in ('SearchRoutingPrivate', 'PrivateFeat', secret): self.assertNotIn(value, status)
                hub.stop()
                for value in (b'SearchRoutingPrivate', b'PrivateFeat', secret.encode()): self.assertNotIn(value, hub.log)


if __name__ == '__main__':
    unittest.main()
