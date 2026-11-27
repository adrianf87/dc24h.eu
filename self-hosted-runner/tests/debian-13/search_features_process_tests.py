"""Live FSCH uses public INF SU, independently of negotiated SUP, over ADC/ADCS."""
import json
import pathlib
import tempfile
import unittest

from adc_login_process_tests import Peer
from api_support import enable_api, fetch_status
from network_process_tests import Hub, drain_until_eof
from tls_support import credentials, connect_tls


class FeatureProcessTests(unittest.TestCase):
    def test_dynamic_features_and_private_fanout(self):
        for encrypted in (False, True):
            with self.subTest(encrypted=encrypted), tempfile.TemporaryDirectory(prefix='dc24h-features-') as directory:
                cert, _ = credentials(directory)
                ini = pathlib.Path(directory) / 'hub.ini'
                ini.write_text('[network]\nlisten=127.0.0.1:0\nadcs_listen=127.0.0.1:0\n'
                               '[tls]\ncertificate=certificate.pem\nprivate_key=private.pem\n'
                               '[security]\nmax_frames_per_second=10000\n')
                endpoint, secret = enable_api(ini, directory)
                hub = Hub(self, '--config', str(ini), '--log-connections', 'on', '--log-chat', 'on')
                peers = []
                for n in range(4):
                    peer = Peer(self, connect_tls(self, hub, cert) if (n % 2 == 0) == encrypted else hub.connect())
                    peer.login(b'FeatureUser' + str(n).encode(), n+1)
                    for existing in peers:
                        self.assertTrue(existing.line().startswith(b'BINF ' + peer.sid + b' '))
                    peers.append(peer)
                source, match, excluded, empty = peers
                waiting = Peer(self, hub.connect()); waiting.handshake()
                before = fetch_status(endpoint, secret)['chat']

                def update(peer, features):
                    frame = b'BINF ' + peer.sid + b' SU' + features
                    peer.send(frame)
                    for receiver in peers: self.assertEqual(receiver.line(), frame)

                def fence():
                    frame = b'BSTA ' + source.sid + b' 000 FeatureFence'
                    source.send(frame)
                    for receiver in peers: self.assertEqual(receiver.line(), frame)

                def route(selectors, recipients, token=b''):
                    frame = (b'FSCH ' + source.sid + b' ' + selectors +
                             ' ANFeaturePrivateŻółw EXtxt GE-0 XXunknown'.encode() + token)
                    source.peer.sendall(frame[:8]); source.peer.sendall(frame[8:] + b'\n')
                    for receiver in recipients: self.assertEqual(receiver.line(), frame)
                    fence()

                update(match, b'ADC0,ZZZ9')
                update(excluded, b'ADC0,ZZZ9,TCP4')
                # SUP is hub negotiation. It never grants a public SU feature.
                for peer in (source, empty): peer.send(b'HSUP ADZZZ9 ADADC0')
                fence()
                route(b'+ZZZ9+ADC0-TCP4', [match], b' TOFeaturePrivateToken')
                route(b'-ZZZ9', [source, empty])
                route(b'+ZZZ8', [])
                route(b'-ZZZ8', peers)
                for _ in range(3): route(b'+ZZZ8', [], b' TOFeatureNoRecipients')
                # Removing hub-level support must not remove public SU support.
                match.send(b'HSUP ADZZZ9 RMZZZ9'); fence()
                route(b'+ZZZ9+ADC0-TCP4', [match])
                # Atomic public profile updates immediately alter the next query.
                update(match, b'ADC0,ZZZ9,TCP4')
                route(b'+ZZZ9-TCP4', [])
                update(excluded, b'ADC0,ZZZ9')
                route(b'+ZZZ9-TCP4', [excluded])
                update(excluded, b'')
                route(b'-ZZZ9', [source, excluded, empty])
                update(source, b'ADC0,ZZZ9')
                route(b'+ZZZ9+ADC0-TCP4', [source])
                update(source, b'')
                route(b'+ZZZ9+ADC0-TCP4', [])
                # All 32 selectors match; 33 are refused before queue/token work.
                names = [b'A' + bytes([65+n//26, 65+n%26]) + b'0' for n in range(32)]
                update(match, b','.join(names))
                route(b''.join(b'+' + name for name in names), [match])
                invalid = [b'+ZZZ9+ZZZ9', b'+ZZZ9-ZZZ9', b'-ZZZ9-ZZZ9',
                           b''.join(b'+' + name for name in names) + b'+ZZZ9']
                for selectors in invalid:
                    source.send(b'FSCH ' + source.sid + b' ' + selectors + b' ANFeaturePrivate TOFeatureInvalid')
                    self.assertEqual(source.line(), b'ISTA 100 Invalid\\ssearch\\squery FCFSCH')
                    fence()
                route(b'+AAA0', [match], b' TOFeatureInvalid')
                source.send(b'FSCH ' + source.sid + b' +AAA0 ANFeaturePrivate TOFeatureInvalid')
                self.assertEqual(source.line(), b'ISTA 100 Search\\stoken\\salready\\sactive FCFSCH'); fence()
                waiting.send(b'FSCH ' + waiting.sid + b' +AAA0 ANFeaturePrivate')
                self.assertTrue(drain_until_eof(waiting.peer).startswith(b'ISTA 244 '))
                # Search traffic and feature updates do not become chat events.
                self.assertEqual(fetch_status(endpoint, secret)['chat'], before)
                # Invalid multi-field SU updates close their owner without
                # publishing a partial feature delta to any other client.
                match.send(b'BINF ' + match.sid + b' SUADC0,ADC0')
                self.assertTrue(match.line().startswith(b'ISTA 243 '))
                self.assertTrue(match.line().startswith(b'IQUI ' + match.sid + b' '))
                self.assertEqual(drain_until_eof(match.peer), b'')
                for receiver in (source, excluded, empty): self.assertEqual(receiver.line(), b'IQUI ' + match.sid)
                peers.remove(match)
                source.send(b'FSCH ' + excluded.sid + b' +AAA0 ANFeaturePrivate')
                self.assertTrue(drain_until_eof(source.peer).startswith(b'ISTA 240 '))
                for receiver in peers[1:]: self.assertEqual(receiver.line(), b'IQUI ' + source.sid)
                status = json.dumps(fetch_status(endpoint, secret))
                for value in ('FeaturePrivate', 'FeatureInvalid', secret): self.assertNotIn(value, status)
                hub.stop()
                for value in (b'FeaturePrivate', b'FeatureInvalid', secret.encode()): self.assertNotIn(value, hub.log)


if __name__ == '__main__':
    unittest.main()
