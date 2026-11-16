"""Account-backed GPA/PAS on the actual TCP/TLS reactor and real ncdc."""
import base64
import configparser
import ctypes
import fcntl
import os
from pathlib import Path
import pty
import select
import socket
import ssl
import sqlite3
import struct
import subprocess
import sys
import tempfile
import termios
import time
import unittest

from network_process_tests import Hub, BINARY
from adc_login_process_tests import Peer, pair
from api_support import enable_api, fetch_status
from tls_support import credentials, connect_tls, tls_endpoint

BASE = Path(sys.argv.pop(1)).resolve()
KEY = Path(sys.argv.pop(1)).resolve()
NCDC = sys.argv.pop(1)
PASSWORD = b'ADC wire password 42!'
WELCOME = b'IMSG Authentication\\sready.'


def response(challenge, password=PASSWORD):
    salt = base64.b32decode(challenge + b'=' * ((-len(challenge)) % 8))
    assert len(salt) == 24
    value = password + salt
    gcrypt = ctypes.CDLL('libgcrypt.so.20')
    gcrypt.gcry_md_hash_buffer.argtypes = [ctypes.c_int, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t]
    result = ctypes.create_string_buffer(24)
    gcrypt.gcry_md_hash_buffer(306, result, value, len(value))
    return base64.b32encode(result.raw).rstrip(b'=')


class Client:
    def __init__(self, case, folder):
        self.case = case
        self.master, slave = pty.openpty()
        fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack('HHHH', 30, 140, 0, 0))
        self.process = subprocess.Popen([NCDC, '-c', str(folder), '-n'], stdin=slave, stdout=slave, stderr=slave,
                                        start_new_session=True, env={**os.environ, 'TERM': 'xterm', 'LC_ALL': 'C.UTF-8'})
        os.close(slave)
        self.screen = bytearray()
        case.addCleanup(self.close)
        self.wait_screen(b'welcome')

    def poll(self, hub=None):
        inputs = [self.master] + ([hub.process.stderr] if hub else [])
        readable = select.select(inputs, [], [], 0.03)[0]
        for source in readable:
            chunk = os.read(source if isinstance(source, int) else source.fileno(), 65536)
            if source == self.master:
                self.screen.extend(chunk)
            else:
                hub.log.extend(chunk)

    def wait(self, condition, hub=None, timeout=8):
        deadline = time.monotonic() + timeout
        while not condition():
            # Do not print a terminal transcript containing typed passwords.
            self.case.assertIsNone(self.process.poll(), 'ncdc exited before its expected state')
            self.case.assertLess(time.monotonic(), deadline, 'ncdc authentication timed out')
            self.poll(hub)

    def wait_screen(self, marker, start=0, hub=None):
        self.wait(lambda: marker in bytes(self.screen[start:]).lower(), hub)

    def command(self, command):
        os.write(self.master, command + b'\r')

    def close(self):
        if self.process.poll() is None:
            self.process.kill()
        self.process.wait(timeout=3)
        if self.master is not None:
            os.close(self.master)
            self.master = None


