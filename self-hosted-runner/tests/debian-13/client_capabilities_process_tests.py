"""Live INF mode changes govern CTM/RCM on both families and hub transports."""
import json
import pathlib
import tempfile
import unittest

from adc_login_process_tests import Peer
from transfer_statistics_support import assert_statistics
from api_support import enable_api, fetch_status
from network_process_tests import Hub
from tls_support import credentials, connect_tls


class ClientCapabilitiesProcessTests(unittest.TestCase):
    def test_live_modes_and_address_updates(self):
        for ipv6 in (False, True):
            with self.subTest(ipv6=ipv6), tempfile.TemporaryDirectory(prefix='dc24h-capabilities-') as directory:
                suffix, host, address = (b'6', '[::1]', b'::1') if ipv6 else (b'4', '127.0.0.1', b'127.0.0.1')
                cert, _ = credentials(directory)
                ini = pathlib.Path(directory) / 'hub.ini'
                ini.write_text(f'[network]\nlisten={host}:0\nadcs_listen={host}:0\n[tls]\ncertificate=certificate.pem\nprivate_key=private.pem\n')
                endpoint, secret = enable_api(ini, directory)
                hub = Hub(self, '--config', str(ini), '--log-connections', 'on', '--log-chat', 'on')
                peers = []
                for n in range(1, 4):
                    peer = Peer(self, connect_tls(self, hub, cert) if n == 2 else hub.connect())
                    peer.handshake(fragmented=True)
                    peer.send(peer.identity(b'CapabilitiesUser' + str(n).encode(), n).replace(b'I40.0.0.0', b'I' + suffix + address))
                    while not peer.line().startswith(b'BINF ' + peer.sid + b' '):
                        pass
                    self.assertTrue(peer.line().startswith(b'IMSG '))
                    for old in peers:
                        self.assertTrue(old.line().startswith(b'BINF ' + peer.sid + b' '))
                    peers.append(peer)
                source, target, bystander = peers
                before = fetch_status(endpoint, secret)

                def update(peer, fields, normalized=None):
                    wire = b'BINF ' + peer.sid + b' ' + fields
                    peer.send(wire)
                    for recipient in peers:
                        self.assertEqual(recipient.line(), b'BINF ' + peer.sid + b' ' + (fields if normalized is None else normalized))

                sequence = 0

                def request(origin, recipient, active_source, active_target):
                    nonlocal sequence
                    for command, allowed in ((b'CTM', active_source), (b'RCM', not active_source and active_target)):
                        for scope in (b'D', b'E'):
                            sequence += 1
                            token = b'CapabilitiesPrivate' + str(sequence).encode()
                            protocol = b'ADCS/1.0' if sequence % 2 else b'ADC/1.0'
                            wire = scope + command + b' ' + origin.sid + b' ' + recipient.sid + b' ' + protocol + (b' 01515 ' if command == b'CTM' else b' ') + token
                            # Fragment the header across TCP/TLS writes.
                            origin.peer.sendall(wire[:3])
                            fence = b'BSTA ' + origin.sid + b' 000 CapabilityFence' + str(sequence).encode()
                            origin.peer.sendall(wire[3:] + b'\n' + fence + b'\n')
                            for peer in peers:
                                messages = []
                                while True:
                                    line = peer.line()
                                    if line == fence:
                                        break
                                    messages.append(line)
                                if allowed:
                                    expected = [wire] if peer is recipient or (scope == b'E' and peer is origin) else []
                                    self.assertEqual(messages, expected)
                                elif peer is origin:
                                    self.assertEqual(len(messages), 1)
                                    self.assertTrue(messages[0].startswith(b'ISTA 1'), messages)
                                    self.assertTrue(messages[0].endswith(b' FC' + scope + command), messages)
                                else:
                                    self.assertEqual(messages, [])

                for active_source in (False, True):
                    for active_target in (False, True):
                        for peer, active in ((source, active_source), (target, active_target)):
                            # UDP and ADCS declarations never imply incoming TCP.
                            update(peer, b'SUUDP' + suffix + b',ADCS' + (b',TCP' + suffix if active else b'') + b' U' + suffix + b'1515')
                        request(source, target, active_source, active_target)
                        request(target, source, active_target, active_source)
                # Activity in the other address family has no usable listener.
                other = b'4' if ipv6 else b'6'
                update(source, b'SUTCP' + other + b',ADC0,ADCS')
                request(source, target, False, True)
                # Exact feature cancellation makes both clients passive.
                update(target, b'SU')
                request(source, target, False, False)
                # A declared listener without its address cannot originate CTM.
                update(source, b'SUTCP' + suffix)
                update(source, b'I' + suffix)
                request(source, target, False, False)
                update(source, b'I' + suffix + (b'::' if ipv6 else b'0.0.0.0'), b'I' + suffix + address)
                request(source, target, True, False)
                after = fetch_status(endpoint, secret)
                self.assertEqual(before['chat'], after['chat'])
                self.assertEqual(before['search'], after['search'])
                assert_statistics(self, after, ctm_received=24, ctm_routed=10, ctm_rejected=14, ctm_queued=15, rcm_received=24, rcm_routed=6, rcm_rejected=18, rcm_queued=9)
                self.assertNotIn('CapabilitiesPrivate', json.dumps(after))
                # Readiness survives recoverable refusals.
                chat = b'BMSG ' + bystander.sid + b' CapabilityRecovery'
                bystander.send(chat)
                for peer in peers:
                    self.assertEqual(peer.line(), chat)
                hub.stop()
                for private in (b'CapabilitiesPrivate', secret.encode()):
                    self.assertNotIn(private, hub.log)


if __name__ == '__main__':
    unittest.main()
