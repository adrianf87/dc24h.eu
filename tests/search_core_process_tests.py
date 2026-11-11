"""Search Core is staged: SCH parsing and the RES boundary stay private."""
import json
import pathlib
import tempfile
import unittest

from adc_login_process_tests import Peer
from api_support import enable_api, fetch_status
from network_process_tests import Hub, drain_until_eof
from tls_support import credentials, connect_tls


class SearchCoreProcessTests(unittest.TestCase):
    def test_transport_boundary_source_checks_and_query_privacy(self):
        for encrypted in (False, True):
            with tempfile.TemporaryDirectory(prefix='dc24h-search-core-') as directory:
                cert, _ = credentials(directory)
                ini = pathlib.Path(directory) / 'hub.ini'
                ini.write_text('[network]\nlisten=127.0.0.1:0\nadcs_listen=127.0.0.1:0\n'
                               '[tls]\ncertificate=certificate.pem\nprivate_key=private.pem\n')
                endpoint, token = enable_api(ini, directory)
                hub = Hub(self, '--config', str(ini), '--log-connections', 'on', '--log-chat', 'on')
                source = Peer(self, connect_tls(self, hub, cert) if encrypted else hub.connect())
                source.login(b'SearchSource', 1)
                observer = Peer(self, hub.connect())
                observer.login(b'SearchObserver', 2)
                self.assertTrue(source.line().startswith(b'BINF ' + observer.sid + b' '))
                for kind in (b'BSCH', b'FSCH', b'DSCH', b'ESCH', b'DRES'):
                    wire = kind + b' ' + source.sid
                    if kind in (b'DSCH', b'ESCH', b'DRES'): wire += b' ' + observer.sid
                    if kind == b'FSCH': wire += b' +TCP4'
                    wire += b' ANSearchCoreSecret TOPrivateSearchToken' if kind != b'DRES' else b' FNSearchCoreSecret SI1 SL1 TOPrivateSearchToken'
                    source.send(wire)
                    self.assertEqual(source.line(), b'ISTA 125 ' +
                                     (b'Command\\snot\\simplemented' if kind == b'DRES' else b'Search\\srouting\\snot\\simplemented') + b' FC' + kind)
                    # Ordered INF fence proves that no query/result was relayed.
                    fence = b'BINF ' + source.sid + b' DESearchFence'
                    source.send(fence)
                    for peer in (source, observer): self.assertEqual(peer.line(), fence)
                waiting = Peer(self, hub.connect()); waiting.handshake()
                waiting.send(b'BSCH ' + waiting.sid + b' ANSearchCoreSecret')
                self.assertTrue(drain_until_eof(waiting.peer).startswith(b'ISTA 244 '))
                frame = b'BMSG ' + source.sid + b' ChatStillWorks'
                source.send(frame)
                for peer in (source, observer): self.assertEqual(peer.line(), frame)
                source.send(b'BSCH ' + observer.sid + b' ANSearchCoreSecretForged')
                self.assertTrue(drain_until_eof(source.peer).startswith(b'ISTA 240 '))
                self.assertEqual(observer.line(), b'IQUI ' + source.sid)
                status = fetch_status(endpoint, token)
                self.assertNotIn('SearchCoreSecret', json.dumps(status))
                self.assertNotIn('PrivateSearchToken', json.dumps(status))
                self.assertNotIn('search', status)
                hub.stop()
                self.assertNotIn(b'SearchCoreSecret', hub.log)
                self.assertNotIn(b'PrivateSearchToken', hub.log)
                self.assertNotIn(token.encode(), hub.log)


if __name__ == '__main__':
    unittest.main()
