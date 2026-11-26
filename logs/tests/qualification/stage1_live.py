"""Stage I loopback qualification, using the project's ncdc and hub fixtures."""
import base64
from collections import Counter
from concurrent.futures import ThreadPoolExecutor
from contextlib import closing
from datetime import datetime, timedelta, timezone
import hashlib
import os
from pathlib import Path
import re
import socket
import sqlite3
import ssl
import subprocess
import sys
import tempfile
import time
import unittest

source, binary, ncdc = map(str, sys.argv[1:4])
sys.argv[1:] = [binary, source, source, ncdc, *sys.argv[4:]]
sys.path.insert(0, str(Path(source) / 'tests'))

from adc_authentication_process_tests import Client, Hub, Peer
from api_support import enable_api, fetch_status
from stage4_perf_probe import process_cpu_seconds
from tls_support import TLS_READY

WELCOME = b'stage one ready.'
MAX_CLIENT_WAIT = 6


def openssl(*args):
    return subprocess.run(['openssl', *map(str, args)], check=True, timeout=15,
                          capture_output=True).stdout


def make_ca(folder, name):
    cert, key = folder / (name + '.pem'), folder / (name + '.key')
    openssl('req', '-x509', '-newkey', 'rsa:2048', '-nodes', '-days', 2,
            '-subj', '/CN=Stage I ephemeral CA',
            '-addext', 'basicConstraints=critical,CA:TRUE',
            '-addext', 'keyUsage=critical,keyCertSign,cRLSign',
            '-keyout', key, '-out', cert)
    return cert, key


def make_leaf(folder, name, ca, dates=()):
    cert, key = folder / (name + '.pem'), folder / (name + '.key')
    csr, ext = folder / (name + '.csr'), folder / (name + '.ext')
    openssl('req', '-new', '-newkey', 'rsa:2048', '-nodes',
            '-subj', '/CN=localhost', '-keyout', key, '-out', csr)
    ext.write_text('basicConstraints=critical,CA:FALSE\n'
                   'keyUsage=critical,digitalSignature,keyEncipherment\n'
                   'extendedKeyUsage=serverAuth\nsubjectAltName=DNS:localhost\n')
    openssl('x509', '-req', '-in', csr, '-CA', ca[0], '-CAkey', ca[1],
            '-extfile', ext, '-out', cert, *dates)
    return cert, key


def keyprint(cert):
    der = ssl.PEM_cert_to_DER_cert(cert.read_text())
    digest = hashlib.sha256(der).digest()
    # Verify independently via OpenSSL read of the local certificate fixture.
    openssl_digest = openssl('x509', '-in', cert, '-noout', '-sha256', '-fingerprint')
    assert bytes.fromhex(openssl_digest.decode().strip().split('=', 1)[1].replace(':', '')) == digest
    return base64.b32encode(digest).rstrip(b'=').decode()


