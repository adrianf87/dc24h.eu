"""Real ncdc SUP exchange with the C++ module through an isolated fixture.

Python owns loopback sockets (and test TLS); this is not the hub Network reactor.
No identity is approved and the client never reaches a normal logged-in state.
"""
import fcntl
import os
import pty
import select
import socket
import ssl
import struct
import subprocess
import sys
import tempfile
import termios
import time
import unittest

from tls_support import credentials

PROBE, NCDC = sys.argv[1:3]
del sys.argv[1:3]
SID_MODE = '--sid' in sys.argv
if SID_MODE:
    sys.argv.remove('--sid')


def frame(peer, pending):
    while b'\n' not in pending:
        chunk = peer.recv(4096)
        if not chunk:
            raise AssertionError('ncdc disconnected before completing the frame')
        pending += chunk
        if len(pending) > 16385:
            raise AssertionError('fixture frame bound exceeded')
    line, pending = pending.split(b'\n', 1)
    return line + b'\n', pending


def probe_reply(probe, lines):
    reply = bytearray()
    deadline = time.monotonic() + 5
    while reply.count(b'\n') < lines:
        remaining = deadline - time.monotonic()
        if remaining <= 0 or not select.select([probe.stdout], [], [], remaining)[0]:
            raise AssertionError('C++ fixture reply timed out')
        chunk = os.read(probe.stdout.fileno(), 1024)
        if not chunk or len(reply) + len(chunk) > 1024:
            raise AssertionError('C++ fixture reply missing or oversized')
        reply.extend(chunk)
    return bytes(reply)


class SupNcdcTest(unittest.TestCase):
    def exchange(self, encrypted):
        with tempfile.TemporaryDirectory(prefix='dc24h-sup-ncdc-') as folder, socket.socket() as listener:
            listener.bind(('127.0.0.1', 0))
            listener.listen(1)
            listener.settimeout(5)
            context = None
            if encrypted:
                cert, key = credentials(folder)
                context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
                context.minimum_version = ssl.TLSVersion.TLSv1_2
                context.load_cert_chain(cert, key)
            master, slave = pty.openpty()
            fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack('HHHH', 30, 120, 0, 0))
            client = subprocess.Popen([NCDC, '-c', folder + '/session', '-n'], stdin=slave,
                stdout=slave, stderr=slave, start_new_session=True,
                env={**os.environ, 'TERM': 'xterm', 'LC_ALL': 'C.UTF-8'})
            os.close(slave)
            probe = None
            try:
                screen = bytearray()
                deadline = time.monotonic() + 5
                while b'Welcome' not in screen:
                    self.assertIsNone(client.poll(), 'ncdc exited during startup')
                    self.assertLess(time.monotonic(), deadline, 'ncdc startup timed out')
                    if select.select([master], [], [], 0.1)[0]:
                        screen.extend(os.read(master, 65536))
                os.write(master, b'/set nick dc24hSupTest\r')
                scheme = 'adcs' if encrypted else 'adc'
                os.write(master, f'/open sup_fixture {scheme}://127.0.0.1:{listener.getsockname()[1]}/\r'.encode())
                raw, _ = listener.accept()
                raw.settimeout(5)
                with raw:
                    peer = context.wrap_socket(raw, server_side=True) if context else raw
                    with peer:
                        initial, pending = frame(peer, b'')
                        self.assertTrue(initial.startswith(b'HSUP '))
                        probe = subprocess.Popen([PROBE, '--ncdc-transcript'], stdin=subprocess.PIPE,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE, bufsize=0)
                        probe.stdin.write(initial)
                        if SID_MODE:
                            reply = probe_reply(probe, 1)
                            self.assertEqual(reply, b'ISUP ADBASE ADTIGR\n')
                            peer.sendall(reply)
                            probe.stdin.write(b'SUP_SENT\n')
                            sid_reply = probe_reply(probe, 1)
                            self.assertRegex(sid_reply, rb'\AISID [A-Z2-7]{4}\n\Z')
                            assigned = sid_reply[5:9]
                            self.assertNotEqual(assigned, b'AAAA')
                            peer.sendall(sid_reply + b'IINF CT32 NIdc24h-SID-fixture\n')
                        else:
                            reply = probe_reply(probe, 2)
                            self.assertEqual(reply, b'ISUP ADBASE ADTIGR\nISID ABCD\n')
                            assigned = b'ABCD'
                            peer.sendall(reply + b'IINF CT32 NIdc24h-SUP-fixture\n')
                        inf, pending = frame(peer, pending)
                        self.assertTrue(inf.startswith(b'BINF ' + assigned + b' '), 'ncdc did not accept assigned SID')
                        self.assertIsNone(client.poll())
                        probe.stdin.write(b'SID_SENT\n' if SID_MODE else b'SENT\n')
                        stdout, stderr = probe.communicate(input=inf, timeout=5)
                        self.assertEqual(probe.returncode, 0, stderr.decode(errors='replace'))
                        self.assertEqual(stdout, b'IDENTIFY SID_RELEASED\n' if SID_MODE else b'IDENTIFY TIGR 24\n')
                        self.assertEqual(stderr, b'')
            finally:
                if probe is not None:
                    if probe.poll() is None:
                        probe.kill()
                    probe.communicate(timeout=3)
                if client.poll() is None:
                    client.kill()
                client.wait(timeout=3)
                os.close(master)

    def test_plaintext_module_exchange(self):
        self.exchange(False)

    def test_tls_fixture_module_exchange(self):
        self.exchange(True)


if __name__ == '__main__':
    unittest.main()
