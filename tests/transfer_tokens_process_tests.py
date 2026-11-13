"""Opaque transfer tokens stay byte-exact and private on mixed TCP/TLS pairs."""
import json
import pathlib
import tempfile
import unittest

from adc_login_process_tests import Peer
from api_support import enable_api, fetch_status
from network_process_tests import Hub, drain_until_eof
from tls_support import credentials, connect_tls


def escaped(value):
    return value.replace(b'\\', b'\\\\').replace(b' ', b'\\s').replace(b'\n', b'\\n')


class TransferTokensProcessTests(unittest.TestCase):
    def test_token_boundaries_and_same_token_pairs(self):
        for ipv6 in (False, True):
            with self.subTest(ipv6=ipv6), tempfile.TemporaryDirectory(prefix='dc24h-transfer-tokens-') as directory:
                suffix, host = (b'6', '[::1]') if ipv6 else (b'4', '127.0.0.1')
                cert, _ = credentials(directory)
                ini = pathlib.Path(directory) / 'hub.ini'
                ini.write_text(f'[network]\nlisten={host}:0\nadcs_listen={host}:0\n[tls]\ncertificate=certificate.pem\nprivate_key=private.pem\n')
                endpoint, secret = enable_api(ini, directory)
                hub = Hub(self, '--config', str(ini), '--log-connections', 'on', '--log-chat', 'on')
                peers = []

                def login(n):
                    peer = Peer(self, connect_tls(self, hub, cert) if n % 2 == 0 else hub.connect())
                    peer.handshake(fragmented=True)
                    identity = peer.identity(b'TokenUser' + str(n).encode(), n)
                    peer.send(identity.replace(b'I40.0.0.0', b'I' + suffix + (b'::' if ipv6 else b'0.0.0.0')))
                    while not peer.line().startswith(b'BINF ' + peer.sid + b' '):
                        pass
                    self.assertTrue(peer.line().startswith(b'IMSG '))
                    for old in peers:
                        self.assertTrue(old.line().startswith(b'BINF ' + peer.sid + b' '))
                    peers.append(peer)
                    return peer

                active_a, passive_b, passive_c, active_d, bystander = [login(n) for n in range(1, 6)]
                for active in (active_a, active_d):
                    update = b'BINF ' + active.sid + b' SUTCP' + suffix
                    active.send(update)
                    for peer in peers:
                        self.assertEqual(peer.line(), update)
                before = fetch_status(endpoint, secret)
                serial = 0

                def wave(requests, expected):
                    nonlocal serial
                    fences = set()
                    for origin, frame in requests:
                        serial += 1
                        fence = b'BINF ' + origin.sid + b' DETokenFence' + str(serial).encode()
                        fences.add(fence)
                        # Exercise stream fragmentation with source-local ordering.
                        origin.peer.sendall(frame[:3])
                        origin.peer.sendall(frame[3:] + b'\n' + fence + b'\n')
                    for peer in peers:
                        seen, frames = set(), []
                        while seen != fences:
                            line = peer.line()
                            if line in fences:
                                seen.add(line)
                            else:
                                frames.append(line)
                        self.assertCountEqual(frames, expected.get(peer, []), f'Wrong token audience for {peer.sid!r}')

                tokens = [b'A', b' ', b'  TokenPrivate  ', 'TokenPrivate Żółw \\😀'.encode(), b'TOpositional',
                          b'0' * 128, b' ' * 128, b'\\' * 128, b'T' * 124 + '😀'.encode()]
                for token in tokens:
                    encoded = escaped(token)
                    for scope in (b'D', b'E'):
                        ctm = scope + b'CTM ' + active_a.sid + b' ' + passive_b.sid + b' ADCS/1.0 01515 ' + encoded + b' TOextension'
                        rcm = scope + b'RCM ' + passive_c.sid + b' ' + active_d.sid + b' ADC/1.0 ' + encoded + b' TOextension'
                        expected = {passive_b: [ctm], active_d: [rcm]}
                        if scope == b'E':
                            expected.update({active_a: [ctm], passive_c: [rcm]})
                        wave([(active_a, ctm), (passive_c, rcm)], expected)
                        reply = b'DCTM ' + active_d.sid + b' ' + passive_c.sid + b' ADC/1.0 01516 ' + encoded
                        failure = b'DSTA ' + passive_b.sid + b' ' + active_a.sid + b' 142 Peer\\sfailed PRADCS/1.0 TO' + encoded
                        wave([(active_d, reply), (passive_b, failure + b' XXTokenPrivateUnknown')], {passive_c: [reply], active_a: [failure]})
                        # A new independent attempt can reuse bytes without a registry collision.
                        wave([(active_a, ctm)], {passive_b: [ctm], **({active_a: [ctm]} if scope == b'E' else {})})
                for token in (b'T' * 129, b'T' * 125 + '😀'.encode(), b'bad\nTokenPrivate', 'bad\u200bTokenPrivate'.encode()):
                    for reverse, origin, target in ((False, active_a, passive_b), (True, passive_c, active_d)):
                        command = b'ERCM' if reverse else b'ECTM'
                        frame = command + b' ' + origin.sid + b' ' + target.sid + b' ADC/1.0 ' + (b'' if reverse else b'1515 ') + escaped(token)
                        error = b'ISTA 100 Invalid\\sconnection\\srequest FC' + command
                        wave([(origin, frame)], {origin: [error]})
                # An identical token never revives the public SID of a departed client.
                old_sid = passive_b.sid
                passive_b.peer.close()
                peers.remove(passive_b)
                for peer in peers:
                    self.assertEqual(peer.line(), b'IQUI ' + old_sid)
                replacement = login(6)
                stale = b'DCTM ' + active_a.sid + b' ' + old_sid + b' ADC/1.0 1515 TokenPrivate'
                wave([(active_a, stale)], {active_a: [b'ISTA 140 CTM\\srecipient\\sunavailable FCDCTM']})
                fresh = stale.replace(old_sid, replacement.sid)
                wave([(active_a, fresh)], {replacement: [fresh]})
                after = fetch_status(endpoint, secret)
                self.assertEqual(after['chat'], before['chat'])
                self.assertEqual(after['search'], before['search'])
                self.assertNotIn('transfers', after)
                self.assertNotIn('TokenPrivate', json.dumps(after))
                # Ambiguous peer failure TO is rejected rather than guessed.
                replacement.send(b'DSTA ' + replacement.sid + b' ' + active_a.sid + b' 142 Failed PRADC/1.0 TOTokenPrivate TOother')
                self.assertTrue(drain_until_eof(replacement.peer).startswith(b'ISTA 2'))
                peers.remove(replacement)
                for peer in peers:
                    self.assertEqual(peer.line(), b'IQUI ' + replacement.sid)
                chat = b'BMSG ' + bystander.sid + b' TokenRecovery'
                bystander.send(chat)
                for peer in peers:
                    self.assertEqual(peer.line(), chat)
                hub.stop()
                for private in (b'TokenPrivate', b'TOpositional', secret.encode()):
                    self.assertNotIn(private, hub.log)


if __name__ == '__main__':
    unittest.main()
