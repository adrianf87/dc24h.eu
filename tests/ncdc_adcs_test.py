"""Real ncdc negotiates TLS with the C++ hub through a byte-transparent relay."""
import fcntl
import os
import pty
import select
import socket
import struct
import subprocess
import sys
import tempfile
import termios
import threading
import time
import unittest

from network_process_tests import Hub
from adc_process_tests import counters
from tls_support import credentials, TLS_READY

NCDC = sys.argv.pop(1)


class NcdcAdcsTest(unittest.TestCase):
    def test_encrypted_initial_sup(self):
        with tempfile.TemporaryDirectory(prefix='dc24h-ncdc-adcs-') as directory:
            cert, key = credentials(directory)
            hub = Hub(self, '--adcs-listen', '127.0.0.1:0', '--tls-cert', cert,
                      '--tls-key', key, '--idle-timeout', '2', ready_pattern=TLS_READY)
            wire = bytearray()
            errors = []
            finished = threading.Event()
            relay = socket.socket()
            self.addCleanup(relay.close)
            relay.bind(('127.0.0.1', 0))
            relay.listen(1)
            relay.settimeout(8)

            def forward():
                try:
                    client, _ = relay.accept()
                    with client, socket.create_connection(hub.endpoints[0], timeout=3) as upstream:
                        deadline = time.monotonic() + 8
                        while time.monotonic() < deadline:
                            for source in select.select([client, upstream], [], [], 0.1)[0]:
                                chunk = source.recv(65536)
                                if not chunk:
                                    return
                                if source is client:
                                    wire.extend(chunk)
                                (upstream if source is client else client).sendall(chunk)
                        raise TimeoutError('relay did not observe server idle close')
                except Exception as error:
                    errors.append(error)
                finally:
                    finished.set()

            thread = threading.Thread(target=forward, daemon=True)
            thread.start()
            master, slave = pty.openpty()
            fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack('HHHH', 30, 120, 0, 0))
            process = subprocess.Popen([NCDC, '-c', directory + '/session', '-n'],
                stdin=slave, stdout=slave, stderr=slave, start_new_session=True,
                env={**os.environ, 'TERM': 'xterm', 'LC_ALL': 'C.UTF-8'})
            os.close(slave)
            screen = bytearray()
            try:
                deadline = time.monotonic() + 5
                while b'Welcome' not in screen:
                    self.assertIsNone(process.poll())
                    self.assertLess(time.monotonic(), deadline, repr(screen[-2000:]))
                    if select.select([master], [], [], 0.1)[0]:
                        screen.extend(os.read(master, 65536))
                os.write(master, b'/set nick dc24hTlsTest\r')
                os.write(master, f'/open dc24h_tls adcs://127.0.0.1:{relay.getsockname()[1]}/\r'.encode())
                deadline = time.monotonic() + 10
                while not finished.is_set():
                    self.assertIsNone(process.poll())
                    self.assertLess(time.monotonic(), deadline, repr(screen[-4000:]))
                    if select.select([master], [], [], 0.1)[0]:
                        screen.extend(os.read(master, 65536))
                thread.join(timeout=1)
                self.assertFalse(thread.is_alive())
                self.assertEqual(errors, [])
                self.assertTrue(wire.startswith(b'\x16\x03'), repr(wire[:10]))
                self.assertNotIn(b'HSUP', wire, 'ADC command leaked outside TLS')
                hub.stop()
                self.assertIn(b'ADCS stopped; handshakes=1 incomplete=0', hub.log)
                self.assertEqual(counters(self, hub), (1, 0, 0, 0))
                os.write(master, b'/quit\r')
                deadline = time.monotonic() + 3
                while process.poll() is None and time.monotonic() < deadline:
                    if select.select([master], [], [], 0.1)[0]:
                        try:
                            os.read(master, 65536)
                        except OSError:
                            break
                self.assertEqual(process.wait(timeout=2), 0)
                print('ncdc negotiated TLS with C++ ADCS and delivered HSUP; full ADC login remains pending')
            finally:
                if process.poll() is None:
                    process.kill()
                process.wait()
                os.close(master)
                relay.close()
                thread.join(timeout=1)


if __name__ == '__main__':
    unittest.main()
