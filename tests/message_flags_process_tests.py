"""PM/ME combinations and failures through real TCP/TLS with privacy fences."""
import json
import pathlib
import tempfile
import unittest

from adc_login_process_tests import Peer
from api_support import enable_api, fetch_status
from network_process_tests import Hub, drain_until_eof
from tls_support import credentials, connect_tls


class MessageFlagsProcessTests(unittest.TestCase):
    def test_flags_on_both_transports(self):
        with tempfile.TemporaryDirectory(prefix='dc24h-flags-') as directory:
            cert, _ = credentials(directory)
            ini = pathlib.Path(directory)/'hub.ini'
            ini.write_text('[network]\nlisten=127.0.0.1:0\nadcs_listen=127.0.0.1:0\n'
                           '[tls]\ncertificate=certificate.pem\nprivate_key=private.pem\n'
                           '[security]\nmax_frames_per_second=10000\nmax_accepts_per_minute=256\n')
            endpoint, token = enable_api(ini, directory)
            # High chat budget isolates this suite's transport/ordering checks.
            with ini.open('a') as config:
                config.write('[chat]\nmax_messages_per_second=10000\nmessage_burst=10000\nmax_bytes_per_second=16777216\nbyte_burst=16777216\n')
            hub = Hub(self, '--config', str(ini), '--log-connections', 'on')
            peers = []
            for index in range(3):
                peer = Peer(self, connect_tls(self, hub, cert) if index == 1 else hub.connect())
                peer.login(f'Flags{index}'.encode(), index+1)
                for earlier in peers:
                    self.assertTrue(earlier.line().startswith(b'BINF '+peer.sid+b' '))
                peers.append(peer)
            for source, target in ((peers[0], peers[1]), (peers[1], peers[0])):
                for kind in (b'BMSG', b'DMSG', b'EMSG'):
                    for me, canonical in ((b'', b''), (b' ME0', b' ME0'), (b' ME-0', b' ME0'),
                                          (b' ME01', b' ME1'), (b' ME1', b' ME1')):
                        for pm in ((False,) if kind == b'BMSG' else (False, True)):
                            header = kind+b' '+source.sid
                            if kind != b'BMSG': header += b' '+target.sid
                            text = r'FlagSecretŻółw\sPMBBBB\sME1\nBMSG\sAAAA\sinjected\\path'.encode()
                            private = b' PM'+source.sid if pm else b''
                            expected = header+b' '+text+private+canonical
                            # Known fields may be interleaved and in either order.
                            source.send(header+b' '+text+me+b' XXFlagSecretMeta'+private+b' XXfuture')
                            source.send(b'BMSG '+source.sid+b' FlagFence')
                            recipients = peers if kind == b'BMSG' else [target] if kind == b'DMSG' else [target, source]
                            for peer in peers:
                                if peer in recipients: self.assertEqual(peer.line(), expected)
                                self.assertEqual(peer.line(), b'BMSG '+source.sid+b' FlagFence')
                # A valid PM is conversation metadata, not permission to broadcast
                # or impersonate a group's reply SID. Recovery publishes no text.
                for wire in (b'BMSG '+source.sid+b' FlagSecretHidden ME1 PM'+source.sid,
                             b'EMSG '+source.sid+b' '+target.sid+b' FlagSecretHidden ME0 PM'+peers[2].sid):
                    source.send(wire)
                    self.assertTrue(source.line().startswith(b'ISTA 125 '))
                source.send(b'BMSG '+source.sid+b' RecoveryFence')
                for peer in peers: self.assertEqual(peer.line(), b'BMSG '+source.sid+b' RecoveryFence')
            # Semantic flag errors close only the source; no message/echo leaks.
            for encrypted in (False, True):
                for flags in (b' ME0 ME0', b' ME1 ME0', b' ME', b' ME2', b' ME+1',
                              b' ME9223372036854775808', b' PM', b' PMabc0', b' PMAAAB PMAAAB'):
                    bad = Peer(self, connect_tls(self, hub, cert) if encrypted else hub.connect())
                    bad.login(b'BadFlags', 9)
                    for peer in peers: self.assertTrue(peer.line().startswith(b'BINF '+bad.sid+b' '))
                    bad.send(b'EMSG '+bad.sid+b' '+peers[0].sid+b' FlagSecretInvalid'+flags)
                    terminal = drain_until_eof(bad.peer)
                    self.assertTrue(terminal.startswith(b'ISTA 240 '))
                    self.assertIn(b'IQUI '+bad.sid+b' ', terminal)
                    self.assertNotIn(b'FlagSecret', terminal)
                    for peer in peers: self.assertEqual(peer.line(), b'IQUI '+bad.sid)
            peers[0].send(b'BMSG '+peers[0].sid+b' FinalFence')
            for peer in peers: self.assertEqual(peer.line(), b'BMSG '+peers[0].sid+b' FinalFence')
            self.assertNotIn('FlagSecret', json.dumps(fetch_status(endpoint, token)))
            hub.stop()
            self.assertNotIn(b'FlagSecret', hub.log)
            self.assertNotIn(b'injected', hub.log)


if __name__ == '__main__':
    unittest.main()
