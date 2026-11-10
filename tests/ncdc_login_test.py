"""Real ncdc completes guest login/reconnect on the C++ TCP/OpenSSL reactor."""
import fcntl
import os
import pathlib
import pty
import re
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
from adc_login_process_tests import Peer
from tls_support import credentials, TLS_READY, connect_tls

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
            observer = Peer(self, connect_tls(self, hub, str(pathlib.Path(directory) / 'certificate.pem')) if ENCRYPTED else hub.connect())
            observer.login(b'ListObserver', 99)
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
                                    try:
                                        chunk = source.recv(65536)
                                        if not chunk:
                                            ended = True
                                            break
                                        (wire if source is client else replies).extend(chunk)
                                        (upstream if source is client else client).sendall(chunk)
                                    except (ConnectionResetError, BrokenPipeError):
                                        # Normal /reconnect or /quit may reset
                                        # a transport with TLS bytes in flight.
                                        ended = True
                                        break
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
                nickname = 'dc24hŻółw'.encode('utf-8')
                os.write(master, b'/set nick ' + nickname + b'\r')
                scheme = 'adcs' if ENCRYPTED else 'adc'
                os.write(master, f'/open dc24h_login {scheme}://127.0.0.1:{listener.getsockname()[1]}/\r'.encode())
                hub.wait_for(lambda: hub.log.count(b'event=login_ready') == 2)
                initial_join = observer.line()
                self.assertTrue(initial_join.startswith(b'BINF '))
                initial_sid = initial_join.split()[1]
                # ncdc refuses chat until its own INF completed the initial user
                # list. Its normal BMSG and the returned status prove client-side
                # NORMAL; this stage deliberately has no chat dispatch.
                wait_screen(b'2 users')
                os.write(master, b'dc24hLoginProbe\r')
                wait_screen(b'Command not implemented')
                wait_screen(b'2 users')
                status = fetch_status(api_path, token)
                self.assertGreaterEqual(status['network']['adc_frames'], 3)
                os.write(master, b'/hset description dc24hINFUpdate\r')
                live_update = observer.line()
                self.assertEqual(live_update.split()[:2], [b'BINF', initial_sid])
                # ncdc may also flush its changed HN count with description.
                self.assertIn(b'DEdc24hINFUpdate', live_update.split()[2:])
                self.assertTrue(set(live_update.split()[2:]) <= {b'DEdc24hINFUpdate', b'HN1'})
                # ncdc applies /nick on reconnect (it does not emit a live NI
                # update). Exercise the new name with the same PID/CID.
                renamed = 'dc24hŁoś'.encode('utf-8')
                os.write(master, b'/nick ' + renamed + b'\r')
                os.write(master, b'/reconnect\r')
                hub.wait_for(lambda: hub.log.count(b'event=login_ready') == 3)
                self.assertEqual(observer.line(), b'IQUI ' + initial_sid)
                replacement_join = observer.line()
                self.assertTrue(replacement_join.startswith(b'BINF '))
                replacement_sid = replacement_join.split()[1]
                self.assertNotEqual(initial_sid, replacement_sid)
                public_cid = lambda line: re.search(rb'(?:^| )ID([A-Z2-7]+)(?: |$)', line)[1]
                self.assertEqual(public_cid(initial_join), public_cid(replacement_join))
                screen.clear()
                wait_screen(renamed + b' @ ')
                screen.clear()
                os.write(master, b'dc24hRenamedProbe\r')
                wait_screen(b'Command not implemented')
                if ENCRYPTED:
                    self.assertTrue(wire.startswith(b'\x16\x03'))
                    self.assertNotIn(b'HSUP', wire)
                    self.assertNotIn(b'dc24hLoginProbe', wire)
                    self.assertNotIn(b'BINF', replies)
                else:
                    self.assertIn(b'BMSG ', wire)
                    self.assertIn(b'dc24hLoginProbe', wire)
                    self.assertIn(b'BINF ', replies)
                    self.assertIn(b' NI' + nickname, replies)
                    self.assertIn(b' NI' + renamed, replies)
                    self.assertNotIn(b' PD', replies)
                    self.assertEqual(replies.count(b'ISUP ADBASE ADTIGR\n'), 2)
                    self.assertEqual(replies.count(b' NIListObserver'), 2)
                    initial = [line for line in bytes(wire).splitlines() if line.startswith(b'BINF ') and b' PD' in line]
                    self.assertEqual(len(initial), 2)
                    self.assertNotEqual(initial[0].split()[1], initial[1].split()[1], 'Reconnect reused its ADC SID')
                    cids = [re.search(rb'(?:^| )ID([A-Z2-7]+)(?: |$)', line)[1] for line in initial]
                    self.assertEqual(cids[0], cids[1], 'Reconnect changed its public CID')
                os.write(master, b'/quit\r')
                deadline = time.monotonic() + 4
                while process.poll() is None and time.monotonic() < deadline:
                    if select.select([master], [], [], 0.1)[0]:
                        try:
                            os.read(master, 65536)
                        except OSError:
                            break
                self.assertEqual(process.wait(timeout=2), 0)
                self.assertEqual(observer.line(), b'IQUI ' + replacement_sid)
                observer.send(b'DSTA ' + observer.sid + b' ' + replacement_sid + b' 100 Departed')
                self.assertTrue(observer.line().startswith(b'ISTA 140 '))
                observer.peer.close()
                hub.stop()
                self.assertEqual(hub.log.count(b'event=login_ready'), 3)
                records = re.findall(rb'Connection id=(\d+) transport=(ADC|ADCS) event=([a-z_]+)', hub.log)
                accepted = [ident for ident, _, event in records if event == b'accepted']
                self.assertEqual(len(accepted), 3)
                self.assertLess(int(accepted[1]), int(accepted[2]), 'Reconnect reused a local session identity')
                for ident in accepted:
                    events = [event for current, _, event in records if current == ident]
                    self.assertEqual(events.count(b'login_ready'), 1)
                    self.assertEqual(events.count(b'tls_ready'), 1 if ENCRYPTED else 0)
                    self.assertEqual(len(events), 4 if ENCRYPTED else 3, 'Session cleanup missing or duplicated')
                self.assertEqual(counters(self, hub)[2], 0)
                self.assertNotIn(nickname, hub.log)
                self.assertNotIn(renamed, hub.log)
                self.assertNotIn(b'dc24hLoginProbe', hub.log)
                self.assertNotIn(b'dc24hINFUpdate', hub.log)
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
