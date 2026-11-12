"""Target-only results, owner tokens and reconnect privacy on mixed ADC/ADCS."""
import json
import pathlib
import tempfile
import unittest

from adc_login_process_tests import Peer
from api_support import enable_api, fetch_status
from network_process_tests import Hub, drain_until_eof
from tls_support import credentials, connect_tls


class ResultRoutingProcessTests(unittest.TestCase):
    def test_target_only_owner_tokens_and_reconnect(self):
        for encrypted in (False, True):
            with self.subTest(encrypted=encrypted), tempfile.TemporaryDirectory(prefix='dc24h-results-') as directory:
                cert, _ = credentials(directory)
                ini = pathlib.Path(directory) / 'hub.ini'
                ini.write_text('[network]\nlisten=127.0.0.1:0\nadcs_listen=127.0.0.1:0\n'
                               '[tls]\ncertificate=certificate.pem\nprivate_key=private.pem\n'
                               '[security]\nmax_frames_per_second=10000\n')
                endpoint, secret = enable_api(ini, directory)
                hub = Hub(self, '--config', str(ini), '--log-connections', 'on', '--log-chat', 'on')
                peers = []
                for n in range(3):
                    peer = Peer(self, connect_tls(self, hub, cert) if (n % 2 == 0) == encrypted else hub.connect())
                    peer.login(b'ResultUser' + str(n).encode(), n+1)
                    for existing in peers: self.assertTrue(existing.line().startswith(b'BINF ' + peer.sid + b' '))
                    peers.append(peer)
                seeker, responder, other = peers
                before = fetch_status(endpoint, secret)['chat']
                token = 'ResultPrivateŻółw\\sopaque'.encode()
                root = b'LWPNACQDBZRYXW3VHJVCJ64QBZNGHOHHHZWCLNQ'
                fields = 'XXunknown XXagain FN/ResultPrivate/Żółw\\s😀.txt SI00042 SL-0'.encode() + b' TR' + root

                def fence():
                    marker = b'BINF ' + responder.sid + b' DEResultFence'
                    responder.send(marker)
                    for peer in peers: self.assertEqual(peer.line(), marker)

                def search(owner, value):
                    frame = b'DSCH ' + owner.sid + b' ' + responder.sid + b' ANResultPrivate TO' + value
                    owner.send(frame); self.assertEqual(responder.line(), frame)
                    fence()

                def result(recipient, value=token, expected=None, body=fields):
                    frame = b'DRES ' + responder.sid + b' ' + recipient.sid + b' ' + body
                    if value is not None: frame += b' TO' + value
                    wire = frame + b'\n'
                    responder.peer.sendall(wire[:13]); responder.peer.sendall(wire[13:])
                    self.assertEqual((responder if expected else recipient).line(), expected or frame)
                    fence()  # A bystander/source echo or a duplicate becomes observable.

                search(seeker, token)
                result(seeker)
                result(seeker)  # Multiple responses do not complete the search.
                result(other, expected=b'ISTA 100 Search\\sresult\\stoken\\sis\\snot\\sactive FCDRES')
                search(other, token)  # Same opaque value has an independent owner namespace.
                result(other); result(seeker)
                result(seeker, value=b'WrongToken', expected=b'ISTA 100 Search\\sresult\\stoken\\sis\\snot\\sactive FCDRES')
                result(other, value=None)  # Explicit tokenless compatibility.
                result(seeker, body=b'FN/ResultPrivate/ SI0 SL0')
                result(seeker, body=b'FNfile SI0', expected=b'ISTA 100 Invalid\\ssearch\\sresult FCDRES')
                # Own directed result is queued once, without public fanout.
                result(responder, value=None)
                old_sid = seeker.sid
                seeker.peer.close(); peers.remove(seeker)
                for peer in peers: self.assertEqual(peer.line(), b'IQUI ' + old_sid)
                responder.send(b'DRES ' + responder.sid + b' ' + old_sid + b' ' + fields + b' TO' + token)
                self.assertEqual(responder.line(), b'ISTA 140 Search\\sresult\\srecipient\\sunavailable FCDRES'); fence()
                seeker = Peer(self, connect_tls(self, hub, cert) if encrypted else hub.connect())
                seeker.login(b'ResultUser0', 1)
                for peer in peers: self.assertTrue(peer.line().startswith(b'BINF ' + seeker.sid + b' '))
                peers.append(seeker)
                result(seeker, expected=b'ISTA 100 Search\\sresult\\stoken\\sis\\snot\\sactive FCDRES')
                search(seeker, token); result(seeker)
                self.assertEqual(fetch_status(endpoint, secret)['chat'], before)
                waiting = Peer(self, hub.connect()); waiting.handshake()
                waiting.send(b'DRES ' + waiting.sid + b' ' + seeker.sid + b' FNfile SIbad')
                self.assertTrue(drain_until_eof(waiting.peer).startswith(b'ISTA 244 '))
                responder.send(b'DRES ' + other.sid + b' ' + seeker.sid + b' FNfile SIbad')
                self.assertTrue(drain_until_eof(responder.peer).startswith(b'ISTA 240 '))
                for peer in (seeker, other): self.assertEqual(peer.line(), b'IQUI ' + responder.sid)
                snapshot = json.dumps(fetch_status(endpoint, secret), ensure_ascii=False)
                for value in ('ResultPrivate', root.decode(), secret): self.assertNotIn(value, snapshot)
                hub.stop()
                for value in (b'ResultPrivate', root, secret.encode()): self.assertNotIn(value, hub.log)


if __name__ == '__main__':
    unittest.main()
