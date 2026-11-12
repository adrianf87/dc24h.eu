"""Live bounded RES reception on ADC/ADCS, source/state priority and privacy."""
import json
import pathlib
import tempfile
import unittest

from adc_login_process_tests import Peer
from api_support import enable_api, fetch_status
from network_process_tests import Hub, drain_until_eof
from tls_support import credentials, connect_tls


class ResProcessTests(unittest.TestCase):
    def test_validation_recovery_and_privacy(self):
        for encrypted in (False, True):
            with self.subTest(encrypted=encrypted), tempfile.TemporaryDirectory(prefix='dc24h-res-') as directory:
                cert, _ = credentials(directory)
                ini = pathlib.Path(directory) / 'hub.ini'
                ini.write_text('[network]\nlisten=127.0.0.1:0\nadcs_listen=127.0.0.1:0\n'
                               '[tls]\ncertificate=certificate.pem\nprivate_key=private.pem\n'
                               '[security]\nmax_frames_per_second=10000\n')
                endpoint, secret = enable_api(ini, directory)
                hub = Hub(self, '--config', str(ini), '--log-connections', 'on', '--log-chat', 'on')
                source = Peer(self, connect_tls(self, hub, cert) if encrypted else hub.connect())
                target = Peer(self, hub.connect())
                source.login(b'ResSource', 1); target.login(b'ResTarget', 2)
                self.assertTrue(source.line().startswith(b'BINF ' + target.sid + b' '))
                before = fetch_status(endpoint, secret)['chat']
                root = b'LWPNACQDBZRYXW3VHJVCJ64QBZNGHOHHHZWCLNQ'
                prefix = b'DRES ' + source.sid + b' ' + target.sid + b' '

                def fence():
                    marker = b'BINF ' + source.sid + b' DEResFence'
                    source.send(marker)
                    for peer in (source, target): self.assertEqual(peer.line(), marker)

                def result(fields, valid):
                    wire = (prefix + fields).rstrip(b' ') + b'\n'
                    source.peer.sendall(wire[:11]); source.peer.sendall(wire[11:])
                    expected = (b'ISTA 125 Search\\sresult\\srouting\\snot\\simplemented FCDRES' if valid
                                else b'ISTA 100 Invalid\\ssearch\\sresult FCDRES')
                    self.assertEqual(source.line(), expected)
                    fence()  # The target received no result; the source remains usable.

                valid = 'FN/ResPrivate/Żółw\\s😀.txt SI00042 SL-0 TOResPrivateToken'.encode() + b' TR' + root
                result(valid + b' XXunknown XXagain', True)
                result(b'FN/ResPrivate/file SI0 TR' + root, True)  # tokenless SCH compatibility
                result(b'FN/ResPrivate/ SI0 SL0 TOResPrivateToken', True)
                for fields in (b'', b'FNfile SI0', b'FNfile TR' + root, b'SI0 TR' + root,
                               valid + b' SI42', valid + b' TOduplicate', valid + b' SL1', valid + b' FNother',
                               valid + b' TR' + root, b'FN/../file SI0 TR' + root,
                               b'FNfile SI-1 TR' + root, b'FNfile SI9223372036854775808 TR' + root,
                               b'FNfile SI0 SL-1 TR' + root, b'FNfile SI0 TR' + b'A'*38 + b'B',
                               b'FNfile SI0 TR' + root + b' TO', b'FNfile SI0 TR' + root + b' TO' + b'x'*129,
                               b'FNfile\\n SI0 TR' + root, valid + b' XX' + b'x'*8192):
                    result(fields, False)
                source.send(b'ERES ' + source.sid + b' ' + target.sid + b' ' + valid)
                self.assertEqual(source.line(), b'ISTA 100 Invalid\\ssearch\\sresult FCERES'); fence()
                # State and source authorization precede semantic payload checks.
                waiting = Peer(self, hub.connect()); waiting.handshake()
                waiting.send(b'DRES ' + waiting.sid + b' ' + target.sid + b' FNfile SIbad')
                self.assertTrue(drain_until_eof(waiting.peer).startswith(b'ISTA 244 '))
                self.assertEqual(fetch_status(endpoint, secret)['chat'], before)
                source.send(b'DRES ' + target.sid + b' ' + source.sid + b' FNfile SIbad')
                self.assertTrue(drain_until_eof(source.peer).startswith(b'ISTA 240 '))
                self.assertEqual(target.line(), b'IQUI ' + source.sid)
                status = json.dumps(fetch_status(endpoint, secret), ensure_ascii=False)
                for value in ('ResPrivate', root.decode(), secret): self.assertNotIn(value, status)
                hub.stop()
                for value in (b'ResPrivate', root, secret.encode()): self.assertNotIn(value, hub.log)


if __name__ == '__main__':
    unittest.main()
