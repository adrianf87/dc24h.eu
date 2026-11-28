"""Guest login, presence, rejection and resource policy on the actual TCP/TLS hub."""
import base64
import ctypes
import pathlib
import sys
import socket
import tempfile
import time
import unittest

from network_process_tests import Hub
from tls_support import credentials, connect_tls

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / 'scripts'))
from stage4_perf_probe import process_cpu_seconds, rss_kib

HUB_WELCOME = b'IMSG Welcome\\sto\\sdc24h.eu.\\sGuest\\saccess\\sis\\senabled.'


def pair(seed):
    pid = bytes((i + seed) % 256 for i in range(24))
    gcrypt = ctypes.CDLL('libgcrypt.so.20')
    gcrypt.gcry_md_hash_buffer.argtypes = [ctypes.c_int, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t]
    digest = ctypes.create_string_buffer(24)
    gcrypt.gcry_md_hash_buffer(306, digest, pid, 24)  # TIGER1 / NESSIE ordering
    encode = lambda value: base64.b32encode(value).rstrip(b'=')
    return encode(pid), encode(digest.raw)


class Peer:
    def __init__(self, case, peer):
        self.case, self.peer, self.pending = case, peer, b''

    def line(self):
        while b'\n' not in self.pending:
            chunk = self.peer.recv(4096)
            self.case.assertTrue(chunk, 'Hub closed before a complete reply')
            self.pending += chunk
            self.case.assertLessEqual(len(self.pending), 256 * 1024)
        line, self.pending = self.pending.split(b'\n', 1)
        return line

    def send(self, wire):
        self.peer.sendall(wire + b'\n')

    def handshake(self, fragmented=False):
        wire = b'HSUP ADBASE ADTIGR\n'
        if fragmented:
            for byte in wire:
                self.peer.sendall(bytes([byte]))
        else:
            self.peer.sendall(wire)
        self.case.assertEqual(self.line(), b'ISUP ADBASE ADTIGR')
        line = self.line()
        self.case.assertRegex(line, rb'^ISID [A-Z2-7]{4}$')
        self.sid = line[5:]
        return self.sid

    def identity(self, nick, seed):
        pid, cid = pair(seed)
        return b'BINF ' + self.sid + b' NI' + nick + b' ID' + cid + b' PD' + pid + b' I40.0.0.0 DEHello\\sworld XXPrivateUnknown'

    def login(self, nick, seed, fragmented=False, welcome=HUB_WELCOME):
        self.handshake(fragmented)
        wire = self.identity(nick, seed) + b'\n'
        if fragmented:
            for byte in wire:
                self.peer.sendall(bytes([byte]))
        else:
            self.peer.sendall(wire)
        result = [self.line()]
        self.case.assertTrue(result[0].startswith(b'IINF '), result)
        while not result[-1].startswith(b'BINF ' + self.sid + b' '):
            result.append(self.line())
            self.case.assertLess(len(result), 100)
        if welcome is not None:
            result.append(self.line())
            self.case.assertEqual(result[-1], welcome)
        for line in result:
            self.case.assertNotIn(b' PD', line)
            self.case.assertNotIn(b' XX', line)
        return result


