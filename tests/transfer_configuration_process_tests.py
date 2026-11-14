"""Effective startup transfer limits/policy over real ADC/ADCS, without peer payload telemetry."""
import json
import pathlib
import subprocess
import tempfile
import unittest

from api_support import fetch_status
from network_process_tests import Hub, BINARY
from transfer_limits_process_tests import TransferLimitsProcessTests


DEFAULTS = dict(ctm_enabled=True, rcm_enabled=True, guest_initiate=True, guest_receive=True,
                request_rate=32, request_burst=64, request_byte_rate=131072, request_byte_burst=262144,
                incoming_rate=128, incoming_burst=256, incoming_byte_rate=524288, incoming_byte_burst=1048576,
                allowed_protocols='any')


class TransferConfigurationProcessTests(unittest.TestCase):
    setup_hub = TransferLimitsProcessTests.setup_hub
    exchange = TransferLimitsProcessTests.exchange

    def test_policy_matrix_effective_limits_and_privacy(self):
        quoted = 'Future"\\Protocol/1.0'
        protocols = ['ADC/1.0', 'ADCS/1.0', 'future/1.0', 'adc/1.0', quoted, 'ADC/0.10']
        for ipv6 in (False, True):
            for policy in ('any', 'none', 'ADC/1.0', 'ADCS/1.0', 'ADC/1.0, ADCS/1.0', quoted, 'adc/1.0'):
                with self.subTest(ipv6=ipv6, policy=policy), tempfile.TemporaryDirectory(prefix='dc24h-transfer-policy-') as directory:
                    custom = dict(request_rate=7, request_burst=40, request_byte_rate=70000, request_byte_burst=131072,
                                  incoming_rate=11, incoming_burst=50, incoming_byte_rate=200000, incoming_byte_burst=262144)
                    settings = 'allowed_protocols=' + policy + '\n' + ''.join(f'{key}={value}\n' for key, value in custom.items())
                    hub, peers, suffix, endpoint, secret = self.setup_hub(directory, ipv6, settings)
                    expected = {**DEFAULTS, **custom, 'allowed_protocols': 'any' if policy == 'any' else [] if policy == 'none' else policy.split(', ')}
                    self.assertEqual(fetch_status(endpoint, secret)['transfers']['configuration'], expected)
                    a, b, c = peers
                    update = b'BINF ' + b.sid + b' SU'
                    self.exchange(peers, b, [update], {p: [update] for p in peers})
                    for reverse in (False, True):
                        origin, target = (b, a) if reverse else (a, b)
                        for scope in (b'D', b'E'):
                            for protocol in protocols:
                                # INI names are literal; the wire escapes backslashes.
                                wire_name = protocol.encode().replace(b'\\', b'\\\\')
                                command = scope + (b'RCM' if reverse else b'CTM')
                                frame = command + b' ' + origin.sid + b' ' + target.sid + b' ' + wire_name + (b'' if reverse else b' 01515') + b' ConfigurationPrivateToken'
                                allowed = policy == 'any' or protocol in expected['allowed_protocols']
                                deliveries = {target: [frame], **({origin: [frame]} if scope == b'E' else {})} if allowed else {origin: [b'ISTA 125 Connection\\sprotocol\\snot\\spermitted FC' + command]}
                                self.exchange(peers, origin, [frame], deliveries)
                    # Peer diagnostics are still routable, even for a disallowed protocol.
                    report = b'DSTA ' + b.sid + b' ' + a.sid + b' 141 PeerReport PRfuture/1.0 TOConfigurationPrivateToken'
                    self.exchange(peers, b, [report], {a: [report]})
                    snapshot = fetch_status(endpoint, secret)
                    stats = snapshot['transfers']['statistics']
                    admitted = sum(policy == 'any' or p in expected['allowed_protocols'] for p in protocols) * 2
                    for kind in ('ctm', 'rcm'):
                        self.assertEqual(stats[kind + '_received'], 12)
                        self.assertEqual(stats[kind + '_routed'], admitted)
                        self.assertEqual(stats[kind + '_rejected'], 12 - admitted)
                        self.assertEqual(stats[kind + '_rate_limited'], 0)
                    self.assertEqual(stats['peer_protocol_unsupported'], 1)
                    for private in ('ConfigurationPrivateToken', 'LimitsUser', secret, directory):
                        self.assertNotIn(private, json.dumps(snapshot))
                    # Editing an INI cannot mutate the running startup policy.
                    ini = pathlib.Path(directory) / 'hub.ini'
                    ini.write_text(ini.read_text().replace('allowed_protocols=' + policy, 'allowed_protocols=none').replace('request_rate=7\n', 'request_rate=13\n'))
                    self.assertEqual(fetch_status(endpoint, secret)['transfers']['configuration'], expected)
                    hub.stop()
                    self.assertNotIn(b'ConfigurationPrivateToken', hub.log)
                    restarted = Hub(self, '--config', str(ini))
                    self.assertEqual(fetch_status(endpoint, secret)['transfers']['configuration'], {**expected, 'allowed_protocols': [], 'request_rate': 13})
                    restarted.stop()

    def test_invalid_policy_startup_and_boundaries(self):
        with tempfile.TemporaryDirectory(prefix='dc24h-transfer-config-input-') as directory:
            ini = pathlib.Path(directory) / 'hub.ini'
            for policy in ('', ' ', 'any,ADC/1.0', 'ADC/1.0,none', 'ADC/1.0,', ',ADC/1.0',
                           'ADC/1.0,,ADCS/1.0', 'ADC/1.0,ADC/1.0', 'ADC /1.0', 'żółw/1.0',
                           'x' * 33, ','.join('P/' + str(n) for n in range(17))):
                ini.write_text('[transfers]\nallowed_protocols=' + policy + '\n')
                result = subprocess.run([BINARY, '--config', str(ini), '--check-config'], capture_output=True, timeout=3)
                self.assertNotEqual(result.returncode, 0, policy)
            ini.write_text('[transfers]\nallowed_protocols=any\nallowed_protocols=none\n')
            self.assertNotEqual(subprocess.run([BINARY, '--config', str(ini), '--check-config'], capture_output=True, timeout=3).returncode, 0)
            for policy in ('any', 'none', 'x' * 32, ','.join('P/' + str(n) for n in range(16))):
                ini.write_text('[transfers]\nallowed_protocols=' + policy + '\n')
                self.assertEqual(subprocess.run([BINARY, '--config', str(ini), '--check-config'], capture_output=True, timeout=3).returncode, 0, policy)


if __name__ == '__main__':
    unittest.main()