class StageOne(unittest.TestCase):
    def stage(self):
        root = tempfile.TemporaryDirectory(prefix='stage1-')
        self.addCleanup(root.cleanup)
        return Path(root.name)

    def start(self, folder, host='127.0.0.1', port=0, leaf=None, api=False, mixed=False):
        folder.mkdir(mode=0o700)
        ini = folder / 'hub.ini'
        requested = f'{host}:{port}'
        network = f'listen={requested}'
        tls = ''
        if leaf:
            network = f'adcs_listen={requested}'
            if mixed:
                network += '\nlisten=127.0.0.1:0'
            tls = f'[tls]\ncertificate={leaf[0]}\nprivate_key={leaf[1]}\n'
        ini.write_text('[network]\n' + network + '\n' + tls +
                       '[chat]\nwelcome_message=Stage one ready.\n'
                       '[security]\nmax_accepts_per_minute=240\n'
                       'max_connections_per_ip=4\n[users]\nmax_sessions=2\nmax_users=2\n')
        auth = enable_api(ini, folder) if api else None
        options = {'ready_pattern': TLS_READY} if leaf else {}
        hub = Hub(self, '--config', str(ini), '--log-connections', 'on', **options)
        return hub, auth

    def open_url(self, client, hub, scheme, pin=None):
        host, port = hub.endpoints[0]
        if ':' in host:
            host = '[' + host + ']'
        url = f'{scheme}://{host}:{port}/' + (f'?kp=SHA256/{pin}' if pin else '')
        mark = len(client.screen)
        client.command(f'/open qa {url}'.encode())
        return mark

    def wait_text(self, client, marker, mark=0, hub=None):
        try:
            client.wait(lambda: marker in client.screen[mark:].lower(), hub, timeout=MAX_CLIENT_WAIT)
        except AssertionError as error:
            raise AssertionError(f'ncdc did not show expected fixture marker {marker!r}') from error

    def new_client(self, folder, nick):
        client = Client(self, folder)
        client.command(b'/set nick ' + nick)
        self.wait_text(client, b'your new nick')
        return client

    def ready(self, client, hub, mark):
        self.wait_text(client, WELCOME, mark, hub)

    def quit(self, client):
        client.command(b'/quit')
        self.assertEqual(client.process.wait(timeout=3), 0)
        client.close()

    def test_certificate_policy(self):
        root = self.stage()
        ca = make_ca(root, 'trusted')
        other = make_ca(root, 'other')
        old_leaf = make_leaf(root, 'old', ca)
        new_leaf = make_leaf(root, 'new', ca)
        hub, _ = self.start(root / 'first', leaf=old_leaf, mixed=True)
        endpoint = hub.endpoints[0]

        def ca_probe(authority, hostname, code):
            context = ssl.create_default_context(cafile=str(authority))
            with socket.create_connection(endpoint, timeout=3) as raw:
                if code:
                    with self.assertRaises(ssl.SSLCertVerificationError) as caught:
                        context.wrap_socket(raw, server_hostname=hostname)
                    self.assertEqual(caught.exception.verify_code, code)
                else:
                    with context.wrap_socket(raw, server_hostname=hostname) as peer:
                        self.assertEqual(peer.getpeercert()['subjectAltName'], (('DNS', 'localhost'),))
                        adc = Peer(self, peer)
                        adc.login(b'CAclient', 98, welcome=b'IMSG Stage\\sone\\sready.')
                        print(f'PASS CA/SAN CA-signed DNS:localhost, {peer.version()}, ADC NORMAL', flush=True)

        ca_probe(ca[0], 'localhost', 0)
        ca_probe(ca[0], '127.0.0.1', 64)
        ca_probe(other[0], 'localhost', 20)
        print('PASS CA/SAN rejects IP SAN mismatch (64) and untrusted CA (20)', flush=True)

        for name, start, end, code in (('expired', -4, -2, 10), ('future', 2, 4, 9)):
            now = datetime.now(timezone.utc)
            fmt = lambda days: (now + timedelta(days=days)).strftime('%Y%m%d%H%M%SZ')
            leaf = make_leaf(root, name, ca,
                             ('-not_before', fmt(start), '-not_after', fmt(end)))
            checked = subprocess.run(['openssl', 'verify', '-CAfile', str(ca[0]), '-verify_hostname',
                                      'localhost', str(leaf[0])], capture_output=True, timeout=5)
            self.assertEqual(checked.returncode, 2)
            self.assertIn(f'error {code} at 0 depth'.encode(), checked.stderr)
            server_tls = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
            server_tls.load_cert_chain(*map(str, leaf))
            client_tls = ssl.create_default_context(cafile=str(ca[0]))
            with socket.socket() as listener, ThreadPoolExecutor(max_workers=1) as worker:
                listener.bind(('127.0.0.1', 0))
                listener.listen(1)
                listener.settimeout(3)

                def server_handshake():
                    raw, _ = listener.accept()
                    with raw:
                        raw.settimeout(3)
                        with self.assertRaises(ssl.SSLError):
                            server_tls.wrap_socket(raw, server_side=True)

                in_progress = worker.submit(server_handshake)
                with socket.create_connection(listener.getsockname(), timeout=3) as raw:
                    with self.assertRaises(ssl.SSLCertVerificationError) as caught:
                        client_tls.wrap_socket(raw, server_hostname='localhost')
                    self.assertEqual(caught.exception.verify_code, code)
                in_progress.result(timeout=5)
            started = subprocess.run([binary, '--adcs-listen', '127.0.0.1:0',
                                      '--tls-cert', str(leaf[0]), '--tls-key', str(leaf[1])],
                                     capture_output=True, timeout=5)
            self.assertEqual(started.returncode, 1)
            self.assertIn(b'TLS certificate is not valid at startup', started.stderr)
            self.assertNotIn(b'listening on', started.stderr)
            print(f'PASS {name} certificate: OpenSSL verify and live TLS reject ({code}), hub refuses startup before bind', flush=True)

        old_pin = keyprint(old_leaf[0])
        pinned = self.new_client(root / 'pinned', b'QAPinned')
        self.ready(pinned, hub, self.open_url(pinned, hub, 'adcs', old_pin))
        self.quit(pinned)
        wrong_pin = ('A' if old_pin[0] != 'A' else 'B') + old_pin[1:]

        def rejected(active_hub, folder, pin):
            client = self.new_client(folder, b'QAPinRejected')
            mark = self.open_url(client, active_hub, 'adcs', pin)
            self.wait_text(client, b'old keyprint:', mark, active_hub)
            self.wait_text(client, b'new keyprint:', mark, active_hub)
            self.assertNotIn(WELCOME, client.screen[mark:].lower())
            self.quit(client)

        rejected(hub, root / 'wrong', wrong_pin)
        hub.stop()
        self.assertNotIn(b'transport=ADC event=accepted', hub.log)
        self.assertEqual(hub.log.count(b'transport=ADCS event=accepted'), 5)
        self.assertEqual(hub.log.count(b'transport=ADCS event=login_ready'), 2)

        rotated, _ = self.start(root / 'rotated', port=endpoint[1], leaf=new_leaf, mixed=True)
        rejected(rotated, root / 'stale', old_pin)
        fresh = self.new_client(root / 'fresh', b'QAFresh')
        self.ready(fresh, rotated, self.open_url(fresh, rotated, 'adcs', keyprint(new_leaf[0])))
        self.quit(fresh)
        rotated.stop()
        self.assertNotIn(b'transport=ADC event=accepted', rotated.log)
        self.assertEqual(rotated.log.count(b'transport=ADCS event=accepted'), 2)
        self.assertEqual(rotated.log.count(b'transport=ADCS event=login_ready'), 1)
        print('PASS ncdc KEYP matches preverified leaf despite IP SAN mismatch, wrong and stale pins refuse, new pin logs in', flush=True)
        print('PASS ncdc pin failures: ADCS TLS handshakes only, no extra attempts or ADC logins', flush=True)

    def test_two_ncdc(self):
        root = self.stage()
        ca = make_ca(root, 'trusted')
        leaf = make_leaf(root, 'chat', ca)
        shared = root / 'share'
        shared.mkdir()
        document = shared / 'fixture-stage-file.txt'
        video = shared / 'fixture-stage-video.webm'
        document.write_bytes(b'Local Stage I document fixture\n' * 512)
        video.write_bytes(b'Local Stage I extension-filter fixture\n' * 512)
        for index, (scheme, host) in enumerate((('adc', '127.0.0.1'), ('adcs', '127.0.0.1'),
                                                ('adc', '[::1]'), ('adcs', '[::1]'))):
            with self.subTest(scheme=scheme, host=host):
                pin = keyprint(leaf[0]) if scheme == 'adcs' else None
                hub, _ = self.start(root / f'hub-{index}', host, leaf=leaf if pin else None)
                alice = self.new_client(root / f'alice-{index}', b'QAAlice')
                bob_root = root / f'bob-{index}'
                bob = self.new_client(bob_root, b'QABob')
                alice.command(b'/set local_address 127.0.0.1,::1')
                alice.command(b'/set active_ip local')
                alice.command(b'/set sudp_policy disabled')
                alice.command(b'/set active true')
                bob.command(f'/share QAfiles {shared}'.encode())
                deadline = time.monotonic() + 15
                while True:
                    with closing(sqlite3.connect(bob_root / 'db.sqlite3')) as db:
                        indexed, = db.execute('SELECT COUNT(*) FROM hashfiles WHERE filename IN (?, ?)',
                                              (str(document), str(video))).fetchone()
                    if indexed == 2:
                        break
                    self.assertLess(time.monotonic(), deadline, 'Share was not indexed')
                    bob.poll()
                self.ready(alice, hub, self.open_url(alice, hub, scheme, pin))
                self.ready(bob, hub, self.open_url(bob, hub, scheme, pin))

                def message(sender, receiver, command, marker, private_tab=None):
                    mark = len(receiver.screen)
                    sender.command(command)
                    if private_tab:
                        sender.poll(hub)
                        receiver.command(b'/msg ' + private_tab)
                    self.wait_text(receiver, marker, mark, hub)

                message(alice, bob, b'PublicHelloA', b'publichelloa')
                message(bob, alice, b'PublicHelloB', b'publichellob')
                message(alice, bob, b'/msg QABob StagePrivateA', b'stageprivatea', b'QAAlice')
                alice.command(b'/open qa')
                bob.command(b'/open qa')
                message(bob, alice, b'/msg QAAlice StagePrivateB', b'stageprivateb', b'QABob')
                alice.command(b'/open qa')
                mark = len(alice.screen)
                alice.command(b'/search -hub -t doc fixture-stage-file')
                self.wait_text(alice, document.name.encode(), mark, hub)
                # Search results have list bindings. Alt+2 returns to the hub chat tab.
                alice.command(b'\x1b2')
                alice.command(b'/hset active false')
                mark = len(alice.screen)
                alice.command(b'/search -hub -t video fixture-stage-video')
                self.wait_text(alice, video.name.encode(), mark, hub)
                alice.command(b'\x1b2')
                self.quit(alice)
                self.quit(bob)
                hub.stop()
                self.assertEqual(hub.log.count(b'event=login_ready'), 2)
                print(f'PASS two real ncdc {scheme.upper()} on {host}: NORMAL, public and private chat both ways, active doc and passive video search results', flush=True)

    def test_bounded_stability(self):
        root = self.stage()
        alice = self.new_client(root / 'alice', b'QAAlice')
        bob = self.new_client(root / 'bob', b'QABob')
        hub, auth = self.start(root / 'before', api=True)
        pid = hub.process.pid
        baseline_avail = int(next(line.split()[1] for line in Path('/proc/meminfo').read_text().splitlines()
                                  if line.startswith('MemAvailable:')))
        start = time.monotonic()
        cpu_start = process_cpu_seconds(pid)
        samples = []

        def snapshot(active):
            deadline = time.monotonic() + 4
            while True:
                stats = fetch_status(*auth)['network']
                if stats['active_connections'] == active:
                    break
                self.assertLess(time.monotonic(), deadline, 'Active connection count did not settle')
                if alice.master is not None:
                    alice.poll(hub)
                if bob.master is not None:
                    bob.poll()
                time.sleep(0.01)
            status = Path(f'/proc/{pid}/status').read_text()
            memory = [int(re.search(rf'^{name}:\s+(\d+) kB', status, re.M)[1])
                      for name in ('VmRSS', 'VmSize', 'VmHWM')]
            fds = list(Path(f'/proc/{pid}/fd').iterdir())
            count = len(fds)
            samples.append([*memory, count])
            return stats

        snapshot(0)
        self.ready(alice, hub, self.open_url(alice, hub, 'adc'))
        snapshot(1)
        alice.command(b'/disconnect')
        snapshot(0)

        for _ in range(20):
            mark = len(alice.screen)
            alice.command(b'/reconnect')
            self.ready(alice, hub, mark)
            snapshot(1)
            alice.command(b'/disconnect')
            snapshot(0)

        self.ready(alice, hub, self.open_url(alice, hub, 'adc'))
        snapshot(1)
        bob.command(b'/open -n qa')
        bob.command(b'/hset nick QAAlice')
        mark = self.open_url(bob, hub, 'adc')
        for cycle in range(10):
            self.wait_text(bob, b'nickname already in use', mark, hub)
            snapshot(1)
            mark = len(bob.screen)
            bob.command(b'/hset nick QABob')
            bob.command(b'/reconnect')
            self.ready(bob, hub, mark)
            snapshot(2)
            bob.command(b'/disconnect')
            snapshot(1)
            if cycle != 9:
                mark = len(bob.screen)
                bob.command(b'/hset nick QAAlice')
                bob.command(b'/reconnect')

        self.ready(bob, hub, self.open_url(bob, hub, 'adc'))
        stats = snapshot(2)
        mark = len(alice.screen)
        alice.command(b'+kick QABob')
        self.wait_text(alice, b'kick denied', mark, hub)
        snapshot(2)
        port = hub.endpoints[0][1]
        with socket.create_connection(hub.endpoints[0], timeout=3) as excess:
            self.assertEqual(excess.recv(1), b'')
        self.assertEqual(snapshot(2)['accepted_connections'], stats['accepted_connections'])
        self.assertEqual(snapshot(2)['rejected_connections'], stats['rejected_connections'] + 1)
        alice.command(b'/disconnect')
        bob.command(b'/disconnect')
        final = snapshot(0)
        self.assertEqual(final['accepted_connections'], final['closed_connections'])
        self.assertEqual(final['peak_connections'], 2)
        self.assertEqual(samples[0][3], samples[-1][3])
        cpu = process_cpu_seconds(pid) - cpu_start
        wall = time.monotonic() - start
        hub.stop()
        self.assertNotIn(auth[1].encode(), hub.log)
        before_counts = Counter(re.findall(rb'\[(WARN|ERROR)\]', hub.log))
        print('PASS 20 connect/disconnect cycles, 10 reconnects after nickname rejection, guest kick denied, session cap and cleanup', flush=True)
        print(f'Before restart: CPU={cpu:.3f}s wall={wall:.3f}s average={100*cpu/wall:.2f}% of 1 CPU; host MemAvailable={baseline_avail} KiB', flush=True)
        print(f'Samples [RSS KiB, virtual size KiB, kernel RSS high-water KiB, FD]: idle={samples[0]}, peak={list(map(max, zip(*samples)))}, final={samples[-1]}', flush=True)
        print('Network ' + ' '.join(f'{name}={final[name]}' for name in
              ('accepted_connections', 'closed_connections', 'rejected_connections', 'active_connections', 'peak_connections')), flush=True)
        print(f'Logs WARN={before_counts[b"WARN"]} ERROR={before_counts[b"ERROR"]}', flush=True)
        warning_types = Counter(line.split(b'[WARN] ', 1)[1].decode()
                                for line in hub.log.splitlines() if b'[WARN] ' in line)
        for message, count in sorted(warning_types.items()):
            print(f'WARN category ({count}): {message}', flush=True)
        self.assertEqual(before_counts[b'ERROR'], 0)

        hub, auth = self.start(root / 'after', port=port, api=True)
        pid = hub.process.pid
        self.ready(alice, hub, self.open_url(alice, hub, 'adc'))
        self.ready(bob, hub, self.open_url(bob, hub, 'adc'))
        snapshot(2)
        self.quit(alice)
        self.quit(bob)
        final = snapshot(0)
        self.assertEqual(final['accepted_connections'], 2)
        self.assertEqual(final['closed_connections'], 2)
        hub.stop()
        self.assertNotIn(auth[1].encode(), hub.log)
        after_counts = Counter(re.findall(rb'\[(WARN|ERROR)\]', hub.log))
        self.assertFalse(after_counts)
        print(f'PASS same-port restart: two simultaneous ncdc sessions ready, accepted=2 closed=2 active=0 WARN=0 ERROR=0; final metrics={samples[-1]}', flush=True)


if __name__ == '__main__':
    unittest.main()