class LoginProcessTests(unittest.TestCase):
    def hub(self, **settings):
        directory = tempfile.TemporaryDirectory(prefix='dc24h-login-')
        self.addCleanup(directory.cleanup)
        cert, key = credentials(directory.name)
        ini = pathlib.Path(directory.name) / 'hub.ini'
        security = {'max_connections_per_ip': 32, 'max_accepts_per_minute': 256,
                    'max_frames_per_second': 100000, **settings}
        ini.write_text('[network]\nlisten=127.0.0.1:0\nadcs_listen=127.0.0.1:0\n'
                       '[tls]\ncertificate=certificate.pem\nprivate_key=private.pem\n'
                       '[security]\n' + ''.join(f'{key}={value}\n' for key, value in security.items()))
        return Hub(self, '--config', str(ini), '--log-connections', 'on'), cert

    def peer(self, hub, cert, encrypted=False):
        return Peer(self, connect_tls(self, hub, cert) if encrypted else hub.connect())

    def test_mixed_transport_login_presence_updates_and_reconnect(self):
        hub, cert = self.hub()
        first = self.peer(hub, cert)
        first.login('Żółw'.encode(), 1, True)
        second = self.peer(hub, cert, True)
        listing = second.login(b'Second', 2, True)
        self.assertNotEqual(first.sid, second.sid)
        self.assertEqual(len(listing), 4)
        self.assertTrue(listing[1].startswith(b'BINF ' + first.sid + b' '))
        self.assertTrue(first.line().startswith(b'BINF ' + second.sid + b' '))
        first.send(b'BINF ' + first.sid + b' NIrenamed DE XX')
        update = first.line()
        self.assertEqual(update, second.line())
        self.assertIn(b' NIrenamed', update)
        self.assertIn(b' DE', update)
        self.assertNotIn(b'XX', update)
        first.peer.close()
        self.assertEqual(second.line(), b'IQUI ' + first.sid)
        replacement = self.peer(hub, cert)
        replacement.login(b'renamed', 1)
        self.assertTrue(second.line().startswith(b'BINF ' + replacement.sid + b' '))
        replacement.send(b'BMSG ' + replacement.sid + b' PendingChat')
        self.assertEqual(replacement.line(), b'BMSG '+replacement.sid+b' PendingChat')
        self.assertEqual(second.line(), b'BMSG '+replacement.sid+b' PendingChat')
        hub.stop()
        self.assertEqual(hub.log.count(b'event=login_ready'), 3)
        for private in (b'PrivateUnknown', b'PendingChat', pair(1)[0], pair(2)[0]):
            self.assertNotIn(private, hub.log)

    def test_dcpp_shaped_guest_wire_does_not_grant_operator_rights(self):
        """Synthetic DC++-style INF fields over ADC and ADCS, not a DC++ binary certification."""
        for encrypted in (False, True):
            with self.subTest(encrypted=encrypted):
                hub, cert = self.hub()
                observer = self.peer(hub, cert, not encrypted)
                observer.login(b'Observer', 31)
                client = self.peer(hub, cert, encrypted)
                client.handshake()
                client.send(client.identity(b'DCPlusPlus', 32).replace(
                    b' XXPrivateUnknown', b' APDC++ VE0.9.20 LOpl_PL CT1 SUADCS,TCP4 XXPrivateUnknown'))
                self.assertTrue(client.line().startswith(b'IINF CT32'))
                self.assertTrue(client.line().startswith(b'BINF ' + observer.sid))
                own = client.line()
                self.assertTrue(own.startswith(b'BINF ' + client.sid + b' '), own)
                for field in (b' APDC++', b' VE0.9.20', b' LOpl_PL', b' CT1', b' SUADCS,TCP4'):
                    self.assertIn(field, own)
                self.assertNotIn(b' PD', own)
                self.assertNotIn(b' XX', own)
                self.assertEqual(client.line(), HUB_WELCOME)
                self.assertEqual(observer.line(), own)
                # Client application, hub transport and SU are public metadata;
                # none authorize an operator privilege bit in a later INF.
                client.send(b'BINF ' + client.sid + b' CT4 APDC++')
                self.assertTrue(client.line().startswith(b'ISTA 243 '))
                self.assertTrue(client.line().startswith(b'IQUI ' + client.sid))
                self.assertEqual(observer.line(), b'IQUI ' + client.sid)
                hub.stop()

    def test_failures_do_not_announce_or_steal_identity(self):
        hub, cert = self.hub()
        owner = self.peer(hub, cert)
        owner.login(b'Owner', 1)
        for encrypted in (False, True):
            for nick, seed, status in ((b'Owner', 2, b'222'), (b'Other', 1, b'224')):
                peer = self.peer(hub, cert, encrypted)
                peer.handshake()
                peer.send(peer.identity(nick, seed))
                self.assertTrue(peer.line().startswith(b'ISTA ' + status + b' '))
                self.assertTrue(peer.line().startswith(b'IQUI '+peer.sid+b' '))
                self.assertEqual(peer.peer.recv(1), b'')
            bad = self.peer(hub, cert, encrypted)
            bad.handshake()
            bad.send(bad.identity(b'Bad', 3).replace(pair(3)[1], pair(4)[1]))
            self.assertTrue(bad.line().startswith(b'ISTA 227 '))
            self.assertTrue(bad.line().startswith(b'IQUI '+bad.sid+b' '))
            self.assertEqual(bad.peer.recv(1), b'')
        owner.send(b'BINF ' + owner.sid + b' DEStillHere')
        self.assertIn(b'DEStillHere', owner.line())  # No failed guest joined before this echo.
        attacker = self.peer(hub, cert)
        attacker.handshake()
        attacker.send(b'BINF AAAA NIspoof')
        self.assertTrue(attacker.line().startswith(b'ISTA 243 '))
        early = self.peer(hub, cert)
        early.send(b'BMSG AAAB premature')
        self.assertTrue(early.line().startswith(b'ISTA 244 '))
        hub.stop()
        self.assertEqual(hub.log.count(b'event=login_ready'), 1)

    def test_beta07_negotiation_refuses_duplicate_sup_and_preauth_frames(self):
        """Full TCP/OpenSSL reactor rejects invalid state without an identity leak.

        Existing ncdc cases prove the independent client's successful SUP,
        SID and INF. These adversarial peers share the same transport fixture;
        none may become NORMAL or change the next valid Unicode login.
        """
        for encrypted in (False, True):
            with self.subTest(encrypted=encrypted):
                hub, cert = self.hub()
                for command in (b'HSUP ADBASE ADTIGR', b'BMSG PLACEHOLDER preauth'):
                    bad = self.peer(hub, cert, encrypted)
                    bad.handshake()
                    wire = command.replace(b'PLACEHOLDER', bad.sid)
                    bad.send(wire)
                    self.assertTrue(bad.line().startswith(b'ISTA 244 '), wire)
                    self.assertTrue(bad.line().startswith(b'IQUI ' + bad.sid + b' '),
                        "protocol quit can include a non-sensitive ADC status message")
                    self.assertEqual(bad.peer.recv(1), b'')
                no_hash = self.peer(hub, cert, encrypted)
                no_hash.send(b'HSUP ADBASE')
                self.assertTrue(no_hash.line().startswith(b'ISTA 247 '))
                self.assertEqual(no_hash.peer.recv(1), b'')
                healthy = self.peer(hub, cert, encrypted)
                healthy.login('ŻółwBezpieczny'.encode('utf-8'), 172)
                hub.stop()
                self.assertEqual(hub.log.count(b'event=login_ready'), 1)
                self.assertNotIn('ŻółwBezpieczny'.encode('utf-8'), hub.log)
                self.assertNotIn(pair(172)[0], hub.log)

    def test_absolute_login_deadline_ignores_keepalives(self):
        hub, cert = self.hub(login_timeout=1, output_timeout=1)
        for identify in (False, True):
            peer = self.peer(hub, cert)
            if identify:
                peer.handshake()
            start = time.monotonic()
            for _ in range(4):
                peer.peer.sendall(b'\n')
                time.sleep(0.2)
            self.assertTrue(peer.line().startswith(b'ISTA 240 '))
            if identify:
                self.assertTrue(peer.line().startswith(b'IQUI '+peer.sid+b' '))
            self.assertEqual(peer.peer.recv(1), b'')
            self.assertLess(time.monotonic() - start, 2.5)
        healthy = self.peer(hub, cert, True)
        healthy.login(b'Healthy', 5)
        hub.stop()
        self.assertEqual(hub.log.count(b'event=login_timeout'), 2)

    def test_slow_reader_cannot_starve_other_login_or_shutdown(self):
        for encrypted in (False, True):
            with self.subTest(encrypted=encrypted):
                hub, cert = self.hub(output_timeout=1)
                slow = self.peer(hub, cert, encrypted)
                slow.peer.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 1024)
                slow.login(b'Slow', 7)
                slow.peer.settimeout(0.5)
                wire = b'BSTA ' + slow.sid + b' 199 ' + b'x' * 2048 + b'\n'
                # STA, not INF: presence updates are capped. This client keeps sending
                # without reading its own public echoes.
                # A closed slow reader may reset its sending side; that is expected.
                try:
                    slow.peer.sendall(wire * 2000)
                except (OSError, socket.timeout):
                    pass
                hub.wait_for(lambda: b'event=output_limit' in hub.log or b'event=output_timeout' in hub.log)
                healthy = self.peer(hub, cert)
                healthy.login(b'Healthy', 8)
                hub.stop()
                self.assertIn(b'event=login_ready', hub.log)

    def test_beta06_slow_reader_releases_budget_and_healthy_peers_remain_fair(self):
        for encrypted in (False, True):
            with self.subTest(encrypted=encrypted):
                hub, cert = self.hub(output_timeout=1)
                pid = hub.process.pid
                cpu_before = process_cpu_seconds(pid)
                slow = self.peer(hub, cert, encrypted)
                slow.peer.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 1024)
                slow.login(b'BudgetSlow', 160)
                slow.peer.settimeout(0.25)
                frame = b'BSTA ' + slow.sid + b' 199 ' + b'Z' * 2048 + b'\n'
                try:
                    slow.peer.sendall(frame * 2000)
                except (OSError, socket.timeout):
                    pass
                hub.wait_for(lambda: b'event=output_limit' in hub.log or b'event=output_timeout' in hub.log)
                fast = self.peer(hub, cert, encrypted)
                fast.login(b'BudgetFast', 161)
                second = self.peer(hub, cert, encrypted)
                second.login(b'BudgetSecond', 162)
                self.assertTrue(fast.line().startswith(b'BINF ' + second.sid + b' '))
                begin = time.monotonic()
                for i in range(8):
                    wire = b'BSTA ' + fast.sid + b' 000 FairProbe' + str(i).encode()
                    fast.send(wire)
                    self.assertEqual(fast.line(), wire)
                    self.assertEqual(second.line(), wire)
                self.assertLess(time.monotonic() - begin, 5.0)
                memory = rss_kib(pid)
                cpu_after = process_cpu_seconds(pid)
                self.assertIsNotNone(memory)
                self.assertLess(memory, 256 * 1024, 'Synthetic 3-peer RSS safety limit in KiB')
                self.assertIsNotNone(cpu_before)
                self.assertIsNotNone(cpu_after)
                self.assertLessEqual(cpu_after - cpu_before, 12.0, 'Synthetic overload CPU budget in seconds')
                hub.stop()



if __name__ == '__main__':
    unittest.main()
