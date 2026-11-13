"""Live fragmented D/E CTM over IPv4/IPv6 ADC/ADCS, with isolation and privacy."""
import json
import pathlib
import tempfile
import unittest
from adc_login_process_tests import Peer
from api_support import enable_api, fetch_status
from network_process_tests import Hub, drain_until_eof
from tls_support import credentials, connect_tls


class CtmProcessTests(unittest.TestCase):
    def test_ctm_routing_recovery_and_privacy(self):
        for ipv6 in (False, True):
            for encrypted in (False, True):
                with self.subTest(ipv6=ipv6, encrypted=encrypted), tempfile.TemporaryDirectory(prefix='dc24h-ctm-') as directory:
                    suffix, host = (b'6', '[::1]') if ipv6 else (b'4', '127.0.0.1')
                    cert, _ = credentials(directory)
                    ini = pathlib.Path(directory)/'hub.ini'
                    ini.write_text(f'[network]\nlisten={host}:0\nadcs_listen={host}:0\n[tls]\ncertificate=certificate.pem\nprivate_key=private.pem\n')
                    endpoint, secret = enable_api(ini, directory)
                    hub = Hub(self, '--config', str(ini), '--log-connections', 'on', '--log-chat', 'on')
                    peers = []

                    def login(n):
                        peer = Peer(self, connect_tls(self, hub, cert) if encrypted else hub.connect())
                        peer.handshake()
                        identity = peer.identity(b'CtmUser'+str(n).encode(), n).replace(b'I40.0.0.0', b'I'+suffix+(b'::' if ipv6 else b'0.0.0.0'))
                        peer.send(identity)
                        line = peer.line()
                        while not line.startswith(b'BINF '+peer.sid+b' '): line = peer.line()
                        self.assertTrue(peer.line().startswith(b'IMSG '))
                        for old in peers: self.assertTrue(old.line().startswith(b'BINF '+peer.sid+b' '))
                        peers.append(peer)
                        return peer

                    source, target, other = login(1), login(2), login(3)
                    before = fetch_status(endpoint, secret)

                    def update(features):
                        wire = b'BINF '+source.sid+b' SU'+features
                        source.send(wire)
                        for peer in peers: self.assertEqual(peer.line(), wire)

                    def fence():
                        wire = b'BINF '+other.sid+b' DECtmFence'
                        other.send(wire)
                        for peer in peers: self.assertEqual(peer.line(), wire)

                    def ctm(kind=b'DCTM', protocol=b'ADC/1.0', port=b'01515', token=b'CtmSecret\\stoken', recipient=None):
                        return kind+b' '+source.sid+b' '+(target.sid if recipient is None else recipient)+b' '+protocol+b' '+port+b' '+token

                    source.send(ctm()); self.assertEqual(source.line(), b'ISTA 100 CTM\\ssender\\sis\\snot\\sTCP\\sactive FCDCTM'); fence()
                    update(b'UDP'+suffix)
                    source.send(ctm()); self.assertTrue(source.line().startswith(b'ISTA 100 CTM\\ssender')); fence()
                    update(b'TCP'+suffix)
                    for kind, protocol in ((b'DCTM', b'ADC/1.0'), (b'ECTM', b'ADCS/1.0'), (b'DCTM', b'future/1.0')):
                        wire = ctm(kind, protocol)+b' XXopaque XXagain'
                        for byte in wire+b'\n': source.peer.sendall(bytes([byte]))
                        self.assertEqual(target.line(), wire)
                        if kind == b'ECTM': self.assertEqual(source.line(), wire)
                        fence()
                    for wire in (ctm(port=b'0'), ctm(port=b'65536'), ctm(token=b'T'*129),
                                 ctm(protocol=b'bad\\sprotocol'), ctm()+b' XX'+b'x'*2048,
                                 b'BCTM '+source.sid+b' ADC/1.0 1515 CtmSecret',
                                 b'FCTM '+source.sid+b' +TCP4 ADC/1.0 1515 CtmSecret', ctm(recipient=source.sid)):
                        source.send(wire); self.assertTrue(source.line().startswith(b'ISTA 100 Invalid\\sconnection\\srequest FC')); fence()
                    source.send(ctm(b'ECTM', recipient=b'ZZZZ')); self.assertEqual(source.line(), b'ISTA 140 CTM\\srecipient\\sunavailable FCECTM'); fence()
                    source.send(b'DRCM '+source.sid+b' '+target.sid+b' ADC/1.0 CtmSecret')
                    self.assertEqual(source.line(), b'ISTA 100 RCM\\srequires\\spassive\\ssender\\sand\\sactive\\srecipient FCDRCM'); fence()
                    after = fetch_status(endpoint, secret)
                    self.assertEqual(after['chat'], before['chat']); self.assertEqual(after['search'], before['search'])
                    self.assertNotIn('transfers', after)
                    for private in ('CtmSecret', 'opaque', secret): self.assertNotIn(private, json.dumps(after))
                    old_sid = target.sid; target.peer.close(); peers.remove(target)
                    for peer in peers: self.assertEqual(peer.line(), b'IQUI '+old_sid)
                    target = login(4); self.assertNotEqual(target.sid, old_sid)
                    source.send(ctm(recipient=old_sid)); self.assertTrue(source.line().startswith(b'ISTA 140 ')); fence()
                    wire = ctm(); source.send(wire); self.assertEqual(target.line(), wire); fence()
                    waiting = Peer(self, connect_tls(self, hub, cert) if encrypted else hub.connect()); waiting.handshake()
                    waiting.send(b'DCTM '+waiting.sid+b' '+target.sid+b' ADC/1.0 1515 CtmSecret')
                    self.assertTrue(drain_until_eof(waiting.peer).startswith(b'ISTA 244 '))
                    source.send(b'DCTM '+target.sid+b' '+other.sid+b' ADC/1.0 1515 CtmSecret')
                    self.assertTrue(drain_until_eof(source.peer).startswith(b'ISTA 240 '))
                    for peer in (target, other): self.assertEqual(peer.line(), b'IQUI '+source.sid)
                    target.send(b'BMSG '+target.sid+b' CtmRecoveryChat')
                    for peer in (target, other): self.assertEqual(peer.line(), b'BMSG '+target.sid+b' CtmRecoveryChat')
                    hub.stop()
                    self.assertNotIn(b'CtmSecret', hub.log); self.assertNotIn(b'opaque', hub.log); self.assertNotIn(secret.encode(), hub.log)


if __name__ == '__main__': unittest.main()
