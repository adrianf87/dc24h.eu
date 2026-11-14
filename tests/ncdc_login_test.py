"""Real ncdc completes guest login/reconnect on the C++ TCP/OpenSSL reactor."""
from contextlib import closing
import fcntl
import os
import pathlib
import pty
import re
import select
import socket
import ssl
import sqlite3
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
from adc_login_process_tests import Peer, pair
from tls_support import credentials, TLS_READY, connect_tls

NCDC = sys.argv.pop(1)
MSG_CODEC = sys.argv.pop(1)
ENCRYPTED = '--tls' in sys.argv
if ENCRYPTED:
    sys.argv.remove('--tls')
PEER_TLS = '--peer-tls' in sys.argv
if PEER_TLS:
    sys.argv.remove('--peer-tls')
PEER_PROTOCOL = b'ADCS/1.0' if PEER_TLS else b'ADC/1.0'
TERMINAL_PRESSURE = '--terminal-pressure' in sys.argv
if TERMINAL_PRESSURE:
    sys.argv.remove('--terminal-pressure')
RCM_TEST = '--rcm' in sys.argv
if RCM_TEST:
    sys.argv.remove('--rcm')
CLIENT_CAPABILITIES = '--client-capabilities' in sys.argv
if CLIENT_CAPABILITIES:
    sys.argv.remove('--client-capabilities')
RCM_TEST = RCM_TEST or CLIENT_CAPABILITIES
TRANSFER_TOKENS = '--transfer-tokens' in sys.argv
if TRANSFER_TOKENS:
    sys.argv.remove('--transfer-tokens')
RCM_TEST = RCM_TEST or TRANSFER_TOKENS
TRANSFER_ERRORS = '--transfer-errors' in sys.argv
if TRANSFER_ERRORS:
    sys.argv.remove('--transfer-errors')
TRANSFER_LIMITS = '--transfer-limits' in sys.argv
if TRANSFER_LIMITS:
    sys.argv.remove('--transfer-limits')
CTM_TEST = '--ctm' in sys.argv
if CTM_TEST:
    sys.argv.remove('--ctm')
TRANSFER_CORE = '--transfer-core' in sys.argv
if TRANSFER_CORE:
    sys.argv.remove('--transfer-core')
CONNECTION_SECURITY_ACTIVE = '--connection-security-active' in sys.argv
if CONNECTION_SECURITY_ACTIVE:
    sys.argv.remove('--connection-security-active')
CONNECTION_SECURITY = '--connection-security' in sys.argv
if CONNECTION_SECURITY:
    sys.argv.remove('--connection-security')
CONNECTION_SECURITY = CONNECTION_SECURITY or CONNECTION_SECURITY_ACTIVE
SEARCH_SECURITY = '--search-security' in sys.argv
if SEARCH_SECURITY:
    sys.argv.remove('--search-security')
TTH_SEARCH = '--tth' in sys.argv
if TTH_SEARCH:
    sys.argv.remove('--tth')
ACTIVE_PASSIVE = '--active-passive' in sys.argv
if ACTIVE_PASSIVE:
    sys.argv.remove('--active-passive')
FEATURE_SEARCH = '--feature' in sys.argv
if FEATURE_SEARCH:
    sys.argv.remove('--feature')
FEATURE_SEARCH = FEATURE_SEARCH or ACTIVE_PASSIVE
TEST_TTH = b'LWPNACQDBZRYXW3VHJVCJ64QBZNGHOHHHZWCLNQ'


