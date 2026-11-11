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
MSG_CODEC = sys.argv.pop(1)
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
            welcome=b'IMSG Welcome\\sto\\sdc24h.eu.\\sConfigured\\sncdc\\schat.'
            ini.write_text(network + '[chat]\nwelcome_message=Welcome to dc24h.eu. Configured ncdc chat.\n'
                           '[users]\nmax_sessions=3\nmax_users=2\n'
                           '[security]\nmax_connections_per_ip=8\n')
            api_path, token = enable_api(ini, directory)
            options = {'ready_pattern': TLS_READY} if ENCRYPTED else {}
            hub = Hub(self, '--config', str(ini), '--log-connections', 'on', '--log-chat', 'on', **options)
            observer = Peer(self, connect_tls(self, hub, str(pathlib.Path(directory) / 'certificate.pem')) if ENCRYPTED else hub.connect())
            observer.login(b'ListObserver', 99, welcome=welcome)
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
                # list. The observer and own echo now prove client-side NORMAL
                # and public delivery.
                wait_screen(b'2 users')
                wait_screen(b'Welcome to dc24h.eu.')
                wait_screen(b'Configured ncdc chat.')
                observer.send(b'BMSG '+observer.sid+b' dc24hIncomingChat')
                self.assertEqual(observer.line(), b'BMSG '+observer.sid+b' dc24hIncomingChat')
                wait_screen(b'dc24hIncomingChat')
                os.write(master, b'dc24hLoginProbe\r')
                self.assertEqual(observer.line(), b'BMSG '+initial_sid+b' dc24hLoginProbe')
                wait_screen(b'dc24hLoginProbe')
                public_text='dc24hPublicŻółw 😀 path\\folder'.encode()
                os.write(master, public_text+b'\r')
                self.assertEqual(observer.line(), b'BMSG '+initial_sid+b' '+r'dc24hPublicŻółw\s😀\spath\\folder'.encode())
                wait_screen('dc24hPublicŻółw'.encode())
                validated = b'BMSG '+observer.sid+b' '+r'dc24hValidatedŻółw\s😀\nsecond\\line'.encode()+b' ME0'
                observer.send(validated); self.assertEqual(observer.line(),validated)
                wait_screen('dc24hValidatedŻółw'.encode())
                for command in (b'/me dc24hActionProbe likes cats', b'/msg ListObserver dc24hPrivateProbe'):
                    before_frames = fetch_status(api_path, token)['network']['adc_frames']
                    os.write(master, command+b'\r')
                    deadline = time.monotonic()+3
                    while True:
                        probe = fetch_status(api_path, token)
                        if probe['network']['adc_frames'] > before_frames:
                            break
                        self.assertLess(time.monotonic(), deadline, 'ncdc did not send MSG')
                        time.sleep(0.01)
                    self.assertEqual(probe['users']['online'], 2)
                    if command.startswith(b'/me '):
                        self.assertEqual(observer.line(), b'BMSG '+initial_sid+b' dc24hActionProbe\\slikes\\scats ME1')
                    else:
                        expected = b'EMSG '+initial_sid+b' '+observer.sid+b' dc24hPrivateProbe PM'+initial_sid
                        self.assertEqual(observer.line(), expected)
                        wait_screen(b'dc24hPrivateProbe')
                private_reply = b'EMSG '+observer.sid+b' '+initial_sid+b' dc24hPrivateReply PM'+observer.sid
                observer.send(private_reply);self.assertEqual(observer.line(), private_reply)
                wait_screen(b'dc24hPrivateReply')
                # PM routes the reply tab; explicit ME0 remains a plain message,
                # while ME01 is canonicalized to an action with the same origin.
                for flag, canonical, marker in ((b'ME-0', b'ME0', b'dc24hPrivatePlainFlag'),
                                                (b'ME01', b'ME1', b'dc24hPrivateActionFlag')):
                    reply = b'EMSG '+observer.sid+b' '+initial_sid+b' '+marker
                    observer.send(reply+b' '+flag+b' PM'+observer.sid)
                    self.assertEqual(observer.line(), reply+b' PM'+observer.sid+b' '+canonical)
                    wait_screen(marker)
                os.write(master, b'/me dc24hPrivateAction\r')
                self.assertEqual(observer.line(), b'EMSG '+initial_sid+b' '+observer.sid+b' dc24hPrivateAction PM'+initial_sid+b' ME1')
                os.write(master, b'/open dc24h_login\r')
                wait_screen(b'2 users')
                status = fetch_status(api_path, token)
                self.assertGreaterEqual(status['network']['adc_frames'], 3)
                self.assertEqual(status['users'], {'online': 2, 'peak_online': 2, 'successful_logins': 2})
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
                deadline = time.monotonic() + 3
                while True:
                    stats = fetch_status(api_path, token)
                    if stats['users'] == {'online': 2, 'peak_online': 2, 'successful_logins': 3}:
                        break
                    self.assertLess(time.monotonic(), deadline, stats)
                    time.sleep(0.01)
                screen.clear()
                wait_screen(renamed + b' @ ')
                screen.clear()
                os.write(master, b'dc24hRenamedProbe\r')
                self.assertEqual(observer.line(), b'BMSG '+replacement_sid+b' dc24hRenamedProbe')
                wait_screen(b'dc24hRenamedProbe')
                if ENCRYPTED:
                    self.assertTrue(wire.startswith(b'\x16\x03'))
                    self.assertNotIn(b'HSUP', wire)
                    self.assertNotIn(b'dc24hLoginProbe', wire)
                    self.assertNotIn(b'BINF', replies)
                else:
                    self.assertIn(b'BMSG ', wire)
                    msg_frames = [line for line in bytes(wire).splitlines() if line[:4] in (b'BMSG', b'EMSG', b'DMSG')]
                    self.assertTrue(any(b' ME1' in line for line in msg_frames))
                    self.assertTrue(any(line.startswith(b'EMSG ') and b' PM' in line for line in msg_frames))
                    captured = b'\n'.join(msg_frames)+b'\n'
                    checked = subprocess.run([MSG_CODEC, '--roundtrip'], input=captured, capture_output=True, timeout=5)
                    self.assertEqual(checked.returncode, 0, 'MSG codec rejected real ncdc frames')
                    self.assertEqual(checked.stdout, captured, 'MSG codec changed real ncdc frames')
                    self.assertIn(b'dc24hLoginProbe', wire)
                    self.assertIn(b'BINF ', replies)
                    self.assertIn(welcome+b'\n', replies)
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
                deadline = time.monotonic() + 3
                while True:
                    stats = fetch_status(api_path, token)
                    if stats['users']['online'] == 1 and stats['network']['closed_connections'] == 2:
                        break
                    self.assertLess(time.monotonic(), deadline, stats)
                    time.sleep(0.01)
                observer.peer.close()
                self.assertGreater(stats['chat']['public_dispatches'], 0)
                self.assertGreater(stats['chat']['private_dispatches'], 0)
                hub.stop()
                self.assertIn(b'Chat event=public_dispatch', hub.log)
                self.assertIn(b'Chat event=private_dispatch', hub.log)
                self.assertIn(b'Users stopped; online=0 peak_online=2 successful_logins=3', hub.log)
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
                self.assertNotIn(b'dc24hActionProbe', hub.log)
                self.assertNotIn(b'dc24hPrivateProbe', hub.log)
                self.assertNotIn(b'dc24hPrivateReply', hub.log)
                self.assertNotIn(b'dc24hPrivateAction', hub.log)
                self.assertNotIn(b'dc24hIncomingChat', hub.log)
                self.assertNotIn(b'dc24hPublic', hub.log)
                self.assertNotIn(b'dc24hValidated', hub.log)
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
