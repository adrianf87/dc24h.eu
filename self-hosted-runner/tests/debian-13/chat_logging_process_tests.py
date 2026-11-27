"""Real ADC/ADCS chat outcomes, private API accounting and payload-free logs."""
import json
import pathlib
import tempfile
import time
import unittest

from adc_login_process_tests import Peer
from api_support import enable_api, fetch_status
from network_process_tests import Hub, drain_until_eof
from tls_support import credentials, connect_tls

FIELDS = ('received', 'public_dispatches', 'private_dispatches', 'rejected',
          'flood_limited', 'errors', 'queued', 'unavailable', 'refused')


class ChatLoggingProcessTests(unittest.TestCase):
    def hub(self, policy='', *options):
        directory = tempfile.TemporaryDirectory(prefix='dc24h-chat-log-')
        self.addCleanup(directory.cleanup)
        cert, _ = credentials(directory.name)
        ini = pathlib.Path(directory.name) / 'hub.ini'
        ini.write_text('[network]\nlisten=127.0.0.1:0\nadcs_listen=127.0.0.1:0\n'
                       '[tls]\ncertificate=certificate.pem\nprivate_key=private.pem\n'
                       '[security]\nmax_frames_per_second=10000\n'
                       '[logging]\nchat=on\n[chat]\n' + policy)
        endpoint, token = enable_api(ini, directory.name)
        return Hub(self, '--config', str(ini), *options), cert, endpoint, token

    def wait(self, endpoint, token, **expected):
        deadline = time.monotonic() + 3
        while True:
            stats = fetch_status(endpoint, token)['chat']
            if all(stats[key] == value for key, value in expected.items()):
                self.assertEqual(set(stats), set(FIELDS))
                self.assertEqual(stats['received'], sum(stats[key] for key in FIELDS[1:6]))
                return stats
            self.assertLess(time.monotonic(), deadline, stats)
            time.sleep(.01)

    def peers(self, hub, cert, encrypted):
        peers = []
        for index in range(3):
            peer = Peer(self, connect_tls(self, hub, cert) if (index == 0) == encrypted else hub.connect())
            peer.login(f'LogSecretNick{index}'.encode(), index + 1)
            for earlier in peers:
                self.assertTrue(earlier.line().startswith(b'BINF ' + peer.sid + b' '))
            peers.append(peer)
        return peers

    def test_exact_counts_private_isolation_rejections_and_privacy(self):
        for encrypted in (False, True):
            hub, cert, endpoint, token = self.hub('max_text_bytes=64\n')
            source, target, observer = self.peers(hub, cert, encrypted)
            self.assertEqual(self.wait(endpoint, token, received=0), dict.fromkeys(FIELDS, 0))
            for kind in (b'BMSG', b'DMSG', b'EMSG'):
                frame = kind + b' ' + source.sid + (b' ' + target.sid if kind != b'BMSG' else b'')
                frame += b' LogSecretText\\nFAKE_LOG\\sPMZZZZ'
                if kind != b'BMSG': frame += b' PM' + source.sid
                frame += b' ME1'
                source.send(frame)
                recipients = (source, target, observer) if kind == b'BMSG' else (target,) if kind == b'DMSG' else (source, target)
                for peer in recipients: self.assertEqual(peer.line(), frame)
            source.send(b'EMSG ' + source.sid + b' ' + source.sid + b' LogSecretSelf PM' + source.sid)
            self.assertTrue(source.line().startswith(b'EMSG '))
            source.send(b'DMSG ' + source.sid + b' ZZZZ LogSecretMissing')
            self.assertTrue(source.line().startswith(b'ISTA 140 '))
            source.send(b'BMSG ' + source.sid + b' ' + b'LogSecret' * 8)
            self.assertTrue(source.line().startswith(b'ISTA 100 '))
            # An INF fence proves private frames did not reach the observer.
            fence = b'BINF ' + source.sid + b' DELogSecretFence'
            source.send(fence)
            for peer in (source, target, observer): self.assertEqual(peer.line(), fence)
            stats = self.wait(endpoint, token, received=6, public_dispatches=1, private_dispatches=3,
                              rejected=2, queued=7, flood_limited=0, errors=0, unavailable=0, refused=0)
            source.send(b'BMSG ' + observer.sid + b' LogSecretForged')
            self.assertTrue(drain_until_eof(source.peer).startswith(b'ISTA 240 '))
            for peer in (target, observer): self.assertEqual(peer.line(), b'IQUI ' + source.sid)
            self.wait(endpoint, token, received=7, rejected=3)
            self.assertNotIn('LogSecret', json.dumps(fetch_status(endpoint, token)))
            hub.stop()
            self.assertEqual(hub.log.count(b'Chat event=public_dispatch'), 1)
            self.assertEqual(hub.log.count(b'Chat event=private_dispatch'), 3)
            self.assertEqual(hub.log.count(b'Chat event=rejected'), 3)
            self.assertNotIn(b'LogSecret', hub.log)
            self.assertNotIn(b'FAKE_LOG', hub.log)
            self.assertNotIn(token.encode(), hub.log)
            for peer in (source, target, observer): self.assertNotIn(peer.sid, hub.log)

    def test_silent_flood_refusals_count_and_logs_are_bounded(self):
        for encrypted in (False, True):
            hub, cert, endpoint, token = self.hub('max_messages_per_second=1\nmessage_burst=1\n')
            source, target, observer = self.peers(hub, cert, encrypted)
            frame = b'DMSG ' + source.sid + b' ' + target.sid + b' LogSecretFlood PM' + source.sid
            source.peer.sendall((frame + b'\n') * 100 + b'HMSG Fence\n')
            # Exactly one admission; the frame burst fits a one-second budget.
            self.assertEqual(target.line(), frame)
            self.assertTrue(source.line().startswith(b'ISTA 100 '))
            self.assertTrue(source.line().startswith(b'ISTA 125 '))
            stats = self.wait(endpoint, token, received=100, private_dispatches=1, flood_limited=99, queued=1)
            self.assertEqual(stats['rejected'], 0)
            hub.stop()
            self.assertEqual(hub.log.count(b'Chat event='), 32)
            self.assertIn(b'Chat log events suppressed=68', hub.log)
            self.assertNotIn(b'LogSecret', hub.log)

    def test_cli_off_overrides_ini_without_disabling_statistics(self):
        hub, cert, endpoint, token = self.hub('', '--log-chat', 'off')
        source, target, observer = self.peers(hub, cert, False)
        frame = b'BMSG ' + source.sid + b' LogSecretOff'
        source.send(frame)
        for peer in (source, target, observer): self.assertEqual(peer.line(), frame)
        self.wait(endpoint, token, received=1, public_dispatches=1, queued=3)
        hub.stop()
        self.assertNotIn(b'Chat event=', hub.log)
        self.assertNotIn(b'LogSecret', hub.log)


if __name__ == '__main__':
    unittest.main()
