"""Connection policy, endpoint/source spoofing and recovery on live ADC/ADCS."""
import json
import pathlib
import subprocess
import tempfile
import unittest

from adc_login_process_tests import Peer
from api_support import enable_api, fetch_status
from network_process_tests import BINARY, Hub, drain_until_eof
from tls_support import credentials, connect_tls


class ConnectionSecurityProcessTests(unittest.TestCase):
    def fixture(self, ipv6, policy=''):
        folder = tempfile.TemporaryDirectory(prefix='dc24h-connection-security-')
        self.addCleanup(folder.cleanup)
        suffix, host = (b'6', '[::1]') if ipv6 else (b'4', '127.0.0.1')
        cert, _ = credentials(folder.name)
        ini = pathlib.Path(folder.name) / 'hub.ini'
        ini.write_text(f'[network]\nlisten={host}:0\nadcs_listen={host}:0\n[tls]\ncertificate=certificate.pem\nprivate_key=private.pem\n[transfers]\n{policy}')
        endpoint, secret = enable_api(ini, folder.name)
        hub = Hub(self, '--config', str(ini), '--log-connections', 'on', '--log-chat', 'on')
        peers = []
        for n in range(1, 4):
            peer = Peer(self, connect_tls(self, hub, cert) if n == 2 else hub.connect())
            peer.handshake()
            peer.send(peer.identity(b'ConnectionSecurityUser' + str(n).encode(), n).replace(b'I40.0.0.0', b'I' + suffix + (b'::' if ipv6 else b'0.0.0.0')))
            while not peer.line().startswith(b'BINF ' + peer.sid + b' '):
                pass
            self.assertTrue(peer.line().startswith(b'IMSG '))
            for old in peers:
                self.assertTrue(old.line().startswith(b'BINF ' + peer.sid + b' '))
            peers.append(peer)
        active, passive, bystander = peers
        active.send(b'BINF ' + active.sid + b' SUTCP' + suffix)
        for peer in peers:
            self.assertEqual(peer.line(), b'BINF ' + active.sid + b' SUTCP' + suffix)
        return hub, peers, suffix, endpoint, secret

    def exchange(self, peers, source, frame, expected):
        fence = b'BSTA ' + source.sid + b' 000 SecurityFence'
        source.peer.sendall(frame[:3])
        source.peer.sendall(frame[3:] + b'\n' + fence + b'\n')
        for peer in peers:
            seen = []
            while True:
                line = peer.line()
                if line == fence:
                    break
                seen.append(line)
            self.assertEqual(seen, expected.get(peer, []))

    def test_global_and_guest_policy(self):
        policies = [('ctm_enabled=off\n', b'CTM', b'Connection\\soperation\\sis\\sdisabled'),
                    ('rcm_enabled=off\n', b'RCM', b'Connection\\soperation\\sis\\sdisabled'),
                    ('guest_initiate=off\n', None, b'Connection\\soperation\\snot\\spermitted'),
                    ('guest_receive=off\n', None, b'Connection\\soperation\\snot\\spermitted')]
        for ipv6 in (False, True):
            for policy, disabled_command, description in policies:
                with self.subTest(ipv6=ipv6, policy=policy):
                    hub, peers, suffix, endpoint, secret = self.fixture(ipv6, policy)
                    active, passive, _ = peers
                    before = fetch_status(endpoint, secret)
                    for command, source, target in ((b'CTM', active, passive), (b'RCM', passive, active)):
                        for scope in (b'D', b'E'):
                            frame = scope + command + b' ' + source.sid + b' ' + target.sid + b' ADCS/1.0 ' + (b'01515 ' if command == b'CTM' else b'') + b'SecurityPrivateToken'
                            if disabled_command is None or disabled_command == command:
                                expected = {source: [b'ISTA 125 ' + description + b' FC' + scope + command]}
                            else:
                                expected = {target: [frame]}
                                if scope == b'E':
                                    expected[source] = [frame]
                            self.exchange(peers, source, frame, expected)
                    # Public feature/application/bot claims never override a denial.
                    source, target = (passive, active) if disabled_command == b'RCM' else (active, passive)
                    delta = b'BINF ' + source.sid + b' CT1 APAdmin SUADCS' + (b',TCP' + suffix if source is active else b'')
                    source.send(delta)
                    for peer in peers:
                        self.assertEqual(peer.line(), delta)
                    command = disabled_command or b'CTM'
                    frame = b'D' + command + b' ' + source.sid + b' ' + target.sid + b' ADC/1.0 ' + (b'1515 ' if command == b'CTM' else b'') + b'SecurityPrivateToken'
                    self.exchange(peers, source, frame, {source: [b'ISTA 125 ' + description + b' FCD' + command]})
                    after = fetch_status(endpoint, secret)
                    self.assertEqual(before['chat'], after['chat'])
                    self.assertEqual(before['search'], after['search'])
                    self.assertNotIn('SecurityPrivateToken', json.dumps(after))
                    chat = b'BMSG ' + active.sid + b' SecurityRecovery'
                    active.send(chat)
                    for peer in peers:
                        self.assertEqual(peer.line(), chat)
                    hub.stop()
                    for private in (b'SecurityPrivateToken', secret.encode()):
                        self.assertNotIn(private, hub.log)

    def test_ports_addresses_sources_and_lifetimes(self):
        for ipv6 in (False, True):
            with self.subTest(ipv6=ipv6):
                hub, peers, suffix, endpoint, secret = self.fixture(ipv6)
                active, passive, other = peers
                for port in (b'0', b'-1', b'65536', b'9223372036854775808'):
                    wire = b'ECTM ' + active.sid + b' ' + passive.sid + b' ADC/1.0 ' + port + b' SecurityPrivateToken'
                    expected = {} if port == b'9223372036854775808' else {active: [b'ISTA 100 Invalid\\sconnection\\srequest FCECTM']}
                    self.exchange(peers, active, wire, expected)
                for port in (b'00001', b'65535'):
                    wire = b'DCTM ' + active.sid + b' ' + passive.sid + b' ADC/1.0 ' + port + b' SecurityPrivateToken'
                    self.exchange(peers, active, wire, {passive: [wire]})
                unavailable = b'DCTM ' + active.sid + b' ZZZZ ADC/1.0 1515 SecurityPrivateToken'
                self.exchange(peers, active, unavailable, {active: [b'ISTA 140 CTM\\srecipient\\sunavailable FCDCTM']})
                self_request = unavailable.replace(b'ZZZZ', active.sid)
                self.exchange(peers, active, self_request, {active: [b'ISTA 100 Invalid\\sconnection\\srequest FCDCTM']})
                # IP declarations are bound to the kernel peer, even after login.
                foreign = b'2001:db8::1' if ipv6 else b'192.0.2.99'
                other.send(b'BINF ' + other.sid + b' I' + suffix + foreign)
                self.assertTrue(drain_until_eof(other.peer).startswith(b'ISTA 246 '))
                peers.remove(other)
                for peer in peers:
                    self.assertEqual(peer.line(), b'IQUI ' + other.sid)
                # A valid SID belonging to another user does not confer its permissions.
                passive.send(b'DCTM ' + active.sid + b' ' + active.sid + b' ADC/1.0 1515 SecurityPrivateToken')
                self.assertTrue(drain_until_eof(passive.peer).startswith(b'ISTA 240 '))
                self.assertEqual(active.line(), b'IQUI ' + passive.sid)
                snapshot = json.dumps(fetch_status(endpoint, secret))
                self.assertNotIn('SecurityPrivateToken', snapshot)
                hub.stop()
                self.assertNotIn(b'SecurityPrivateToken', hub.log)
                self.assertNotIn(foreign, hub.log)

    def test_ini_strict_switches(self):
        with tempfile.TemporaryDirectory(prefix='dc24h-transfer-config-') as folder:
            path = pathlib.Path(folder) / 'hub.ini'
            for key in ('ctm_enabled', 'rcm_enabled', 'guest_initiate', 'guest_receive'):
                for value in ('on', 'off'):
                    path.write_text('[transfers]\n' + key + '=' + value + '\n')
                    result = subprocess.run([BINARY, '--config', str(path), '--check-config'], capture_output=True)
                    self.assertEqual(result.returncode, 0, result.stderr)
                for value in ('true', '1', 'ON', ''):
                    path.write_text('[transfers]\n' + key + '=' + value + '\n')
                    result = subprocess.run([BINARY, '--config', str(path), '--check-config'], capture_output=True)
                    self.assertNotEqual(result.returncode, 0)
                path.write_text('[transfers]\n' + key + '=on\n' + key + '=off\n')
                self.assertNotEqual(subprocess.run([BINARY, '--config', str(path), '--check-config'], capture_output=True).returncode, 0)


if __name__ == '__main__':
    unittest.main()