class AuthenticationTests(unittest.TestCase):
    def config(self, require_tls=True, key=KEY, policy=''):
        folder = tempfile.TemporaryDirectory(prefix='dc24h-adc-auth-')
        self.addCleanup(folder.cleanup)
        root = Path(folder.name)
        cert, private = credentials(folder.name)
        source = configparser.ConfigParser(interpolation=None)
        self.assertTrue(source.read(BASE))
        database = dict(source['database'])
        for name in ('socket', 'password_file'):
            database[name] = str((BASE.parent / database[name]).resolve())
        ini = root / 'hub.ini'
        ini.write_text('[network]\nlisten=127.0.0.1:0\nadcs_listen=127.0.0.1:0\n'
                       '[tls]\ncertificate=' + cert + '\nprivate_key=' + private + '\n'
                       '[database]\n' + ''.join(k + '=' + v + '\n' for k, v in database.items()) +
                       '[authentication]\nenabled=on\nkey_file=' + str(key) + '\nrequire_tls=' +
                       ('on' if require_tls else 'off') + '\n'
                       '[security]\nlogin_timeout=2\nmax_accepts_per_minute=1000\n'
                       '[chat]\nwelcome_message=Authentication ready.\n' + policy)
        return root, ini, cert

    def hub(self, require_tls=True, policy=''):
        root, ini, cert = self.config(require_tls, policy=policy)
        return Hub(self, '--config', str(ini), '--log-connections', 'on'), root, cert

    def peer(self, hub, cert, secure=True):
        raw = connect_tls(self, hub, cert) if secure else hub.connect()
        self.addCleanup(raw.close)
        return Peer(self, raw)

    def source_peer(self, hub, cert, source, secure):
        endpoint = tls_endpoint(hub) if secure else hub.endpoints[0]
        raw = socket.create_connection(endpoint, timeout=3, source_address=(source, 0))
        self.addCleanup(raw.close)
        if secure:
            raw = ssl.create_default_context(cafile=cert).wrap_socket(raw, server_hostname='localhost')
            self.addCleanup(raw.close)
        return Peer(self, raw)

    def identify(self, peer, nick=b'RegisteredADC', seed=7):
        peer.handshake()
        peer.send(peer.identity(nick, seed))
        return peer.line()

    def failed(self, peer, first=None):
        line = first if first is not None else peer.line()
        self.assertTrue(line.startswith(b'ISTA 223 '), 'Authentication refusal missing')
        self.assertTrue(peer.line().startswith(b'IQUI ' + peer.sid + b' '))
        self.assertEqual(peer.peer.recv(1), b'')

    def finish(self, peer):
        line = peer.line()
        self.assertTrue(line.startswith(b'IINF '))
        while not line.startswith(b'BINF ' + peer.sid + b' '):
            line = peer.line()
        self.assertEqual(peer.line(), WELCOME)

    def test_raw_challenges_failures_and_guest_separation(self):
        hub, _, cert = self.hub()
        observer = self.peer(hub, cert, False)
        observer.login(b'GuestObserver', 99, welcome=WELCOME)
        previous = None
        for secret in (b'Wrong ADC password!', None, PASSWORD):
            peer = self.peer(hub, cert)
            first = self.identify(peer)
            self.assertTrue(first.startswith(b'IGPA '))
            nonce = first[5:]
            answer = response(nonce, secret) if secret is not None else previous
            self.assertIsNotNone(answer)
            peer.send(b'HPAS ' + answer)
            if secret != PASSWORD:
                self.failed(peer)
            else:
                self.finish(peer)
                self.assertTrue(observer.line().startswith(b'BINF ' + peer.sid + b' '))
                peer.send(b'BMSG ' + peer.sid + b' AuthenticatedChat')
                self.assertEqual(peer.line(), b'BMSG ' + peer.sid + b' AuthenticatedChat')
                self.assertEqual(observer.line(), b'BMSG ' + peer.sid + b' AuthenticatedChat')
                peer.send(b'BINF ' + peer.sid + b' NIRenamedAccount')
                self.failed(peer)
                self.assertEqual(observer.line(), b'IQUI ' + peer.sid)
            # A correct response for one challenge must fail on the next connection.
            previous = response(nonce)
        for nick, seed, secure in ((b'RegisteredADC', 10, True), (b'OtherNick', 7, True),
                                   (b'DisabledADC', 8, True), (b'MissingADC', 9, True),
                                   (b'RegisteredADC', 7, False)):
            peer = self.peer(hub, cert, secure)
            self.failed(peer, self.identify(peer, nick, seed))
        hub.stop()
        self.assertEqual(hub.log.count(b'event=login_ready'), 2)
        for value in (PASSWORD, b'HPAS', b'IGPA', b'AuthenticatedChat', b'DC24HADC1', b'$dc24h-scrypt$'):
            self.assertNotIn(value, hub.log)

    def test_role_grants_guest_policy_and_global_switches(self):
        for secure in (False, True):
            with self.subTest(secure=secure):
                hub, _, cert = self.hub(False, '[search]\nguest_queries=off\nguest_results=off\n'
                    '[transfers]\nguest_initiate=off\nguest_receive=off\n')
                observer = self.peer(hub, cert, secure)
                observer.login(b'RoleGuest', 99, welcome=WELCOME)
                registered = self.peer(hub, cert, secure)
                first = self.identify(registered)
                registered.send(b'HPAS ' + response(first[5:])); self.finish(registered)
                self.assertTrue(observer.line().startswith(b'BINF ' + registered.sid + b' '))
                moderator = self.peer(hub, cert, secure)
                first = self.identify(moderator, b'ModeratorADC', 12)
                moderator.send(b'HPAS ' + response(first[5:])); self.finish(moderator)
                for peer in (observer, registered):
                    self.assertTrue(peer.line().startswith(b'BINF ' + moderator.sid + b' '))
                for source in (observer, registered, moderator):
                    update = b'BINF ' + source.sid + b' SUTCP4'
                    source.send(update)
                    for peer in (observer, registered, moderator): self.assertEqual(peer.line(), update)
                observer.send(b'HSUP ADOPER')
                observer.send(b'DSCH ' + observer.sid + b' ' + moderator.sid + b' ANRoleGuest')
                self.assertTrue(observer.line().startswith(b'ISTA 125 Search\\soperation\\snot\\spermitted'))
                query = b'DSCH ' + registered.sid + b' ' + moderator.sid + b' ANRolePrivate TORoleToken'
                registered.send(query); self.assertEqual(moderator.line(), query)
                result = b'DRES ' + moderator.sid + b' ' + registered.sid + b' FN/file SI0 TR' + b'A'*39 + b' TORoleToken'
                moderator.send(result); self.assertEqual(registered.line(), result)
                transfer = b'DCTM ' + registered.sid + b' ' + moderator.sid + b' ADC/1.0 1511 RolePrivateTransfer'
                registered.send(transfer); self.assertEqual(moderator.line(), transfer)
                observer.send(b'DCTM ' + observer.sid + b' ' + moderator.sid + b' ADC/1.0 1511 RolePrivateDenied')
                self.assertTrue(observer.line().startswith(b'ISTA 125 Connection\\soperation\\snot\\spermitted'))
                # A public role claim is rejected instead of becoming authority.
                observer.send(b'BINF ' + observer.sid + b' CT4')
                self.assertTrue(observer.line().startswith(b'ISTA 2'))
                hub.stop()
                for value in (PASSWORD, b'HPAS', b'IGPA', b'RolePrivate'):
                    self.assertNotIn(value, hub.log)
                blocked, _, blocked_cert = self.hub(False, '[search]\nqueries_enabled=off\n[transfers]\nctm_enabled=off\n')
                peers = []
                for nickname, seed in ((b'AdministratorADC', 13), (b'RegisteredADC', 7)):
                    peer = self.peer(blocked, blocked_cert, secure)
                    challenge = self.identify(peer, nickname, seed)
                    peer.send(b'HPAS ' + response(challenge[5:])); self.finish(peer)
                    for earlier in peers: self.assertTrue(earlier.line().startswith(b'BINF ' + peer.sid + b' '))
                    peers.append(peer)
                admin, target = peers
                for source in peers:
                    update = b'BINF ' + source.sid + b' SUTCP4'
                    source.send(update)
                    for peer in peers: self.assertEqual(peer.line(), update)
                admin.send(b'DSCH ' + admin.sid + b' ' + target.sid + b' ANDisabled')
                self.assertTrue(admin.line().startswith(b'ISTA 125 Search\\soperation\\sis\\sdisabled'))
                admin.send(b'DCTM ' + admin.sid + b' ' + target.sid + b' ADC/1.0 1511 Disabled')
                self.assertTrue(admin.line().startswith(b'ISTA 125 Connection\\soperation\\sis\\sdisabled'))
                blocked.stop()

    def test_attempt_limits_shared_ip_account_and_recovery(self):
        policies = (
            ('attempts_per_ip=2\nattempts_per_account=100\n', ('127.0.0.1', '127.0.0.1', '127.0.0.1'), b'RegisteredADC', 7, '127.0.0.2'),
            ('attempts_per_ip=100\nattempts_per_account=2\n', ('127.0.0.1', '127.0.0.2', '127.0.0.3'), b'ModeratorADC', 12, '127.0.0.3'),
        )
        for policy, sources, allowed_nick, allowed_seed, allowed_source in policies:
            with self.subTest(policy=policy):
                root, ini, cert = self.config(False, policy='[authentication]\n' + policy +
                    'attempt_window_seconds=60\nattempts_per_second=100000\n')
                endpoint, token = enable_api(ini, root)
                hub = Hub(self, '--config', str(ini), '--log-connections', 'on')
                for index, source in enumerate(sources):
                    peer = self.source_peer(hub, cert, source, index != 0)
                    first = self.identify(peer)
                    if index < 2:
                        self.assertTrue(first.startswith(b'IGPA '))
                        peer.send(b'HPAS ' + response(first[5:], b'Wrong ADC password!'))
                        self.failed(peer)
                    else:
                        # Throttled clients never receive a challenge or user list.
                        self.assertTrue(first.startswith(b'ISTA 223 '))
                        self.failed(peer, first)
                allowed = self.source_peer(hub, cert, allowed_source, True)
                first = self.identify(allowed, allowed_nick, allowed_seed)
                self.assertTrue(first.startswith(b'IGPA '))
                allowed.send(b'HPAS ' + response(first[5:])); self.finish(allowed)
                self.assertEqual(fetch_status(endpoint, token)['network']['security_closed'], 1)
                hub.stop()
                self.assertIn(b'event=authentication_limit', hub.log)
                self.assertEqual(hub.log.count(b'event=login_ready'), 1)
                for value in (PASSWORD, b'HPAS', b'IGPA', b'RegisteredADC', b'ModeratorADC', b'127.0.0.2', b'127.0.0.3'):
                    self.assertNotIn(value, hub.log)
        hub, _, cert = self.hub(False, '[authentication]\nattempts_per_ip=1\nattempts_per_account=1\n'
            'attempt_window_seconds=1\nattempts_per_second=100000\n')
        wrong = self.source_peer(hub, cert, '127.0.0.1', False)
        challenge = self.identify(wrong); self.assertTrue(challenge.startswith(b'IGPA '))
        wrong.send(b'HPAS ' + response(challenge[5:], b'Wrong ADC password!')); self.failed(wrong)
        time.sleep(1.1)
        recovered = self.source_peer(hub, cert, '127.0.0.1', True)
        fresh = self.identify(recovered); self.assertTrue(fresh.startswith(b'IGPA '))
        self.assertNotEqual(fresh, challenge)
        recovered.send(b'HPAS ' + response(fresh[5:])); self.finish(recovered)
        hub.stop()

    def test_attempt_limit_configuration_bounds(self):
        invalid = ('attempts_per_ip=0', 'attempts_per_ip=100001', 'attempts_per_account=0',
            'attempts_per_account=100001', 'attempts_per_second=0', 'attempts_per_second=100001',
            'attempt_window_seconds=0', 'attempt_window_seconds=3601', 'max_tracked_attempt_ips=0',
            'max_tracked_attempt_ips=65537', 'max_tracked_attempt_accounts=0', 'max_tracked_attempt_accounts=65537',
            'attempts_per_ip=-1', 'attempts_per_account=no', 'attempts_per_ip=2\nattempts_per_ip=3')
        with tempfile.TemporaryDirectory(prefix='dc24h-auth-limits-') as temp:
            ini = Path(temp) / 'hub.ini'
            for setting in invalid:
                ini.write_text('[authentication]\n' + setting + '\n')
                result = subprocess.run([BINARY, '--config', str(ini), '--check-config'], capture_output=True, timeout=3)
                self.assertEqual(result.returncode, 2, setting)
            ini.write_text('[authentication]\nattempts_per_ip=100000\nattempts_per_account=100000\n'
                'attempts_per_second=100000\nattempt_window_seconds=3600\n'
                'max_tracked_attempt_ips=65536\nmax_tracked_attempt_accounts=65536\n')
            result = subprocess.run([BINARY, '--config', str(ini), '--check-config'], capture_output=True, timeout=3)
            self.assertEqual(result.returncode, 0)

    def test_authenticated_disconnect_requires_fresh_login(self):
        hub, _, cert = self.hub(False)
        observer = self.peer(hub, cert, False)
        observer.login(b'SessionObserver', 110, welcome=WELCOME)
        previous = None
        for secure in (False, True):
            for _ in range(2):
                peer = self.peer(hub, cert, secure)
                first = self.identify(peer)
                self.assertTrue(first.startswith(b'IGPA '))
                challenge = first[5:]
                self.assertNotEqual(challenge, previous)
                # No new public session is published before its own fresh PAS.
                observer.peer.settimeout(0.05)
                with self.assertRaises(socket.timeout):
                    observer.peer.recv(1)
                observer.peer.settimeout(3)
                peer.send(b'HPAS ' + response(challenge))
                self.finish(peer)
                self.assertTrue(observer.line().startswith(b'BINF ' + peer.sid + b' '))
                peer.send(b'BMSG ' + peer.sid + b' FreshSession')
                self.assertEqual(peer.line(), b'BMSG ' + peer.sid + b' FreshSession')
                self.assertEqual(observer.line(), b'BMSG ' + peer.sid + b' FreshSession')
                peer.peer.close()
                self.assertEqual(observer.line(), b'IQUI ' + peer.sid)
                previous = challenge
        hub.stop()
        self.assertEqual(hub.log.count(b'event=login_ready'), 5)
        for value in (PASSWORD, b'HPAS', b'IGPA', b'FreshSession'):
            self.assertNotIn(value, hub.log)

    def test_wrong_key_refuses_startup_and_config_aliases(self):
        root, ini, _ = self.config(key=Path(str(KEY) + '.wrong'))
        result = subprocess.run([BINARY, '--config', str(ini)], capture_output=True, timeout=5)
        self.assertNotEqual(result.returncode, 0)
        self.assertNotIn(b'Network listening on', result.stderr)
        self.assertNotIn(str(KEY).encode(), result.stderr)
        root, ini, _ = self.config()
        original = ini.read_text()
        check = subprocess.run([BINARY, '--config', str(ini), '--check-config'], capture_output=True, timeout=3)
        self.assertEqual(check.returncode, 0, check.stderr)
        for replacement in (root / 'missing.key', BASE, Path(str(KEY) + '.malformed')):
            if replacement.name.endswith('.malformed'):
                replacement.write_text('z' * 64)
                replacement.chmod(0o600)
            ini.write_text(original.replace(str(KEY), str(replacement)))
            check = subprocess.run([BINARY, '--config', str(ini), '--check-config'], capture_output=True, timeout=3)
            self.assertEqual(check.returncode, 2)
            self.assertNotIn(str(replacement).encode(), check.stderr)
        ini.write_text(original)
        for line in ('[authentication]\nenabled=on\n', '[authentication]\nenabled=on\nkey_file=missing.key\nrequire_tls=off\n'):
            invalid = root / 'invalid.ini'
            invalid.write_text('[network]\nlisten=127.0.0.1:0\n' + line)
            check = subprocess.run([BINARY, '--config', str(invalid), '--check-config'], capture_output=True, timeout=3)
            self.assertEqual(check.returncode, 2)

    def test_real_ncdc_password_rejection_then_normal_adc_and_adcs(self):
        for secure in (True, False):
            hub, root, cert = self.hub(require_tls=secure)
            session = root / 'ncdc'
            initialized = Client(self, session)
            initialized.command(b'/quit')
            initialized.process.wait(timeout=3)
            initialized.close()
            # Deterministic private client identity matches the enrolled fixture.
            pid, cid = pair(7)
            with sqlite3.connect(session / 'db.sqlite3') as db:
                for name, value in (('pid', pid.decode()), ('cid', cid.decode()), ('nick', 'RegisteredADC')):
                    db.execute('UPDATE vars SET value=? WHERE name=? AND hub=0', (value, name))
            client = Client(self, session)
            observer = self.peer(hub, cert, False)
            observer.login(b'NcdcObserver', 101, welcome=WELCOME)
            if secure:
                # Both listeners exist; use the explicitly selected TLS endpoint.
                import re
                hub.wait_for(lambda: b'ADCS listening on' in hub.log)
                port = int(re.search(rb'ADCS listening on 127\.0\.0\.1:(\d+)', hub.log)[1])
            else:
                port = hub.endpoints[0][1]
            scheme = b'adcs' if secure else b'adc'
            start = len(client.screen)
            client.command(b'/open authenticated ' + scheme + b'://127.0.0.1:' + str(port).encode() + b'/')
            client.wait_screen(b'password', start, hub)
            start = len(client.screen)
            client.command(b'/password Wrong ADC password!')
            client.wait_screen(b'authentication failed', start, hub)
            start = len(client.screen)
            client.command(b'/reconnect')
            client.wait_screen(b'password', start, hub)
            client.command(b'/password ' + PASSWORD)
            client.wait(lambda: hub.log.count(b'event=login_ready') == 2, hub)
            client.wait_screen(b'authentication ready.', hub=hub)
            self.assertTrue(observer.line().startswith(b'BINF '))
            client.command(b'NcdcAuthenticatedChat')
            expected = b'NcdcAuthenticatedChat'
            observer.peer.settimeout(0.05)
            received = bytearray()
            deadline = time.monotonic() + 5
            while expected not in received:
                self.assertLess(time.monotonic(), deadline, 'Authenticated ncdc chat missing')
                client.poll(hub)
                try:
                    received.extend(observer.peer.recv(4096))
                except socket.timeout:
                    pass
            client.command(b'/quit')
            client.process.wait(timeout=3)
            client.close()
            hub.stop()
            for value in (PASSWORD, b'HPAS', b'IGPA', b'NcdcAuthenticatedChat'):
                self.assertNotIn(value, hub.log)


if __name__ == '__main__':
    unittest.main()
