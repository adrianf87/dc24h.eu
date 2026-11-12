"""Routed SCH registers opaque TO without leaking it to administration."""
import json
import pathlib
import tempfile
import unittest

from adc_login_process_tests import Peer
from api_support import enable_api, fetch_status
from network_process_tests import Hub
from tls_support import credentials, connect_tls


class TokensProcessTests(unittest.TestCase):
    def test_active_tokens_remain_private_across_owners_and_reconnect(self):
        for encrypted in (False, True):
            with tempfile.TemporaryDirectory(prefix='dc24h-tokens-') as directory:
                cert, _ = credentials(directory)
                ini = pathlib.Path(directory) / 'hub.ini'
                ini.write_text('[network]\nlisten=127.0.0.1:0\nadcs_listen=127.0.0.1:0\n'
                               '[tls]\ncertificate=certificate.pem\nprivate_key=private.pem\n'
                               '[security]\nmax_frames_per_second=10000\n')
                endpoint, secret = enable_api(ini, directory)
                hub = Hub(self, '--config', str(ini), '--log-connections', 'on', '--log-chat', 'on')
                source = Peer(self, connect_tls(self, hub, cert) if encrypted else hub.connect())
                source.login(b'TokenSource', 1)
                observer = Peer(self, hub.connect()); observer.login(b'TokenObserver', 2)
                self.assertTrue(source.line().startswith(b'BINF ' + observer.sid + b' '))
                before = fetch_status(endpoint, secret)['chat']
                token = 'PrivateTokenŻółw\\sopaque'.encode()

                def request(peer, value, criteria=b'ANPrivateTokenQuery', duplicate=False):
                    frame = b'BSCH ' + peer.sid + b' ' + criteria + b' TO' + value
                    peer.send(frame)
                    if duplicate:
                        self.assertEqual(peer.line(), b'ISTA 100 Search\\stoken\\salready\\sactive FCBSCH')
                    else:
                        for receiver in (source, observer): self.assertEqual(receiver.line(), frame)

                request(source, token)
                for _ in range(12):
                    request(source, token, duplicate=True)
                request(observer, token)
                source.send(b'BSCH ' + source.sid + b' ANx TOa TOb')
                self.assertEqual(source.line(), b'ISTA 100 Invalid\\ssearch\\squery FCBSCH')
                request(source, token + b'hash', b'TR' + b'A' * 39)
                for fields in (b'', b' TO' + token):
                    source.send(b'BSCH ' + source.sid + fields)
                    fence = b'BINF ' + source.sid + b' DETokenFence'
                    source.send(fence)
                    for peer in (source, observer): self.assertEqual(peer.line(), fence)
                source.peer.close()
                self.assertEqual(observer.line(), b'IQUI ' + source.sid)
                again = Peer(self, connect_tls(self, hub, cert) if encrypted else hub.connect())
                again.login(b'TokenSource', 1)
                self.assertTrue(observer.line().startswith(b'BINF ' + again.sid + b' '))
                source = again
                request(again, token)
                again.send(b'DRES ' + again.sid + b' ' + observer.sid + b' FNPrivateTokenQuery SI0 SL1 TO' + token)
                self.assertEqual(again.line(), b'ISTA 100 Invalid\\ssearch\\sresult FCDRES')
                self.assertEqual(fetch_status(endpoint, secret)['chat'], before)
                frame = b'BMSG ' + again.sid + b' ChatAfterTokenRefusal'
                again.send(frame)
                for peer in (again, observer): self.assertEqual(peer.line(), frame)
                status = json.dumps(fetch_status(endpoint, secret), ensure_ascii=False)
                for value in ('PrivateTokenŻółw', 'PrivateTokenQuery', secret): self.assertNotIn(value, status)
                # Only the final ordinary public chat contributes to chat counters.
                self.assertEqual(fetch_status(endpoint, secret)['chat']['received'], before['received'] + 1)
                hub.stop()
                for value in (b'PrivateToken', secret.encode()): self.assertNotIn(value, hub.log)


if __name__ == '__main__':
    unittest.main()
