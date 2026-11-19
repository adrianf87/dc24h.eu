"""Connection failures, routed fatal STA, disconnect/reset and SID reuse."""
import json
import pathlib
import socket
import struct
import tempfile
import unittest

from adc_login_process_tests import Peer
from api_support import enable_api, fetch_status
from network_process_tests import Hub, drain_until_eof
from tls_support import credentials, connect_tls


class TransferErrorsProcessTests(unittest.TestCase):
    def test_peer_errors_and_disconnect_recovery(self):
        for ipv6 in (False, True):
            with self.subTest(ipv6=ipv6), tempfile.TemporaryDirectory(prefix='dc24h-transfer-errors-') as directory:
                suffix, host = (b'6', '[::1]') if ipv6 else (b'4', '127.0.0.1')
                cert, _ = credentials(directory)
                ini = pathlib.Path(directory) / 'hub.ini'
                ini.write_text(f'[network]\nlisten={host}:0\nadcs_listen={host}:0\n[tls]\ncertificate=certificate.pem\nprivate_key=private.pem\n')
                endpoint, secret = enable_api(ini, directory)
                hub = Hub(self, '--config', str(ini), '--log-connections', 'on', '--log-chat', 'on')
                peers = []

                def login(n, identity_seed=None):
                    peer = Peer(self, connect_tls(self, hub, cert) if n % 2 == 0 else hub.connect())
                    peer.handshake(fragmented=True)
                    seed = n if identity_seed is None else identity_seed
                    info = peer.identity(b'ErrorUser' + str(seed).encode(), seed)
                    peer.send(info.replace(b'I40.0.0.0', b'I' + suffix + (b'::' if ipv6 else b'0.0.0.0')))
                    while not peer.line().startswith(b'BINF ' + peer.sid + b' '):
                        pass
                    self.assertTrue(peer.line().startswith(b'IMSG '))
                    for old in peers:
                        self.assertTrue(old.line().startswith(b'BINF ' + peer.sid + b' '))
                    peers.append(peer)
                    return peer

                active, passive, bystander = [login(n) for n in range(1, 4)]
                update = b'BINF ' + active.sid + b' SUTCP' + suffix
                active.send(update)
                for peer in peers:
                    self.assertEqual(peer.line(), update)
                before = fetch_status(endpoint, secret)
                sequence = 0

                def exchange(origin, frame, expected):
                    nonlocal sequence
                    sequence += 1
                    fence = b'BSTA ' + origin.sid + b' 000 ErrorFence' + str(sequence).encode()
                    origin.peer.sendall(frame[:3])
                    origin.peer.sendall(frame[3:] + b'\n' + fence + b'\n')
                    for peer in peers:
                        received = []
                        while True:
                            line = peer.line()
                            if line == fence:
                                break
                            received.append(line)
                        self.assertEqual(received, expected.get(peer, []))

                token = 'TransferErrorPrivate\\sŻółw\\\\token'.encode()
                for scope in (b'D', b'E'):
                    offer = scope + b'CTM ' + active.sid + b' ' + passive.sid + b' ADCS/1.0 01515 ' + token
                    exchange(active, offer, {passive: [offer], **({active: [offer]} if scope == b'E' else {})})
                    for code in (b'141', b'142', b'241', b'242', b'199', b'299'):
                        failure_token = token if code[1:] in (b'41', b'42') else b'TransferErrorPrivate'
                        failure = scope + b'STA ' + passive.sid + b' ' + active.sid + b' ' + code + b' Peer\\sconnection\\sfailed PRADCS/1.0 TO' + failure_token
                        exchange(passive, failure + b' XXErrorPrivateUnknown PDdiscard', {active: [failure], **({passive: [failure]} if scope == b'E' else {})})
                for scope in (b'D', b'E'):
                    reverse = scope + b'RCM ' + passive.sid + b' ' + active.sid + b' ADC/1.0 ' + token
                    exchange(passive, reverse, {active: [reverse], **({passive: [reverse]} if scope == b'E' else {})})
                    reply = b'DCTM ' + active.sid + b' ' + passive.sid + b' ADC/1.0 1515 ' + token
                    exchange(active, reply, {passive: [reply]})
                # Unscoped success/recoverable status has no response loop.
                for code in (b'000', b'199'):
                    exchange(passive, b'HSTA ' + code + b' HubPrivateStatus', {})
                # A fatal HSTA applies only to its own hub connection.
                departed = passive.sid
                passive.send(b'HSTA 299 PrivateFatalHubReason')
                self.assertEqual(drain_until_eof(passive.peer), b'')
                peers.remove(passive)
                for peer in peers:
                    self.assertEqual(peer.line(), b'IQUI ' + departed)
                missing = b'ESTA ' + active.sid + b' ' + departed + b' 142 Failed TO' + token
                exchange(active, missing, {active: [b'ISTA 140 Status\\srecipient\\sunavailable']})
                replacement = login(4, 2)
                self.assertNotEqual(replacement.sid, departed)
                fresh = missing.replace(departed, replacement.sid)
                exchange(active, fresh, {replacement: [fresh], active: [fresh]})
                # Malformed known token metadata closes only the sender.
                replacement.send(b'DSTA ' + replacement.sid + b' ' + active.sid + b' 142 Malformed TOTokenA TOTokenB')
                self.assertTrue(drain_until_eof(replacement.peer).startswith(b'ISTA 240 '))
                peers.remove(replacement)
                for peer in peers:
                    self.assertEqual(peer.line(), b'IQUI ' + replacement.sid)
                # An abrupt reset is finalized once; another user remains usable.
                active.peer.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack('ii', 1, 0))
                active.peer.close()
                peers.remove(active)
                self.assertEqual(bystander.line(), b'IQUI ' + active.sid)
                after = fetch_status(endpoint, secret)
                self.assertEqual(before['chat'], after['chat'])
                self.assertEqual(before['search'], after['search'])
                self.assertEqual(after['users']['online'], 1)
                for value in ('TransferErrorPrivate', 'ErrorPrivateUnknown', 'PrivateFatalHubReason', secret):
                    self.assertNotIn(value, json.dumps(after))
                chat = b'BMSG ' + bystander.sid + b' ErrorHandlingRecovery'
                bystander.send(chat)
                self.assertEqual(bystander.line(), chat)
                hub.stop()
                self.assertEqual(hub.log.count(b'event=peer_status'), 1)
                for value in (b'TransferErrorPrivate', b'ErrorPrivateUnknown', b'PrivateFatalHubReason', secret.encode()):
                    self.assertNotIn(value, hub.log)


if __name__ == '__main__':
    unittest.main()
