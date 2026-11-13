"""Live IPv4/IPv6 endpoint changes and client-owned UDP paths, with hub fallback."""
import json
import pathlib
import socket
import tempfile
import unittest

from adc_login_process_tests import Peer, pair
from api_support import enable_api, fetch_status
from network_process_tests import Hub, drain_until_eof
from tls_support import credentials, connect_tls


class ConnectivityProcessTests(unittest.TestCase):
    def test_families_updates_udp_and_hub_fallback(self):
        for ipv6 in (False, True):
            for encrypted in (False, True):
                with self.subTest(ipv6=ipv6, encrypted=encrypted), tempfile.TemporaryDirectory(prefix='dc24h-paths-') as directory:
                    family, host, suffix = (socket.AF_INET6, '::1', b'6') if ipv6 else (socket.AF_INET, '127.0.0.1', b'4')
                    cert, _ = credentials(directory)
                    address = '[::1]' if ipv6 else host
                    ini = pathlib.Path(directory) / 'hub.ini'
                    ini.write_text(f'[network]\nlisten={address}:0\nadcs_listen={address}:0\n'
                                   '[tls]\ncertificate=certificate.pem\nprivate_key=private.pem\n'
                                   '[security]\nmax_frames_per_second=10000\n')
                    endpoint, secret = enable_api(ini, directory)
                    hub = Hub(self, '--config', str(ini), '--log-connections', 'on', '--log-chat', 'on')
                    peers = []
                    for n in range(3):
                        peer = Peer(self, connect_tls(self, hub, cert) if encrypted else hub.connect())
                        # Peer.login normally advertises I4; use the matching family.
                        peer.handshake(); peer.send(peer.identity(b'PathUser' + str(n).encode(), n+1).replace(b'I40.0.0.0', b'I' + suffix + (b'::' if ipv6 else b'0.0.0.0')))
                        line = peer.line()
                        while not line.startswith(b'BINF ' + peer.sid + b' '): line = peer.line()
                        self.assertTrue(peer.line().startswith(b'IMSG '))
                        for existing in peers: self.assertTrue(existing.line().startswith(b'BINF ' + peer.sid + b' '))
                        peers.append(peer)
                    seeker, responder, other = peers
                    udp = socket.socket(family, socket.SOCK_DGRAM); self.addCleanup(udp.close)
                    udp.bind((host, 0)); udp.settimeout(2)
                    before = fetch_status(endpoint, secret)['chat']

                    def update(fields, normalized=None):
                        seeker.send(b'BINF ' + seeker.sid + b' ' + fields)
                        expected = b'BINF ' + seeker.sid + b' ' + (fields if normalized is None else normalized)
                        for peer in peers: self.assertEqual(peer.line(), expected)

                    def fence():
                        frame = b'BINF ' + responder.sid + b' DEPathFence'
                        responder.send(frame)
                        for peer in peers: self.assertEqual(peer.line(), frame)

                    root = b'A' * 39
                    for state in ('active', 'zero', 'cancel', 'active-again'):
                        port = str(udp.getsockname()[1]).encode()
                        if state in ('active', 'active-again'):
                            fields = b'SUUDP' + suffix + b' U' + suffix + port
                        elif state == 'zero': fields = b'SUUDP' + suffix + b' U' + suffix + b'0'
                        else: fields = b'SU U' + suffix
                        update(fields)
                        query = b'DSCH ' + seeker.sid + b' ' + responder.sid + b' ANPathPrivate TOPath' + state.encode()
                        seeker.send(query); self.assertEqual(responder.line(), query)
                        fields = b'FN/PathPrivate.txt SI0 TR' + root + b' TOPath' + state.encode()
                        if state in ('active', 'active-again'):
                            # A client uses the validated INF advertisement; the hub
                            # forwards SCH, while URES travels on a separate socket.
                            with socket.socket(family, socket.SOCK_DGRAM) as sender:
                                datagram = b'URES ' + pair(2)[1] + b' ' + fields + b'\n'
                                sender.sendto(datagram, udp.getsockname())
                            self.assertEqual(udp.recvfrom(16384)[0], datagram)
                        else:
                            frame = b'DRES ' + responder.sid + b' ' + seeker.sid + b' ' + fields
                            responder.send(frame); self.assertEqual(seeker.line(), frame)
                        fence()
                    self.assertEqual(fetch_status(endpoint, secret)['chat'], before)
                    # Untrusted alternate addresses cannot become UDP destinations.
                    spoof = b'I' + suffix + (b'2001:db8::123' if ipv6 else b'192.0.2.123') + b' U' + suffix + b'12345 SUUDP' + suffix
                    seeker.send(b'BINF ' + seeker.sid + b' ' + spoof)
                    self.assertTrue(drain_until_eof(seeker.peer).startswith(b'ISTA 246 '))
                    for peer in (responder, other): self.assertEqual(peer.line(), b'IQUI ' + seeker.sid)
                    snapshot = json.dumps(fetch_status(endpoint, secret))
                    for private in ('PathPrivate', 'TOPath', secret): self.assertNotIn(private, snapshot)
                    hub.stop()
                    for private in (b'PathPrivate', b'TOPath', secret.encode()): self.assertNotIn(private, hub.log)


if __name__ == '__main__':
    unittest.main()
