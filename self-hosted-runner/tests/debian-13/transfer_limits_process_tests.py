"""Transfer budgets on fragmented mixed ADC/ADCS IPv4/IPv6 hub connections."""
import json
import pathlib
import subprocess
import tempfile
import time
import unittest

from adc_login_process_tests import Peer
from api_support import enable_api, fetch_status
from network_process_tests import Hub, BINARY
from tls_support import credentials, connect_tls


class TransferLimitsProcessTests(unittest.TestCase):
    def setup_hub(self, directory, ipv6, settings):
        suffix, host = (b'6', '[::1]') if ipv6 else (b'4', '127.0.0.1')
        cert, _ = credentials(directory)
        ini = pathlib.Path(directory) / 'hub.ini'
        ini.write_text(f'[network]\nlisten={host}:0\nadcs_listen={host}:0\n[tls]\ncertificate=certificate.pem\nprivate_key=private.pem\n'
                       '[security]\nmax_frames_per_second=100000\n[transfers]\n' + settings)
        endpoint, secret = enable_api(ini, directory)
        hub = Hub(self, '--config', str(ini), '--log-connections', 'on', '--log-chat', 'on')
        peers = []
        for n in range(1, 4):
            peer = Peer(self, connect_tls(self, hub, cert) if n % 2 == 0 else hub.connect())
            peer.handshake(fragmented=True)
            info = peer.identity(b'LimitsUser' + str(n).encode(), n)
            peer.send(info.replace(b'I40.0.0.0', b'I' + suffix + (b'::' if ipv6 else b'0.0.0.0')) + b' SUTCP' + suffix)
            while not peer.line().startswith(b'BINF ' + peer.sid + b' '):
                pass
            self.assertTrue(peer.line().startswith(b'IMSG '))
            for old in peers:
                self.assertTrue(old.line().startswith(b'BINF ' + peer.sid + b' '))
            peers.append(peer)
        return hub, peers, suffix, endpoint, secret

    def exchange(self, peers, origin, frames, expected):
        fence = b'BSTA ' + origin.sid + b' 000 LimitsFence' + str(time.monotonic_ns()).encode()
        wire = b'\n'.join(frames + [fence]) + b'\n'
        origin.peer.sendall(wire[:3])
        origin.peer.sendall(wire[3:])
        for peer in peers:
            received = []
            while True:
                line = peer.line()
                if line == fence:
                    break
                received.append(line)
            self.assertEqual(received, expected.get(peer, []))

    def test_source_budget_refill_command_scope_and_privacy(self):
        for ipv6 in (False, True):
            with self.subTest(ipv6=ipv6), tempfile.TemporaryDirectory(prefix='dc24h-transfer-limits-') as directory:
                hub, peers, suffix, endpoint, secret = self.setup_hub(directory, ipv6, 'request_rate=1\nrequest_burst=1\n')
                a, b, c = peers
                before = fetch_status(endpoint, secret)
                offer = b'ECTM ' + a.sid + b' ' + b.sid + b' ADCS/1.0 01515 LimitPrivate\\sToken'
                self.exchange(peers, a, [offer] * 64, {b: [offer], a: [offer, b'ISTA 100 Connection\\srequest\\srate\\slimit\\sexceeded FCECTM']})
                # Live mode and protocol changes, D/E and another recipient cannot reset credit.
                update = b'BINF ' + a.sid + b' SU'
                self.exchange(peers, a, [update], {peer: [update] for peer in peers})
                reverse = b'DRCM ' + a.sid + b' ' + c.sid + b' ADC/1.0 LimitPrivate\\sToken'
                self.exchange(peers, a, [reverse] * 16, {})
                time.sleep(1.1)
                self.exchange(peers, a, [reverse], {c: [reverse]})
                # Another lifetime retains an independent source budget.
                own = b'DCTM ' + b.sid + b' ' + c.sid + b' ADC/1.0 1515 LimitPrivate\\sToken'
                self.exchange(peers, b, [own], {c: [own]})
                after = fetch_status(endpoint, secret)
                self.assertEqual(before['chat'], after['chat'])
                self.assertEqual(before['search'], after['search'])
                self.assertNotIn('LimitPrivate', json.dumps(after))
                # Fatal validation must still run on an exhausted source.
                forged = b'DRCM ' + b.sid + b' ' + c.sid + b' ADC/1.0 ForgedPrivate'
                a.send(forged)
                self.assertIn(b'ISTA 240 ', a.line())
                hub.stop()
                for value in (b'LimitPrivate', b'ForgedPrivate', secret.encode()):
                    self.assertNotIn(value, hub.log)

    def test_target_aggregation_and_byte_budget(self):
        for ipv6 in (False, True):
            with self.subTest(ipv6=ipv6), tempfile.TemporaryDirectory(prefix='dc24h-incoming-limits-') as directory:
                hub, peers, suffix, endpoint, secret = self.setup_hub(directory, ipv6, 'incoming_rate=1\nincoming_burst=1\n')
                a, b, c = peers
                offer = b'DCTM ' + a.sid + b' ' + b.sid + b' ADC/1.0 1515 TargetPrivate'
                self.exchange(peers, a, [offer], {b: [offer]})
                other = b'ECTM ' + c.sid + b' ' + b.sid + b' ADCS/1.0 1515 TargetPrivate'
                self.exchange(peers, c, [other] * 32, {c: [b'ISTA 100 Connection\\srequest\\srate\\slimit\\sexceeded FCECTM']})
                own = b'DCTM ' + b.sid + b' ' + a.sid + b' ADC/1.0 1515 TargetPrivate'
                self.exchange(peers, b, [own], {a: [own]})
                time.sleep(1.1)
                self.exchange(peers, c, [other], {b: [other], c: [other]})
                hub.stop()
                self.assertNotIn(b'TargetPrivate', hub.log)
        # Large valid extension bytes count too, even though tokens are small.
        with tempfile.TemporaryDirectory(prefix='dc24h-transfer-bytes-') as directory:
            hub, peers, suffix, endpoint, secret = self.setup_hub(directory, False, 'request_rate=1000\nrequest_burst=1000\nrequest_byte_rate=1\nrequest_byte_burst=16384\n')
            a, b, c = peers
            offer = b'DCTM ' + a.sid + b' ' + b.sid + b' ADC/1.0 1515 BytePrivate XX' + b'x' * 1800
            self.exchange(peers, a, [offer] * 9, {b: [offer] * 8, a: [b'ISTA 100 Connection\\srequest\\srate\\slimit\\sexceeded FCDCTM']})
            # A rejected large request must leave enough credit for a small one.
            small = b'DCTM ' + a.sid + b' ' + c.sid + b' ADC/1.0 1515 BytePrivate'
            self.exchange(peers, a, [small], {c: [small]})
            hub.stop()

    def test_configuration_rejects_invalid_or_duplicate_limits(self):
        with tempfile.TemporaryDirectory(prefix='dc24h-transfer-config-') as directory:
            ini = pathlib.Path(directory) / 'hub.ini'
            for prefix in ('request', 'incoming'):
                for suffix, maximum in (('rate', 1000000), ('burst', 1000000), ('byte_rate', 16777216), ('byte_burst', 16777216)):
                    key = prefix + '_' + suffix
                    for value in ('0', '-1', '1.5', 'on', str(maximum + 1), '999999999999999999999999'):
                        ini.write_text('[transfers]\n' + key + '=' + value + '\n')
                        result = subprocess.run([BINARY, '--config', str(ini)], capture_output=True, timeout=3)
                        self.assertNotEqual(result.returncode, 0, (key, value))
                    ini.write_text(f'[transfers]\n{key}=32768\n{key}=32768\n')
                    self.assertNotEqual(subprocess.run([BINARY, '--config', str(ini)], capture_output=True, timeout=3).returncode, 0)
            ini.write_text('[transfers]\nrequest_byte_burst=16383\n')
            self.assertNotEqual(subprocess.run([BINARY, '--config', str(ini)], capture_output=True, timeout=3).returncode, 0)


if __name__ == '__main__':
    unittest.main()
