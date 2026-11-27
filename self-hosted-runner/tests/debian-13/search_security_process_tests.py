"""Search quotas, guest policy, token caps and privacy on real ADC/ADCS."""
import json
import pathlib
import subprocess
import tempfile
import time
import unittest

from adc_login_process_tests import Peer
from api_support import enable_api, fetch_status
from network_process_tests import BINARY, Hub, drain_until_eof
from tls_support import credentials, connect_tls


class SearchSecurityProcessTests(unittest.TestCase):
    def fixture(self, encrypted, settings):
        folder = tempfile.TemporaryDirectory(prefix='dc24h-search-security-'); self.addCleanup(folder.cleanup)
        cert, _ = credentials(folder.name)
        ini = pathlib.Path(folder.name) / 'hub.ini'
        ini.write_text('[network]\nlisten=127.0.0.1:0\nadcs_listen=127.0.0.1:0\n'
                       '[tls]\ncertificate=certificate.pem\nprivate_key=private.pem\n'
                       '[security]\nmax_frames_per_second=10000\n[search]\n' + settings)
        endpoint, secret = enable_api(ini, folder.name)
        hub = Hub(self, '--config', str(ini), '--log-connections', 'on', '--log-chat', 'on')
        peers = []
        for n in range(3):
            peer = Peer(self, connect_tls(self, hub, cert) if encrypted else hub.connect())
            peer.login(b'SecurityUser' + str(n).encode(), n+1)
            for existing in peers: self.assertTrue(existing.line().startswith(b'BINF ' + peer.sid + b' '))
            peers.append(peer)
        return hub, endpoint, secret, peers, cert

    def fence(self, peers):
        frame = b'BSTA ' + peers[1].sid + b' 000 SecurityFence'
        peers[1].send(frame)
        for peer in peers: self.assertEqual(peer.line(), frame)

    def query(self, source, target, token):
        frame = b'DSCH ' + source.sid + b' ' + target.sid + b' ANSecurityPrivate TO' + token
        source.send(frame)
        return frame

    def result(self, source, target, token=None):
        frame = b'DRES ' + source.sid + b' ' + target.sid + b' FN/SecurityPrivate SI0 TR' + b'A'*39
        if token is not None: frame += b' TO' + token
        source.send(frame)
        return frame

    def test_query_sender_and_recipient_flood_recovery(self):
        for encrypted in (False, True):
            with self.subTest(encrypted=encrypted):
                hub, endpoint, secret, peers, _ = self.fixture(encrypted, 'query_rate=1\nquery_burst=1\nresult_rate=1\nresult_burst=1\n')
                source, responder, other = peers
                before = fetch_status(endpoint, secret)['chat']
                frame = self.query(source, responder, b'SecurityPrivateToken'); self.assertEqual(responder.line(), frame)
                self.query(source, responder, b'SecurityRefusedToken')
                self.assertEqual(source.line(), b'ISTA 100 Search\\srate\\slimit\\sexceeded FCDSCH')
                for _ in range(8): self.query(source, responder, b'SecurityRefusedToken')
                self.fence(peers)
                time.sleep(1.05)
                frame = self.query(source, responder, b'SecurityRefusedToken'); self.assertEqual(responder.line(), frame)
                self.fence(peers)
                frame = self.result(responder, source, b'SecurityPrivateToken'); self.assertEqual(source.line(), frame)
                self.result(other, source, b'SecurityRefusedToken')
                self.assertEqual(other.line(), b'ISTA 100 Search\\srecipient\\srate\\slimit\\sexceeded FCDRES')
                for _ in range(5): self.result(other, source, b'SecurityRefusedToken')
                self.fence(peers)
                time.sleep(1.05)
                frame = self.result(other, source, b'SecurityRefusedToken'); self.assertEqual(source.line(), frame)
                self.fence(peers)
                self.assertEqual(fetch_status(endpoint, secret)['chat'], before)
                waiting = Peer(self, hub.connect()); waiting.handshake()
                waiting.send(b'DSCH ' + waiting.sid + b' ' + source.sid + b' ANbad')
                self.assertTrue(drain_until_eof(waiting.peer).startswith(b'ISTA 244 '))
                source.send(b'DSCH ' + other.sid + b' ' + responder.sid + b' ANbad')
                self.assertTrue(drain_until_eof(source.peer).startswith(b'ISTA 240 '))
                for peer in (responder, other): self.assertEqual(peer.line(), b'IQUI ' + source.sid)
                snapshot = json.dumps(fetch_status(endpoint, secret))
                for private in ('SecurityPrivate', 'SecurityRefused', secret): self.assertNotIn(private, snapshot)
                hub.stop()
                for private in (b'SecurityPrivate', b'SecurityRefused', secret.encode()): self.assertNotIn(private, hub.log)

    def test_global_guest_policy_and_strict_token(self):
        policies = [('queries_enabled=off\n', True, b'ISTA 125 Search\\soperation\\sis\\sdisabled FCDSCH'),
                    ('guest_queries=off\n', True, b'ISTA 125 Search\\soperation\\snot\\spermitted FCDSCH'),
                    ('results_enabled=off\n', False, b'ISTA 125 Search\\soperation\\sis\\sdisabled FCDRES'),
                    ('guest_results=off\n', False, b'ISTA 125 Search\\soperation\\snot\\spermitted FCDRES'),
                    ('require_result_token=on\n', False, b'ISTA 100 Search\\sresult\\stoken\\srequired FCDRES')]
        for encrypted in (False, True):
            for settings, query, expected in policies:
                with self.subTest(encrypted=encrypted, settings=settings):
                    hub, _, _, peers, _ = self.fixture(encrypted, settings)
                    source, target, _ = peers
                    if query: self.query(source, target, b'SecurityPolicyToken')
                    else: self.result(source, target)
                    self.assertEqual(source.line(), expected)
                    # Public feature/negotiation claims do not grant privileges.
                    source.send(b'HSUP ADOPER')
                    for _ in range(4):
                        if query: self.query(source, target, b'SecurityPolicyToken')
                        else: self.result(source, target)
                    self.fence(peers)
                    hub.stop()

    def test_configured_active_limits_expiry_and_cleanup(self):
        for encrypted in (False, True):
            hub, _, _, peers, cert = self.fixture(encrypted, 'query_rate=1000\nquery_burst=100\nmax_active_searches=1\nmax_active_per_user=1\nsearch_lifetime_seconds=1\n')
            source, target, other = peers
            frame = self.query(source, target, b'SecurityCap'); self.assertEqual(target.line(), frame)
            self.query(source, target, b'SecurityCap2')
            self.assertEqual(source.line(), b'ISTA 100 Active\\ssearch\\slimit\\sreached FCDSCH')
            self.query(other, target, b'SecurityCap3')
            self.assertEqual(other.line(), b'ISTA 100 Active\\ssearch\\slimit\\sreached FCDSCH')
            self.fence(peers)
            old_sid = source.sid; source.peer.close()
            for peer in (target, other): self.assertEqual(peer.line(), b'IQUI ' + old_sid)
            source = Peer(self, connect_tls(self, hub, cert) if encrypted else hub.connect()); source.login(b'SecurityUser0', 1)
            for peer in (target, other): self.assertTrue(peer.line().startswith(b'BINF ' + source.sid + b' '))
            peers[0] = source
            frame = self.query(source, target, b'SecurityCap'); self.assertEqual(target.line(), frame)
            self.fence(peers)
            time.sleep(1.05)
            frame = self.query(source, target, b'SecurityCap2'); self.assertEqual(target.line(), frame)
            self.fence(peers); hub.stop()

    def test_configuration_bounds(self):
        invalid = ['query_rate=0', 'result_rate=0', 'query_burst=0', 'result_burst=1000001',
                   'query_byte_rate=0', 'result_byte_rate=16777217', 'query_byte_burst=16383',
                   'result_byte_burst=0', 'max_active_searches=0', 'max_active_per_user=65',
                   'max_active_searches=1\nmax_active_per_user=2', 'search_lifetime_seconds=3601',
                   'guest_queries=maybe', 'queries_enabled=2', 'query_rate=1\nquery_rate=2', 'unknown=1']
        with tempfile.TemporaryDirectory(prefix='dc24h-search-settings-') as directory:
            ini = pathlib.Path(directory) / 'hub.ini'
            for fields in invalid:
                with self.subTest(fields=fields):
                    ini.write_text('[search]\n' + fields + '\n')
                    result = subprocess.run([BINARY, '--config', str(ini), '--check-config'], capture_output=True, timeout=5)
                    self.assertNotEqual(result.returncode, 0)
            ini.write_text('[search]\nmax_active_searches=65535\nmax_active_per_user=64\nsearch_lifetime_seconds=3600\nquery_byte_burst=16384\nresult_byte_burst=16384\n')
            result = subprocess.run([BINARY, '--config', str(ini), '--check-config'], capture_output=True, timeout=5)
            self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == '__main__':
    unittest.main()
