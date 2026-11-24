"""Explicit ADC/ADCS intent and peer failure replies stay private and lossless."""
import json
import pathlib
import tempfile
import unittest
from adc_login_process_tests import Peer, pair
from transfer_statistics_support import assert_statistics
from api_support import enable_api, fetch_status
from network_process_tests import Hub, drain_until_eof
from tls_support import credentials, connect_tls


CORPUS = pathlib.Path(__file__).with_name('beta06_adc_compatibility_corpus.json')


class ProtocolNegotiationProcessTests(unittest.TestCase):
    def test_beta06_versioned_adc_adcs_wire_corpus(self):
        """Actual on-loopback peers, never a simulated ADC parser or external client."""
        cases = json.loads(CORPUS.read_text(encoding='utf-8'))
        self.assertEqual(cases['schema'], 1)
        self.assertEqual(cases['version'], 'beta-0.6.9')
        self.assertEqual(len(cases['cases']), 4)
        for encrypted in (False, True):
            with self.subTest(transport='ADCS' if encrypted else 'ADC'), tempfile.TemporaryDirectory(prefix='dc24h-adc-corpus-') as directory:
                cert, _ = credentials(directory)
                ini = pathlib.Path(directory) / 'hub.ini'
                ini.write_text('[network]\nlisten=127.0.0.1:0\nadcs_listen=127.0.0.1:0\n'
                               '[tls]\ncertificate=certificate.pem\nprivate_key=private.pem\n')
                hub = Hub(self, '--config', str(ini))
                peer = Peer(self, connect_tls(self, hub, cert) if encrypted else hub.connect())
                peer.login(cases['cases'][0]['nick'].encode('utf-8'), 191, fragmented=True)
                self.assertTrue(peer.sid)

                bad_cid = Peer(self, connect_tls(self, hub, cert) if encrypted else hub.connect())
                bad_cid.handshake(fragmented=True)
                bad_cid.send(bad_cid.identity(b'InvalidCID', 192).replace(pair(192)[1], pair(193)[1]))
                self.assertTrue(bad_cid.line().startswith(b'ISTA ' + cases['cases'][1]['status'].encode() + b' '))
                self.assertEqual(bad_cid.line(), b'IQUI ' + bad_cid.sid)
                self.assertEqual(bad_cid.peer.recv(1), b'')

                bad_sid = Peer(self, connect_tls(self, hub, cert) if encrypted else hub.connect())
                bad_sid.handshake()
                bad_sid.send(cases['cases'][2]['frame'].encode('ascii'))
                self.assertTrue(bad_sid.line().startswith(b'ISTA ' + cases['cases'][2]['status'].encode() + b' '))

                early = Peer(self, connect_tls(self, hub, cert) if encrypted else hub.connect())
                early.send(cases['cases'][3]['frame'].encode('ascii'))
                self.assertTrue(early.line().startswith(b'ISTA ' + cases['cases'][3]['status'].encode() + b' '))
                peer.send(b'BSTA ' + peer.sid + b' 000 CorpusAlive')
                self.assertEqual(peer.line(), b'BSTA ' + peer.sid + b' 000 CorpusAlive')
                hub.stop()

    def test_explicit_protocols_and_peer_rejections(self):
        for ipv6 in (False, True):
            with self.subTest(ipv6=ipv6), tempfile.TemporaryDirectory(prefix='dc24h-protocol-') as directory:
                suffix, host = (b'6', '[::1]') if ipv6 else (b'4', '127.0.0.1')
                cert, _ = credentials(directory)
                ini = pathlib.Path(directory) / 'hub.ini'
                ini.write_text(f'[network]\nlisten={host}:0\nadcs_listen={host}:0\n[tls]\ncertificate=certificate.pem\nprivate_key=private.pem\n')
                endpoint, secret = enable_api(ini, directory)
                hub = Hub(self, '--config', str(ini), '--log-connections', 'on', '--log-chat', 'on')
                peers = []

                def login(n, encrypted):
                    peer = Peer(self, connect_tls(self, hub, cert) if encrypted else hub.connect())
                    peer.handshake()
                    identity = peer.identity(b'ProtocolUser' + str(n).encode(), n)
                    identity = identity.replace(b'I40.0.0.0', b'I' + suffix + (b'::' if ipv6 else b'0.0.0.0'))
                    peer.send(identity)
                    line = peer.line()
                    while not line.startswith(b'BINF ' + peer.sid + b' '):
                        line = peer.line()
                    self.assertTrue(peer.line().startswith(b'IMSG '))
                    for old in peers:
                        self.assertTrue(old.line().startswith(b'BINF ' + peer.sid + b' '))
                    peers.append(peer)
                    return peer

                active, passive, bystander = login(1, False), login(2, True), login(3, False)
                before = fetch_status(endpoint, secret)
                serial = 0

                def fence():
                    nonlocal serial
                    serial += 1
                    # STA, not INF: ready-session presence has a separate budget.
                    frame = b'BSTA ' + bystander.sid + b' 000 ProtocolFence' + str(serial).encode()
                    bystander.send(frame)
                    for peer in peers:
                        self.assertEqual(peer.line(), frame)

                # SU and hub TLS are independent from the explicit protocol.
                for features in (b'TCP' + suffix + b',ADCS', b'TCP' + suffix):
                    update = b'BINF ' + active.sid + b' SU' + features
                    active.send(update)
                    for peer in peers:
                        self.assertEqual(peer.line(), update)
                    for reverse in (False, True):
                        origin, target = (passive, active) if reverse else (active, passive)
                        for echo in (False, True):
                            for protocol in (b'ADC/1.0', b'ADCS/1.0', b'future/1.0', b'adcs/1.0', b'ADCS/0.10'):
                                token = 'ProtocolPrivate\\sŻółw\\\\token'.encode()
                                command = (b'E' if echo else b'D') + (b'RCM' if reverse else b'CTM')
                                request = command + b' ' + origin.sid + b' ' + target.sid + b' ' + protocol + b' '
                                request += (b'' if reverse else b'001515 ') + token + b' XXkeep'
                                for byte in request + b'\n':
                                    origin.peer.sendall(bytes([byte]))
                                self.assertEqual(target.line(), request)
                                if echo:
                                    self.assertEqual(origin.line(), request)
                                fence()
                                # The recipient owns unsupported/failed decisions.
                                for code in (b'141', b'142'):
                                    response = b'DSTA ' + target.sid + b' ' + origin.sid + b' ' + code
                                    response += b' Peer\\snegotiation\\sfailed PR' + protocol + b' TO' + token
                                    target.send(response + b' XXNegotiationPrivateUnknown')
                                    self.assertEqual(origin.line(), response)
                                    fence()
                                if reverse:
                                    reply = b'DCTM ' + target.sid + b' ' + origin.sid + b' ' + protocol + b' 001516 ' + token
                                    target.send(reply)
                                    self.assertEqual(origin.line(), reply)
                                    fence()
                after = fetch_status(endpoint, secret)
                self.assertEqual(after['chat'], before['chat'])
                self.assertEqual(after['search'], before['search'])
                assert_statistics(self, after, ctm_received=40, ctm_routed=40, ctm_queued=50, rcm_received=20, rcm_routed=20, rcm_queued=30, peer_protocol_unsupported=40, peer_connection_failed=40)
                for private in ('ProtocolPrivate', 'NegotiationPrivateUnknown', secret):
                    self.assertNotIn(private, json.dumps(after))
                # Another source SID cannot forge the peer's rejection.
                passive.send(b'DSTA ' + active.sid + b' ' + bystander.sid + b' 141 Forged PRADCS/1.0 TOProtocolPrivate')
                self.assertTrue(drain_until_eof(passive.peer).startswith(b'ISTA 240 '))
                peers.remove(passive)
                for peer in peers:
                    self.assertEqual(peer.line(), b'IQUI ' + passive.sid)
                message = b'BMSG ' + active.sid + b' ProtocolRecoveryChat'
                active.send(message)
                for peer in peers:
                    self.assertEqual(peer.line(), message)
                hub.stop()
                for private in (b'ProtocolPrivate', b'NegotiationPrivateUnknown', secret.encode()):
                    self.assertNotIn(private, hub.log)


if __name__ == '__main__':
    unittest.main()
