"""Guest login, presence, rejection and resource policy on the actual TCP/TLS hub."""
import base64
import ctypes
import pathlib
import socket
import tempfile
import time
import unittest

from network_process_tests import Hub
from tls_support import credentials, connect_tls


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

    def login(self, nick, seed, fragmented=False):
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
        self.assertEqual(len(listing), 3)
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
        self.assertTrue(replacement.line().startswith(b'ISTA 125 '))
        hub.stop()
        self.assertEqual(hub.log.count(b'event=login_ready'), 3)
        for private in (b'PrivateUnknown', b'PendingChat', pair(1)[0], pair(2)[0]):
            self.assertNotIn(private, hub.log)

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
                wire = b'BINF ' + slow.sid + b' DE' + b'x' * 2048 + b'\n'
                # This client keeps sending updates without reading its own public echoes.
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



if __name__ == '__main__':
    unittest.main()
