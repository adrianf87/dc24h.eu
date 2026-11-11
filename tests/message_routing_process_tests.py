"""MSG route isolation and per-recipient FIFO through the real mixed ADC/ADCS hub."""
import json
import pathlib
import tempfile
import unittest

from adc_login_process_tests import Peer
from api_support import enable_api, fetch_status
from network_process_tests import Hub, drain_until_eof
from tls_support import credentials, connect_tls


class MessageRoutingProcessTests(unittest.TestCase):
    def test_interleaved_recipient_matrix_and_recovery(self):
        with tempfile.TemporaryDirectory(prefix='dc24h-routing-') as directory:
            cert, _ = credentials(directory)
            ini = pathlib.Path(directory)/'hub.ini'
            ini.write_text('[network]\nlisten=127.0.0.1:0\nadcs_listen=127.0.0.1:0\n'
                           '[tls]\ncertificate=certificate.pem\nprivate_key=private.pem\n'
                           '[security]\nmax_frames_per_second=10000\n')
            endpoint, token = enable_api(ini, directory)
            # High chat budget isolates this suite's transport/ordering checks.
            with ini.open('a') as config:
                config.write('[chat]\nmax_messages_per_second=10000\nmessage_burst=10000\nmax_bytes_per_second=16777216\nbyte_burst=16777216\n')
            hub = Hub(self, '--config', str(ini), '--log-connections', 'on')
            peers = []
            for index in range(4):
                peer = Peer(self, connect_tls(self, hub, cert) if index % 2 else hub.connect())
                peer.login(f'Route{index}'.encode(), index+1)
                for earlier in peers:
                    self.assertTrue(earlier.line().startswith(b'BINF '+peer.sid+b' '))
                peers.append(peer)
            pending = Peer(self, connect_tls(self, hub, cert));pending.handshake()
            for sender_index in (0, 1):
                sender = peers[sender_index]
                expected = [[] for _ in peers]
                wire = []
                for index in range(36):
                    kind = (b'BMSG', b'DMSG', b'EMSG')[index % 3]
                    target_index = (index // 3) % len(peers)
                    target = peers[target_index]
                    header = kind+b' '+sender.sid
                    if kind != b'BMSG':
                        header += b' '+target.sid
                    # Escaped newlines and command-like text stay within one MSG.
                    text = f'RouteSecret{index}Żółw'.encode()+b'\\s\\nBMSG\\sAAAA\\sinjected\\\\path'
                    frame = header+b' '+text
                    if kind != b'BMSG':frame += b' PM'+sender.sid
                    frame += b' ME1'
                    wire.append(frame+b' XXRouteSecretMetadata PDRouteSecretCredential\n')
                    recipients = (range(len(peers)) if kind == b'BMSG' else
                                  [target_index] if kind == b'DMSG' or target_index == sender_index else
                                  [target_index, sender_index])
                    for recipient in recipients:expected[recipient].append(frame)
                fence = b'BMSG '+sender.sid+b' RoutingFence'
                wire.append(fence+b'\n')
                sender.peer.sendall(b''.join(wire))
                for index, peer in enumerate(peers):
                    for frame in expected[index]+[fence]:self.assertEqual(peer.line(), frame)
            source, target, observer, _ = peers
            for frame in (b'BMSG '+source.sid+b' RouteSecretHidden PM'+source.sid,
                          b'FMSG '+source.sid+b' +TCP4 RouteSecretHidden',
                          b'HMSG RouteSecretHidden',
                          b'EMSG '+source.sid+b' '+target.sid+b' RouteSecretHidden PM'+observer.sid):
                source.send(frame)
                self.assertTrue(source.line().startswith(b'ISTA 125 '))
            source.send(b'EMSG '+source.sid+b' '+pending.sid+b' RouteSecretHidden PM'+source.sid)
            self.assertTrue(source.line().startswith(b'ISTA 140 '))
            # Completing a late welcome releases no chat backlog.
            pending.send(pending.identity(b'LateRoute', 5))
            self.assertTrue(pending.line().startswith(b'IINF '))
            while True:
                line = pending.line();self.assertTrue(line.startswith(b'BINF '))
                if line.startswith(b'BINF '+pending.sid+b' '):break
            self.assertTrue(pending.line().startswith(b'IMSG Welcome\\sto\\sdc24h.eu.'))
            for peer in peers:self.assertTrue(peer.line().startswith(b'BINF '+pending.sid+b' '))
            source.send(b'BMSG '+source.sid+b' RecoveryFence')
            for peer in peers+[pending]:self.assertEqual(peer.line(), b'BMSG '+source.sid+b' RecoveryFence')
            old_sid = target.sid;target.peer.close()
            for peer in (source, observer, peers[3], pending):self.assertEqual(peer.line(), b'IQUI '+old_sid)
            replacement = Peer(self, connect_tls(self, hub, cert));replacement.login(b'Route1', 2)
            for peer in (source, observer, peers[3], pending):self.assertTrue(peer.line().startswith(b'BINF '+replacement.sid+b' '))
            source.send(b'EMSG '+source.sid+b' '+old_sid+b' RouteSecretOld PM'+source.sid)
            self.assertTrue(source.line().startswith(b'ISTA 140 '))
            # A forged source cannot obtain even a private echo.
            replacement.send(b'EMSG '+source.sid+b' '+observer.sid+b' RouteSecretForged PM'+source.sid)
            terminal = drain_until_eof(replacement.peer)
            self.assertTrue(terminal.startswith(b'ISTA 240 '));self.assertNotIn(b'RouteSecret', terminal)
            for peer in (source, observer, peers[3], pending):self.assertEqual(peer.line(), b'IQUI '+replacement.sid)
            source.send(b'BMSG '+source.sid+b' FinalFence')
            for peer in (source, observer, peers[3], pending):self.assertEqual(peer.line(), b'BMSG '+source.sid+b' FinalFence')
            self.assertNotIn('RouteSecret', json.dumps(fetch_status(endpoint, token)))
            hub.stop();self.assertNotIn(b'RouteSecret', hub.log);self.assertNotIn(b'injected', hub.log)


if __name__ == '__main__':
    unittest.main()
