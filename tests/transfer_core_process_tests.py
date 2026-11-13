"""Transfer Core regression: invalid active/passive coordination fails closed."""
import json
import pathlib
import tempfile
import unittest
from adc_login_process_tests import Peer
from api_support import enable_api, fetch_status
from network_process_tests import Hub, drain_until_eof
from tls_support import credentials, connect_tls


class TransferCoreProcessTests(unittest.TestCase):
    def test_live_boundary_privacy_and_recovery(self):
        for encrypted in (False, True):
            with self.subTest(encrypted=encrypted), tempfile.TemporaryDirectory(prefix='dc24h-transfer-core-') as directory:
                cert, _ = credentials(directory)
                ini = pathlib.Path(directory) / 'hub.ini'
                ini.write_text('[network]\nlisten=127.0.0.1:0\nadcs_listen=127.0.0.1:0\n'
                               '[tls]\ncertificate=certificate.pem\nprivate_key=private.pem\n')
                endpoint, token = enable_api(ini, directory)
                hub = Hub(self, '--config', str(ini), '--log-connections', 'on', '--log-chat', 'on')
                source = Peer(self, connect_tls(self, hub, cert) if encrypted else hub.connect()); source.login(b'TransferSource', 1)
                target = Peer(self, hub.connect()); target.login(b'TransferTarget', 2)
                self.assertTrue(source.line().startswith(b'BINF ' + target.sid))
                bystander = Peer(self, hub.connect()); bystander.login(b'TransferBystander', 3)
                for peer in (source, target): self.assertTrue(peer.line().startswith(b'BINF ' + bystander.sid))
                before = fetch_status(endpoint, token)
                for kind in (b'DCTM', b'ECTM', b'DRCM', b'ERCM'):
                    frame = kind + b' ' + source.sid + b' ' + target.sid + b' ADC/1.0 '
                    if kind.endswith(b'CTM'): frame += b'1515 '
                    source.send(frame + b'CoordinationPrivateToken')
                    expected = (b'ISTA 100 CTM\\ssender\\sis\\snot\\sTCP\\sactive FC' if kind.endswith(b'CTM') else b'ISTA 100 RCM\\srequires\\spassive\\ssender\\sand\\sactive\\srecipient FC')
                    self.assertEqual(source.line(), expected + kind)
                    fence = b'BINF ' + source.sid + b' DETransferFence'
                    source.send(fence)
                    for peer in (source, target, bystander): self.assertEqual(peer.line(), fence)
                after = fetch_status(endpoint, token)
                self.assertEqual(after['search'], before['search'])
                self.assertEqual(after['chat'], before['chat'])
                self.assertNotIn('transfers', after)
                self.assertNotIn('CoordinationPrivate', json.dumps(after))
                waiting = Peer(self, hub.connect()); waiting.handshake()
                waiting.send(b'DRCM ' + waiting.sid + b' ' + target.sid + b' ADC/1.0 CoordinationPrivateToken')
                self.assertTrue(drain_until_eof(waiting.peer).startswith(b'ISTA 244 '))
                chat = b'BMSG ' + source.sid + b' ChatAfterTransferBoundary'; source.send(chat)
                for peer in (source, target, bystander): self.assertEqual(peer.line(), chat)
                source.send(b'DCTM ' + target.sid + b' ' + bystander.sid + b' ADC/1.0 1515 CoordinationPrivateToken')
                self.assertTrue(drain_until_eof(source.peer).startswith(b'ISTA 240 '))
                for peer in (target, bystander): self.assertEqual(peer.line(), b'IQUI ' + source.sid)
                hub.stop()
                self.assertNotIn(b'CoordinationPrivate', hub.log)
                self.assertNotIn(token.encode(), hub.log)


if __name__ == '__main__': unittest.main()