class NcdcLoginTest(unittest.TestCase):
    def test_guest_normal_and_reconnect(self):
        with tempfile.TemporaryDirectory(prefix='dc24h-ncdc-login-') as directory:
            ini = pathlib.Path(directory) / 'hub.ini'
            network = '[network]\nlisten=127.0.0.1:0\n'
            if ENCRYPTED or PEER_TLS:
                credentials(directory)
            if ENCRYPTED:
                network = ('[network]\nadcs_listen=127.0.0.1:0\n[tls]\n'
                           'certificate=certificate.pem\nprivate_key=private.pem\n')
            welcome=b'IMSG Welcome\\sto\\sdc24h.eu.\\sConfigured\\sncdc\\schat.'
            ini.write_text(network + '[chat]\nwelcome_message=Welcome to dc24h.eu. Configured ncdc chat.\n'
                           '[users]\nmax_sessions=3\nmax_users=2\n'
                           '[security]\nmax_connections_per_ip=8\n')
            if SEARCH_SECURITY:
                ini.write_text(ini.read_text() + '[search]\nguest_queries=off\n')
            if CONNECTION_SECURITY:
                ini.write_text(ini.read_text() + '[transfers]\nguest_initiate=off\n')
            if TRANSFER_LIMITS:
                ini.write_text(ini.read_text() + '[transfers]\nrequest_rate=1\nrequest_burst=1\n')
            api_path, token = enable_api(ini, directory)
            options = {'ready_pattern': TLS_READY} if ENCRYPTED else {}
            hub = Hub(self, '--config', str(ini), '--log-connections', 'on', '--log-chat', 'on', **options)
            observer = Peer(self, connect_tls(self, hub, str(pathlib.Path(directory) / 'certificate.pem')) if ENCRYPTED else hub.connect())
            observer.login(b'ListObserver', 99, welcome=welcome)
            feature_update = b'BINF ' + observer.sid + (b' SUTCP4,ADCS' if PEER_TLS else b' SUTCP4')
            observer.send(feature_update); self.assertEqual(observer.line(), feature_update)
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
            terminal = os.ttyname(slave)
            fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack('HHHH', 30, 140, 0, 0))
            process = subprocess.Popen([NCDC, '-c', directory + '/session', '-n'],
                stdin=slave, stdout=slave, stderr=slave, start_new_session=True,
                env={**os.environ, 'TERM': 'xterm', 'LC_ALL': 'C.UTF-8'})
            os.close(slave)
            screen = bytearray()

            def observer_line():
                # ncdc draws its UI on the same thread that handles commands
                # and network events. A full PTY must not prevent the reply
                # whose arrival this test is waiting for. Keep the original
                # three-second reply budget, including fragmented TLS reads.
                deadline = time.monotonic() + 3
                old_timeout = observer.peer.gettimeout()
                try:
                    while b'\n' not in observer.pending:
                        if process.poll() is None and select.select([master], [], [], 0)[0]:
                            screen.extend(os.read(master, 65536))
                        remaining = deadline - time.monotonic()
                        self.assertGreater(remaining, 0, 'Timed out waiting for ncdc observer reply: ' + repr(screen[-3000:]))
                        observer.peer.settimeout(min(0.05, remaining))
                        try:
                            chunk = observer.peer.recv(4096)
                        except socket.timeout:
                            continue
                        self.assertTrue(chunk, 'Hub closed before a complete ncdc observer reply')
                        observer.pending += chunk
                        self.assertLessEqual(len(observer.pending), 256 * 1024)
                    line, observer.pending = observer.pending.split(b'\n', 1)
                    return line
                finally:
                    observer.peer.settimeout(old_timeout)

            def wait_screen(expected, timeout=6):
                deadline = time.monotonic() + timeout
                while expected not in screen:
                    self.assertIsNone(process.poll(), repr(screen[-3000:]))
                    self.assertLess(time.monotonic(), deadline, repr(screen[-5000:]))
                    if select.select([master], [], [], 0.1)[0]:
                        screen.extend(os.read(master, 65536))

            def peer_context(server=False):
                context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER if server else ssl.PROTOCOL_TLS_CLIENT)
                if not server:
                    context.check_hostname = False
                context.minimum_version = ssl.TLSVersion.TLSv1_2
                context.verify_mode = ssl.CERT_REQUIRED
                # The generated local ncdc certificate is the explicit test
                # trust anchor. This is not a claim of hub-side KEYP support.
                context.load_verify_locations(cafile=str(pathlib.Path(directory) / 'session/cert/client.crt'))
                context.load_cert_chain(str(pathlib.Path(directory) / 'certificate.pem'),
                                        str(pathlib.Path(directory) / 'private.pem'))
                return context

            def peer_connect(port):
                raw = socket.create_connection(('127.0.0.1', port), timeout=3)
                if not PEER_TLS:
                    return raw
                try:
                    peer = peer_context().wrap_socket(raw, server_hostname='localhost')
                    self.assertIn(peer.version(), ('TLSv1.2', 'TLSv1.3'))
                    self.assertTrue(peer.getpeercert(binary_form=True))
                    return peer
                except BaseException:
                    raw.close()
                    raise

            def activate_ncdc():
                # ncdc's port 0 means a random number, not an OS-assigned free
                # port. Reserve TCP/UDP numbers until ncdc advertises readiness.
                # Both sides use SO_REUSEADDR; the TCP reservation never listens.
                with socket.socket() as tcp_port, socket.socket(type=socket.SOCK_DGRAM) as udp_port:
                    for reservation in (tcp_port, udp_port):
                        reservation.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
                        reservation.bind(('127.0.0.1', 0))
                    tcp_number, udp_number = tcp_port.getsockname()[1], udp_port.getsockname()[1]
                    commands = (f'/hset active_ip 127.0.0.1\r/hset active_port {tcp_number}\r'
                                f'/hset active_udp_port {udp_number}\r/hset active true\r').encode()
                    os.write(master, commands)
                    advertised = {}
                    for _ in range(12):
                        delta = observer_line()
                        self.assertTrue(delta.startswith(b'BINF ' + replacement_sid + b' '), delta)
                        for field in delta.split()[2:]:
                            advertised[field[:2]] = field[2:]
                        features = advertised.get(b'SU', b'').split(b',')
                        if (b'TCP4' in features and b'UDP4' in features and
                                advertised.get(b'I4') == b'127.0.0.1' and
                                int(advertised.get(b'U4', b'0') or b'0') == udp_number):
                            return advertised, tcp_number
                    self.fail('ncdc did not publish its reserved active endpoints')

            def wait_capture(buffer, expected):
                deadline = time.monotonic() + 3
                while expected not in buffer:
                    self.assertLess(time.monotonic(), deadline, 'Proxy did not capture expected frame')
                    if select.select([master], [], [], 0.05)[0]:
                        screen.extend(os.read(master, 65536))

            try:
                wait_screen(b'Welcome')
                nickname = 'dc24hŻółw'.encode('utf-8')
                os.write(master, b'/set nick ' + nickname + b'\r')
                if PEER_TLS:
                    os.write(master, b'/set tls_policy prefer\r')
                scheme = 'adcs' if ENCRYPTED else 'adc'
                os.write(master, f'/open dc24h_login {scheme}://127.0.0.1:{listener.getsockname()[1]}/\r'.encode())
                hub.wait_for(lambda: hub.log.count(b'event=login_ready') == 2)
                initial_join = observer_line()
                self.assertTrue(initial_join.startswith(b'BINF '))
                initial_sid = initial_join.split()[1]
                # ncdc refuses chat until its own INF completed the initial user
                # list. The observer and own echo now prove client-side NORMAL
                # and public delivery.
                wait_screen(b'2 users')
                wait_screen(b'Welcome to dc24h.eu.')
                wait_screen(b'Configured ncdc chat.')
                observer.send(b'BMSG '+observer.sid+b' dc24hIncomingChat')
                self.assertEqual(observer_line(), b'BMSG '+observer.sid+b' dc24hIncomingChat')
                wait_screen(b'dc24hIncomingChat')
                os.write(master, b'dc24hLoginProbe\r')
                self.assertEqual(observer_line(), b'BMSG '+initial_sid+b' dc24hLoginProbe')
                wait_screen(b'dc24hLoginProbe')
                public_text='dc24hPublicŻółw 😀 path\\folder'.encode()
                os.write(master, public_text+b'\r')
                self.assertEqual(observer_line(), b'BMSG '+initial_sid+b' '+r'dc24hPublicŻółw\s😀\spath\\folder'.encode())
                wait_screen('dc24hPublicŻółw'.encode())
                validated = b'BMSG '+observer.sid+b' '+r'dc24hValidatedŻółw\s😀\nsecond\\line'.encode()+b' ME0'
                observer.send(validated); self.assertEqual(observer_line(),validated)
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
                        self.assertEqual(observer_line(), b'BMSG '+initial_sid+b' dc24hActionProbe\\slikes\\scats ME1')
                    else:
                        expected = b'EMSG '+initial_sid+b' '+observer.sid+b' dc24hPrivateProbe PM'+initial_sid
                        self.assertEqual(observer_line(), expected)
                        wait_screen(b'dc24hPrivateProbe')
                private_reply = b'EMSG '+observer.sid+b' '+initial_sid+b' dc24hPrivateReply PM'+observer.sid
                observer.send(private_reply);self.assertEqual(observer_line(), private_reply)
                wait_screen(b'dc24hPrivateReply')
                # PM routes the reply tab; explicit ME0 remains a plain message,
                # while ME01 is canonicalized to an action with the same origin.
                for flag, canonical, marker in ((b'ME-0', b'ME0', b'dc24hPrivatePlainFlag'),
                                                (b'ME01', b'ME1', b'dc24hPrivateActionFlag')):
                    reply = b'EMSG '+observer.sid+b' '+initial_sid+b' '+marker
                    observer.send(reply+b' '+flag+b' PM'+observer.sid)
                    self.assertEqual(observer_line(), reply+b' PM'+observer.sid+b' '+canonical)
                    wait_screen(marker)
                os.write(master, b'/me dc24hPrivateAction\r')
                self.assertEqual(observer_line(), b'EMSG '+initial_sid+b' '+observer.sid+b' dc24hPrivateAction PM'+initial_sid+b' ME1')
                os.write(master, b'/open dc24h_login\r')
                wait_screen(b'2 users')
                status = fetch_status(api_path, token)
                self.assertGreaterEqual(status['network']['adc_frames'], 3)
                self.assertEqual(status['users'], {'online': 2, 'peak_online': 2, 'successful_logins': 2})
                os.write(master, b'/hset description dc24hINFUpdate\r')
                live_update = observer_line()
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
                self.assertEqual(observer_line(), b'IQUI ' + initial_sid)
                replacement_join = observer_line()
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
                self.assertEqual(observer_line(), b'BMSG '+replacement_sid+b' dc24hRenamedProbe')
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
                if TRANSFER_CORE:
                    # A passive/passive request is explicitly refused; the
                    # connection and chat remain usable after policy admission.
                    observer.send(b'BINF ' + observer.sid + b' SU')
                    self.assertEqual(observer_line(), b'BINF ' + observer.sid + b' SU')
                    screen.clear()
                    os.write(master, b'/browse ListObserver\r')
                    wait_screen(b'RCM requires passive sender and active recipient')
                    os.write(master, b'/open dc24h_login\r')
                    os.write(master, b'dc24hTransferBoundaryRecovery\r')
                    self.assertEqual(observer_line(), b'BMSG ' + replacement_sid + b' dc24hTransferBoundaryRecovery')
                    observer.send(b'BINF ' + observer.sid + b' SUTCP4')
                    self.assertEqual(observer_line(), b'BINF ' + observer.sid + b' SUTCP4')
                if FEATURE_SEARCH:
                    # A real indexed share lets ncdc prove that it processed the
                    # routed FSCH by producing a result that reaches its issuer.
                    share = pathlib.Path(directory) / 'shared'
                    share.mkdir()
                    (share / 'FeatureRoutingProbe.txt').write_text('Feature routing interoperability probe.\n')
                    os.write(master, b'/share FeatureShare ' + str(share).encode() + b'\r')
                    deadline = time.monotonic() + 10
                    while True:
                        with closing(sqlite3.connect(pathlib.Path(directory) / 'session/db.sqlite3')) as database:
                            indexed = database.execute('SELECT 1 FROM hashfiles WHERE filename=?',
                                                       (str(share / 'FeatureRoutingProbe.txt'),)).fetchone()
                        if indexed:
                            break
                        self.assertLess(time.monotonic(), deadline, 'ncdc did not index its share')
                        if select.select([master], [], [], 0.05)[0]:
                            screen.extend(os.read(master, 65536))
                    # Force a normal INF delta after hashing rather than waiting
                    # for ncdc's periodic public-share update timer.
                    os.write(master, b'/hset description dc24hFeatureIndexReady\r')
                    while True:
                        delta = observer_line()
                        self.assertTrue(delta.startswith(b'BINF ' + replacement_sid + b' '), delta)
                        if b'DEdc24hFeatureIndexReady' in delta:
                            break
                if SEARCH_SECURITY:
                    os.write(master, b'/search -hub SecurityDeniedSecret\r')
                    os.write(master, b'\x1b2')
                    wait_screen(b'Search operation not permitted')
                    marker = b'BINF ' + observer.sid + b' DESecurityDeniedFence'
                    observer.send(marker); self.assertEqual(observer_line(), marker)
                    os.write(master, b'/say SecurityRecoveryProbe\r')
                    recovered = observer_line()
                    self.assertTrue(recovered.startswith(b'BMSG ' + replacement_sid + b' '))
                    self.assertIn(b'SecurityRecoveryProbe', recovered)
                else:
                    # Actual SCH delivery is checked at the observer. TLS proxy
                    # captures remain encrypted; the private API fences submission.
                    before_search = fetch_status(api_path, token)['network']['adc_frames']
                    command = (b'/search -hub -tth ' + TEST_TTH + b'\r' if TTH_SEARCH else
                               b'/search -hub -ge 1K -t archive SearchCoreSecret\r')
                    os.write(master, command)
                    deadline = time.monotonic() + 3
                    while fetch_status(api_path, token)['network']['adc_frames'] <= before_search:
                        self.assertLess(time.monotonic(), deadline, 'ncdc did not submit its search')
                        if select.select([master], [], [], 0.05)[0]:
                            screen.extend(os.read(master, 65536))
                    wait_screen(TEST_TTH if TTH_SEARCH else b'SearchCoreSecret')
                    # Search results use list key bindings, not the chat input.
                    routed_search = observer_line()
                    self.assertRegex(routed_search, rb'^[BF]SCH ' + replacement_sid + rb' ')
                    self.assertIn(b'TR' + TEST_TTH if TTH_SEARCH else b'ANSearchCoreSecret', routed_search)
                    if FEATURE_SEARCH:
                        search_token = re.search(rb'(?:^| )TO([^ ]+)', routed_search)
                        probe = (b'DRES ' + observer.sid + b' ' + replacement_sid +
                                 b' FN/ResultProbe/SearchCoreSecretNcdcResultProbe.zip SI2048 SL1 TR' +
                                 TEST_TTH + (b' TO' + search_token[1] if search_token else b''))
                        observer.send(probe)
                        wait_screen(b'NcdcResultProbe.zip')
                        if ENCRYPTED:
                            self.assertNotIn(b'NcdcResultProbe', replies)
                        else:
                            wait_capture(replies, probe + b'\n')
                    os.write(master, b'\x1b2')  # Alt+2 selects the hub tab.
                    self.assertNotIn(b'Search routing not implemented', screen)
                    if not ENCRYPTED and routed_search.startswith(b'BSCH '):
                        wait_capture(replies, routed_search + b'\n')
                    if ENCRYPTED:
                        self.assertNotIn(b'SearchCoreSecret', wire)
                        self.assertNotIn(TEST_TTH, wire)
                    elif TTH_SEARCH:
                        frame = re.search(rb'[BF]SCH ' + replacement_sid + rb' [^\n]*\n', bytes(wire))
                        self.assertIsNotNone(frame, 'ncdc did not emit a TTH SCH')
                        self.assertIn(b'TR' + TEST_TTH, frame[0])
                        self.assertNotRegex(frame[0], rb' (?:AN|EX)')
                        if routed_search.startswith(b'BSCH '): self.assertIn(routed_search + b'\n', replies)
                    else:
                        self.assertRegex(bytes(wire), rb'[BF]SCH ' + replacement_sid + rb' [^\n]*ANSearchCoreSecret')
                        self.assertRegex(bytes(wire), rb'[BF]SCH [^\n]*GE1024')
                        self.assertRegex(bytes(wire), rb'[BF]SCH [^\n]*EX(?:7z|7Z|zip|ZIP)')
                        if routed_search.startswith(b'BSCH '): self.assertIn(routed_search + b'\n', replies)
                    if FEATURE_SEARCH:
                        public_features = re.search(rb'(?:^| )SU([^ ]+)', replacement_join)
                        self.assertIsNotNone(public_features, replacement_join)
                        self.assertIn(b'ADC0', public_features[1].split(b','))
                        self.assertNotIn(b'TCP4', public_features[1].split(b','))
                        feature_query = (b'FSCH ' + observer.sid +
                                         b' -TCP4 ANFeatureRoutingProbe EXtxt TOFeatureRoutingProbeToken')
                        observer.send(feature_query)
                        received_result = observer_line()
                        self.assertTrue(received_result.startswith(b'DRES ' + replacement_sid + b' ' + observer.sid + b' '))
                        self.assertIn(b'/FeatureShare/FeatureRoutingProbe.txt', received_result)
                        self.assertIn(b'TOFeatureRoutingProbeToken', received_result)
                        self.assertRegex(received_result, rb' TR[A-Z2-7]{39}(?: |$)')
                        if ENCRYPTED:
                            self.assertNotIn(b'FeatureRoutingProbe', wire)
                            self.assertNotIn(b'FeatureRoutingProbe', replies)
                        else:
                            wait_capture(wire, received_result + b'\n')
                            self.assertIn(feature_query + b'\n', replies)
                            self.assertIn(received_result + b'\n', wire)
                            self.assertNotIn(b'Search\\sresult\\srouting\\snot\\simplemented', replies)
                        # The issuer receives one result, with no FSCH echo.
                        marker = b'BINF ' + observer.sid + b' DEFeatureRoutingFence'
                        observer.send(marker)
                        self.assertEqual(observer_line(), marker)
                    if ACTIVE_PASSIVE:
                        udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                        self.addCleanup(udp.close)
                        udp.bind(('127.0.0.1', 0)); udp.settimeout(3)
                        udp_port = str(udp.getsockname()[1]).encode()
                        normalized = b'BINF ' + observer.sid + b' SUTCP4,UDP4 I4127.0.0.1 U4' + udp_port
                        observer.send(b'BINF ' + observer.sid + b' SUTCP4,UDP4 I40.0.0.0 U4' + udp_port)
                        self.assertEqual(observer_line(), normalized)
                        udp_query = (b'FSCH ' + observer.sid +
                                     b' -TCP4 ANFeatureRoutingProbe EXtxt TOActivePassiveUdpToken')
                        observer.send(udp_query)
                        packet, _ = udp.recvfrom(16384)
                        cid = re.search(rb'(?:^| )ID([A-Z2-7]{39})(?: |$)', replacement_join)[1]
                        self.assertTrue(packet.startswith(b'URES ' + cid + b' '), packet)
                        self.assertIn(b'TOActivePassiveUdpToken', packet)
                        self.assertIn(b'/FeatureShare/FeatureRoutingProbe.txt', packet)
                        self.assertRegex(packet, rb' TR[A-Z2-7]{39}(?: |\n)')
                        # A fence proves no duplicate DRES or source echo reached the hub path.
                        marker = b'BINF ' + observer.sid + b' DEActivePassiveUdpFence'
                        observer.send(marker); self.assertEqual(observer_line(), marker)
                        # A zero port switches ncdc back to hub delivery, even with UDP4 advertised.
                        observer.send(b'BINF ' + observer.sid + b' U40')
                        self.assertEqual(observer_line(), b'BINF ' + observer.sid + b' U40')
                        observer.send(udp_query.replace(b'ActivePassiveUdpToken', b'ActivePassiveHubToken'))
                        fallback = observer_line()
                        self.assertTrue(fallback.startswith(b'DRES ' + replacement_sid + b' ' + observer.sid + b' '))
                        self.assertIn(b'TOActivePassiveHubToken', fallback)
                        # Turn ncdc itself active and verify actual UDP receipt in its search UI.
                        os.write(master, b'/set sudp_policy disabled\r')
                        advertised, _ = activate_ncdc()
                        os.write(master, b'/search -hub -ge 1K -t archive ActivePassiveNcdcSecret\r')
                        active_query = observer_line()
                        self.assertIn(b'ANActivePassiveNcdcSecret', active_query)
                        active_token = re.search(rb'(?:^| )TO([^ ]+)', active_query)
                        self.assertIsNotNone(active_token, 'Active ncdc search omitted TO')
                        probe = (b'URES ' + pair(99)[1] +
                                 b' FN/ActivePassiveNcdcSecretUdpProbe.zip SI2048 SL1 TR' + TEST_TTH +
                                 b' TO' + active_token[1] + b'\n')
                        udp.sendto(probe, ('127.0.0.1', int(advertised[b'U4'])))
                        wait_screen(b'ActivePassiveNcdcSecretUdpProbe.zip')
                        os.write(master, b'\x1b2')
                        self.assertNotIn(b'ActivePassiveNcdcSecretUdpProbe', replies)
                if PEER_TLS:
                    # Unsupported protocols are decided by the receiving peer.
                    # Its rejection must preserve the opaque original token.
                    unsupported = b'DCTM ' + observer.sid + b' ' + replacement_sid + b' future/1.0 1515 ProtocolPrivate\\s' + 'Żółw'.encode()
                    observer.send(unsupported)
                    rejected = observer_line()
                    self.assertEqual(rejected, b'DSTA ' + replacement_sid + b' ' + observer.sid + b' 141 Unknown\\sprotocol PRfuture/1.0 TOProtocolPrivate\\s' + 'Żółw'.encode())
                if RCM_TEST:
                    os.write(master, b'/open dc24h_login\r/browse ListObserver\r')
                    reverse = observer_line()
                    self.assertTrue(reverse.startswith(b'DRCM ' + replacement_sid + b' ' + observer.sid + b' '), reverse)
                    words = reverse.split(); self.assertEqual(words[3], PEER_PROTOCOL); self.assertTrue(words[4])
                    with socket.socket() as listener_cc:
                        listener_cc.bind(('127.0.0.1', 0)); listener_cc.listen(1); listener_cc.settimeout(3)
                        offer = (b'DCTM ' + observer.sid + b' ' + replacement_sid + b' ' + words[3] + b' ' +
                                 str(listener_cc.getsockname()[1]).encode() + b' ' + words[4])
                        observer.send(offer)
                        cc, _ = listener_cc.accept()
                        cc.settimeout(3)
                        if PEER_TLS:
                            try:
                                cc = peer_context(server=True).wrap_socket(cc, server_side=True)
                            except BaseException:
                                cc.close()
                                raise
                            self.assertIn(cc.version(), ('TLSv1.2', 'TLSv1.3'))
                            self.assertTrue(cc.getpeercert(binary_form=True))
                        with cc:
                            cc.settimeout(3); pending = bytearray()
                            def client_line():
                                while b'\n' not in pending:
                                    chunk = cc.recv(16384); self.assertTrue(chunk, 'Passive ncdc closed its peer connection'); pending.extend(chunk)
                                line, tail = bytes(pending).split(b'\n', 1); pending[:] = tail; return line
                            self.assertTrue(client_line().startswith(b'CSUP '))
                            cc.sendall(b'CSUP ADBASE ADTIGR\n')
                            client_inf = client_line()
                            # An initiating ncdc may announce its hub referrer
                            # with informational CSTA before client identity.
                            for _ in range(4):
                                if client_inf.startswith(b'CINF '): break
                                self.assertTrue(client_inf.startswith(b'CSTA 000 '), client_inf)
                                client_inf = client_line()
                            self.assertTrue(client_inf.startswith(b'CINF '), client_inf)
                            self.assertIn(b'ID' + public_cid(replacement_join), client_inf)
                            self.assertIn(b'TO' + words[4], client_inf)
                            cc.sendall(b'CINF ID' + pair(99)[1] + b'\n')
                            self.assertTrue(client_line().startswith(b'CGET '))
                    # Reverse direction: real ncdc receives RCM as active target,
                    # emits CTM preserving the requested protocol and token.
                    if TERMINAL_PRESSURE:
                        # Saturate terminal output without risking a blocking
                        # writer or filling ncdc's input queue. Its next UI
                        # redraw must wait for the harness to read the PTY.
                        terminal_fd = os.open(terminal, os.O_WRONLY | os.O_NONBLOCK | os.O_NOCTTY)
                        try:
                            for _ in range(64):
                                try:
                                    os.write(terminal_fd, b'TerminalPressure' * 256)
                                except BlockingIOError:
                                    break
                            else:
                                self.fail('Could not saturate ncdc terminal output')
                        finally:
                            os.close(terminal_fd)
                        os.write(master, b'/help\r')
                        time.sleep(0.2)  # Let ncdc reach its blocked UI redraw.
                    _, active_tcp_port = activate_ncdc()
                    passive_update = b'BINF ' + observer.sid + (b' SUADCS' if PEER_TLS else b' SU')
                    observer.send(passive_update)
                    self.assertEqual(observer_line(), passive_update)
                    token_rcm = b'RcmNcdcIncomingPrivate'
                    if TRANSFER_TOKENS:
                        decoded_token = ' TokenNcdcPrivate Żółw \\😀 '.encode()
                        decoded_token += b'T' * (127 - len(decoded_token)) + b' '
                        self.assertEqual(len(decoded_token), 128)
                        token_rcm = decoded_token.replace(b'\\', b'\\\\').replace(b' ', b'\\s')
                    observer.send(b'DRCM ' + observer.sid + b' ' + replacement_sid + b' ' + PEER_PROTOCOL + b' ' + token_rcm)
                    reply = observer_line().split()
                    self.assertEqual(reply[:4], [b'DCTM', replacement_sid, observer.sid, PEER_PROTOCOL])
                    self.assertEqual(reply[5], token_rcm); self.assertEqual(int(reply[4]), active_tcp_port)
                    with peer_connect(int(reply[4])) as cc:
                        cc.sendall(b'CSUP ADBASE ADTIGR\n'); pending = bytearray()
                        def server_line():
                            while b'\n' not in pending:
                                chunk = cc.recv(16384); self.assertTrue(chunk, 'Active ncdc closed its RCM listener'); pending.extend(chunk)
                            line, tail = bytes(pending).split(b'\n', 1); pending[:] = tail; return line
                        self.assertTrue(server_line().startswith(b'CSUP '))
                        cc.sendall(b'CINF ID' + pair(99)[1] + b' TO' + token_rcm + b'\n')
                        self.assertTrue(server_line().startswith(b'CINF '))
                    observer.send(b'BMSG ' + observer.sid + b' RcmNcdcHandshakeRecovery')
                    self.assertEqual(observer_line(), b'BMSG ' + observer.sid + b' RcmNcdcHandshakeRecovery')
                    wait_screen(b'RcmNcdcHandshakeRecovery')
                    self.assertNotIn(token_rcm, hub.log)
                    if TRANSFER_TOKENS:
                        # A real peer rejection must return the exact same
                        # boundary-size opaque token, including edge spaces.
                        update = b'BINF ' + observer.sid + (b' SUTCP4,ADCS' if PEER_TLS else b' SUTCP4')
                        observer.send(update)
                        self.assertEqual(observer_line(), update)
                        observer.send(b'DCTM ' + observer.sid + b' ' + replacement_sid + b' future/1.0 1515 ' + token_rcm)
                        self.assertEqual(observer_line(), b'DSTA ' + replacement_sid + b' ' + observer.sid + b' 141 Unknown\\sprotocol PRfuture/1.0 TO' + token_rcm)
                        self.assertNotIn(b'TokenNcdcPrivate', hub.log)
                if CLIENT_CAPABILITIES:
                    # Change the real client's live mode without reconnecting.
                    # Both peers passive must yield a private recoverable refusal.
                    os.write(master, b'/hset active false\r')
                    for _ in range(12):
                        delta = observer_line()
                        self.assertTrue(delta.startswith(b'BINF ' + replacement_sid + b' '), delta)
                        declared = next((field[2:].split(b',') for field in delta.split()[2:] if field.startswith(b'SU')), None)
                        if declared is not None and b'TCP4' not in declared:
                            break
                    else:
                        self.fail('ncdc did not cancel its TCP4 declaration')
                    denied = b'DRCM ' + observer.sid + b' ' + replacement_sid + b' ' + PEER_PROTOCOL + b' CapabilityNcdcPrivate'
                    observer.send(denied)
                    self.assertEqual(observer_line(), b'ISTA 100 RCM\\srequires\\spassive\\ssender\\sand\\sactive\\srecipient FCDRCM')
                    _, active_tcp_port = activate_ncdc()
                    observer.send(denied)
                    reply = observer_line().split()
                    self.assertEqual(reply[:4], [b'DCTM', replacement_sid, observer.sid, PEER_PROTOCOL])
                    self.assertEqual(int(reply[4]), active_tcp_port)
                    self.assertEqual(reply[5], b'CapabilityNcdcPrivate')
                    with peer_connect(active_tcp_port) as cc:
                        cc.sendall(b'CSUP ADBASE ADTIGR\n')
                        pending = bytearray()
                        def capability_line():
                            while b'\n' not in pending:
                                chunk = cc.recv(16384)
                                self.assertTrue(chunk, 'Reactivated ncdc closed its listener')
                                pending.extend(chunk)
                            line, tail = bytes(pending).split(b'\n', 1)
                            pending[:] = tail
                            return line
                        self.assertTrue(capability_line().startswith(b'CSUP '))
                        cc.sendall(b'CINF ID' + pair(99)[1] + b' TOCapabilityNcdcPrivate\n')
                        self.assertTrue(capability_line().startswith(b'CINF '))
                    observer.send(b'BMSG ' + observer.sid + b' CapabilityNcdcRecovery')
                    self.assertEqual(observer_line(), b'BMSG ' + observer.sid + b' CapabilityNcdcRecovery')
                    wait_screen(b'CapabilityNcdcRecovery')
                    self.assertNotIn(b'CapabilityNcdcPrivate', hub.log)
                if CTM_TEST:
                    _, active_tcp_port = activate_ncdc()
                    os.write(master, b'/open dc24h_login\r/browse ListObserver\r')
                    offer = observer_line()
                    self.assertTrue(offer.startswith(b'DCTM ' + replacement_sid + b' ' + observer.sid + b' '), offer)
                    words = offer.split(); self.assertEqual(words[3], PEER_PROTOCOL)
                    self.assertEqual(int(words[4]), active_tcp_port); self.assertTrue(words[5])
                    # Use the hub-forwarded listener and token in an actual peer
                    # handshake. The hub never opens this connection or sees CINF.
                    with peer_connect(int(words[4])) as cc:
                        cc.sendall(b'CSUP ADBASE ADTIGR\n')
                        pending = bytearray()
                        def cc_line():
                            while b'\n' not in pending:
                                chunk = cc.recv(16384); self.assertTrue(chunk, 'ncdc closed its CTM listener'); pending.extend(chunk)
                            line, tail = bytes(pending).split(b'\n', 1); pending[:] = tail; return line
                        self.assertTrue(cc_line().startswith(b'CSUP '))
                        cc.sendall(b'CINF ID' + pair(99)[1] + b' TO' + words[5] + b'\n')
                        peer_inf = cc_line()
                        self.assertTrue(peer_inf.startswith(b'CINF '), peer_inf)
                        self.assertIn(b'ID' + public_cid(replacement_join), peer_inf)
                        request = cc_line()
                        self.assertTrue(request.startswith(b'CGET '), request)
                    observer.send(b'BMSG ' + observer.sid + b' CtmNcdcHandshakeRecovery')
                    self.assertEqual(observer_line(), b'BMSG ' + observer.sid + b' CtmNcdcHandshakeRecovery')
                    wait_screen(b'CtmNcdcHandshakeRecovery')
                    if not ENCRYPTED:
                        wait_capture(wire, offer+b'\n'); self.assertIn(offer+b'\n', wire)
                    self.assertNotIn(b'CINF ID', hub.log)
                if TRANSFER_ERRORS:
                    # Reset an actual accepted peer connection. ncdc 1.23.1
                    # handles socket failure locally rather than promising a
                    # hub DSTA for every failed TCP attempt.
                    error_token = 'ErrorNcdcPrivate Żółw \\ token'.encode().replace(b'\\', b'\\\\').replace(b' ', b'\\s')
                    with socket.socket() as failing_peer:
                        failing_peer.bind(('127.0.0.1', 0))
                        failing_peer.listen(1)
                        failing_peer.settimeout(3)
                        port = str(failing_peer.getsockname()[1]).encode()
                        observer.send(b'DCTM ' + observer.sid + b' ' + replacement_sid + b' ' + PEER_PROTOCOL + b' ' + port + b' ' + error_token)
                        cc, _ = failing_peer.accept()
                        with cc:
                            cc.settimeout(3)
                            prefix = b'\x16\x03' if PEER_TLS else b'CSUP '
                            received = b''
                            while len(received) < len(prefix):
                                chunk = cc.recv(4096)
                                self.assertTrue(chunk, 'ncdc closed before starting its peer protocol')
                                received += chunk
                            self.assertTrue(received.startswith(prefix), received[:32])
                            cc.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack('ii', 1, 0))
                    observer.send(b'DCTM ' + observer.sid + b' ' + replacement_sid + b' future/1.0 1515 ' + error_token)
                    for _ in range(3):
                        failure = observer_line()
                        if failure.startswith(b'DSTA ' + replacement_sid + b' ' + observer.sid + b' 141 '):
                            break
                        self.assertTrue(failure.startswith(b'DSTA ' + replacement_sid + b' ' + observer.sid + b' 142 '), failure)
                        self.assertIn(b' PR' + PEER_PROTOCOL, failure)
                        self.assertIn(b' TO' + error_token, failure)
                    else:
                        self.fail('ncdc did not report unsupported peer protocol')
                    self.assertIn(b' PRfuture/1.0', failure)
                    self.assertIn(b' TO' + error_token, failure)
                    # A routed fatal peer report must not terminate hub NORMAL.
                    observer.send(b'DSTA ' + observer.sid + b' ' + replacement_sid + b' 242 PeerFailure PR' + PEER_PROTOCOL + b' TO' + error_token)
                    marker = b'BMSG ' + observer.sid + b' ErrorNcdcStillOnline'
                    observer.send(marker)
                    self.assertEqual(observer_line(), marker)
                    wait_screen(b'ErrorNcdcStillOnline')
                    os.write(master, b'/open dc24h_login\rErrorNcdcRecovery\r')
                    self.assertEqual(observer_line(), b'BMSG ' + replacement_sid + b' ErrorNcdcRecovery')
                    wait_screen(b'ErrorNcdcRecovery')
                if CONNECTION_SECURITY:
                    if CONNECTION_SECURITY_ACTIVE:
                        activate_ncdc()
                    screen.clear()
                    os.write(master, b'/open dc24h_login\r/browse ListObserver\r')
                    wait_screen(b'Connection operation not permitted')
                    if not ENCRYPTED:
                        command = b'DCTM' if CONNECTION_SECURITY_ACTIVE else b'DRCM'
                        wait_capture(wire, command + b' ' + replacement_sid + b' ' + observer.sid)
                        wait_capture(replies, b'ISTA 125 Connection\\soperation\\snot\\spermitted FC' + command + b'\n')
                    os.write(master, b'/open dc24h_login\rSecurityNcdcRecovery\r')
                    self.assertEqual(observer_line(), b'BMSG ' + replacement_sid + b' SecurityNcdcRecovery')
                    wait_screen(b'SecurityNcdcRecovery')
                if TRANSFER_LIMITS:
                    # One raw sender floods a real ncdc recipient. Only the
                    # admitted unsupported-protocol offer may reach the client.
                    time.sleep(1.1)
                    limited_offer = b'DCTM ' + observer.sid + b' ' + replacement_sid + b' future/1.0 1515 LimitsNcdcPrivate'
                    observer.peer.sendall((limited_offer + b'\n') * 32)
                    received = [observer_line(), observer_line()]
                    self.assertEqual(sum(line.startswith(b'ISTA 100 Connection\\srequest\\srate\\slimit\\sexceeded FCDCTM') for line in received), 1)
                    self.assertEqual(sum(line.startswith(b'DSTA ' + replacement_sid + b' ' + observer.sid + b' 141 ') for line in received), 1)
                    time.sleep(1.1)
                    observer.send(limited_offer)
                    self.assertTrue(observer_line().startswith(b'DSTA ' + replacement_sid + b' ' + observer.sid + b' 141 '))
                    marker = b'BMSG ' + observer.sid + b' LimitsNcdcRecovery'
                    observer.send(marker)
                    self.assertEqual(observer_line(), marker)
                    wait_screen(b'LimitsNcdcRecovery')
                    os.write(master, b'/open dc24h_login\rLimitsNcdcStillOnline\r')
                    self.assertEqual(observer_line(), b'BMSG ' + replacement_sid + b' LimitsNcdcStillOnline')
                    self.assertNotIn(b'LimitsNcdcPrivate', hub.log)
                os.write(master, b'/quit\r')
                deadline = time.monotonic() + 4
                while process.poll() is None and time.monotonic() < deadline:
                    if select.select([master], [], [], 0.1)[0]:
                        try:
                            os.read(master, 65536)
                        except OSError:
                            break
                self.assertEqual(process.wait(timeout=2), 0)
                self.assertEqual(observer_line(), b'IQUI ' + replacement_sid)
                observer.send(b'DSTA ' + observer.sid + b' ' + replacement_sid + b' 100 Departed')
                self.assertTrue(observer_line().startswith(b'ISTA 140 '))
                deadline = time.monotonic() + 3
                while True:
                    stats = fetch_status(api_path, token)
                    if stats['users']['online'] == 1 and stats['network']['closed_connections'] == 2:
                        break
                    self.assertLess(time.monotonic(), deadline, stats)
                    time.sleep(0.01)
                search_stats=stats['search']['statistics']
                self.assertGreaterEqual(search_stats['queries_received'],1)
                self.assertGreaterEqual(search_stats['queries_rejected' if SEARCH_SECURITY else 'queries_routed'],1)
                if ACTIVE_PASSIVE:
                    self.assertEqual(search_stats['results_received'],3, 'Direct UDP entered hub result counters')
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
                self.assertNotIn(b'SearchCoreSecret', hub.log)
                self.assertNotIn(b'SecurityDeniedSecret', hub.log)
                self.assertNotIn(b'SecurityRecoveryProbe', hub.log)
                self.assertNotIn(b'ErrorNcdcPrivate', hub.log)
                self.assertNotIn(TEST_TTH, hub.log)
                self.assertNotIn(b'FeatureRoutingProbe', hub.log)
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
