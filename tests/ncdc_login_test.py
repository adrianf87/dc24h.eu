"""Real ncdc completes guest login/reconnect on the C++ TCP/OpenSSL reactor."""
import fcntl
import os
import pathlib
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
from api_support import enable_api, fetch_status
from adc_process_tests import counters
from tls_support import credentials, TLS_READY

NCDC = sys.argv.pop(1)
ENCRYPTED = '--tls' in sys.argv
if ENCRYPTED:
    sys.argv.remove('--tls')


class NcdcLoginTest(unittest.TestCase):
    def test_guest_normal_and_reconnect(self):
        with tempfile.TemporaryDirectory(prefix='dc24h-ncdc-login-') as directory:
            ini = pathlib.Path(directory) / 'hub.ini'
            network = '[network]\nlisten=127.0.0.1:0\n'
            if ENCRYPTED:
                credentials(directory)
                network = ('[network]\nadcs_listen=127.0.0.1:0\n[tls]\n'
                           'certificate=certificate.pem\nprivate_key=private.pem\n')
            ini.write_text(network + '[security]\nmax_connections_per_ip=8\n')
            api_path, token = enable_api(ini, directory)
            options = {'ready_pattern': TLS_READY} if ENCRYPTED else {}
            hub = Hub(self, '--config', str(ini), '--log-connections', 'on', **options)
            wire, replies, errors = bytearray(), bytearray(), []
            finished = threading.Event()
            listener = socket.socket()
            self.addCleanup(listener.close)
            listener.bind(('127.0.0.1', 0))
            listener.listen(4)
            listener.settimeout(0.2)

            def forward():
                try:
                    while not finished.is_set():
                        try:
                            client, _ = listener.accept()
                        except socket.timeout:
                            continue
                        with client, socket.create_connection(hub.endpoints[0], timeout=3) as upstream:
                            while not finished.is_set():
                                ended = False
                                for source in select.select([client, upstream], [], [], 0.05)[0]:
                                    chunk = source.recv(65536)
                                    if not chunk:
                                        ended = True
                                        break
                                    (wire if source is client else replies).extend(chunk)
                                    (upstream if source is client else client).sendall(chunk)
                                if ended:
                                    break
                except Exception as error:
                    if not finished.is_set():
                        errors.append(error)

            thread = threading.Thread(target=forward, daemon=True)
            thread.start()
            master, slave = pty.openpty()
            fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack('HHHH', 30, 140, 0, 0))
            process = subprocess.Popen([NCDC, '-c', directory + '/session', '-n'],
                stdin=slave, stdout=slave, stderr=slave, start_new_session=True,
                env={**os.environ, 'TERM': 'xterm', 'LC_ALL': 'C.UTF-8'})
            os.close(slave)
            screen = bytearray()

            def wait_screen(expected, timeout=6):
                deadline = time.monotonic() + timeout
                while expected not in screen:
                    self.assertIsNone(process.poll(), repr(screen[-3000:]))
                    self.assertLess(time.monotonic(), deadline, repr(screen[-5000:]))
                    if select.select([master], [], [], 0.1)[0]:
                        screen.extend(os.read(master, 65536))

            try:
                wait_screen(b'Welcome')
                os.write(master, b'/set nick dc24hLoginTest\r')
                scheme = 'adcs' if ENCRYPTED else 'adc'
                os.write(master, f'/open dc24h_login {scheme}://127.0.0.1:{listener.getsockname()[1]}/\r'.encode())
                hub.wait_for(lambda: hub.log.count(b'event=login_ready') == 1)
                # ncdc refuses chat until its own INF completed the initial user
                # list. Its normal BMSG and the returned status prove client-side
                # NORMAL; this stage deliberately has no chat dispatch.
                time.sleep(0.1)
                os.write(master, b'dc24hLoginProbe\r')
                wait_screen(b'Command not implemented')
                status = fetch_status(api_path, token)
                self.assertGreaterEqual(status['network']['adc_frames'], 3)
                os.write(master, b'/reconnect\r')
                hub.wait_for(lambda: hub.log.count(b'event=login_ready') == 2)
                if ENCRYPTED:
                    self.assertTrue(wire.startswith(b'\x16\x03'))
                    self.assertNotIn(b'HSUP', wire)
                    self.assertNotIn(b'dc24hLoginProbe', wire)
                    self.assertNotIn(b'BINF', replies)
                else:
                    self.assertIn(b'BMSG ', wire)
                    self.assertIn(b'dc24hLoginProbe', wire)
                    self.assertIn(b'BINF ', replies)
                    self.assertNotIn(b' PD', replies)
                    self.assertEqual(replies.count(b'ISUP ADBASE ADTIGR\n'), 2)
                os.write(master, b'/quit\r')
                deadline = time.monotonic() + 4
                while process.poll() is None and time.monotonic() < deadline:
                    if select.select([master], [], [], 0.1)[0]:
                        try:
                            os.read(master, 65536)
                        except OSError:
                            break
                self.assertEqual(process.wait(timeout=2), 0)
                hub.stop()
                self.assertEqual(hub.log.count(b'event=login_ready'), 2)
                self.assertEqual(counters(self, hub)[2], 0)
                self.assertNotIn(b'dc24hLoginTest', hub.log)
                self.assertNotIn(b'dc24hLoginProbe', hub.log)
                self.assertNotIn(b'event=login_rejected', hub.log)
                print(f'ncdc completed guest NORMAL and reconnect via {scheme.upper()} on the C++ hub')
            finally:
                finished.set()
                if process.poll() is None:
                    process.kill()
                process.wait()
                os.close(master)
                thread.join(timeout=2)
                self.assertFalse(thread.is_alive())
                self.assertEqual(errors, [])


if __name__ == '__main__':
    unittest.main()
