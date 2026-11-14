"""Isolated real C++ + Vite + Chromium integration; no host configuration/secrets."""
import http.client
import os
from pathlib import Path
import re
import secrets
import selectors
import signal
import socket
import subprocess
import sys
import tempfile
import time
import unittest

binary = str(Path(sys.argv[1]).resolve())
root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root.parent / 'tests'))
from adc_login_process_tests import Peer

with tempfile.TemporaryDirectory(prefix='dc24h-web-live-') as temp:
    folder = Path(temp)
    token = secrets.token_hex(32)
    credential = folder / 'admin.token'
    credential.write_text(token + '\n'); credential.chmod(0o600)
    endpoint = folder / 'admin.sock'
    config = folder / 'hub.ini'
    config.write_text('[network]\nlisten=127.0.0.1:0\n[chat]\nmax_text_bytes=64\n[api]\nenabled=on\nsocket=admin.sock\ntoken_file=admin.token\n')
    server = subprocess.Popen([binary, '--config', str(config)], stdout=subprocess.PIPE, stderr=subprocess.PIPE, bufsize=0)
    vite = None
    live = None
    peers = []
    try:
        log = bytearray(); deadline = time.monotonic() + 5
        with selectors.DefaultSelector() as ready:
            ready.register(server.stderr, selectors.EVENT_READ)
            while b'Network listening on' not in log:
                remaining = deadline - time.monotonic()
                assert remaining > 0 and ready.select(remaining), 'C++ startup timed out'
                chunk = os.read(server.stderr.fileno(), 8192); assert chunk, 'C++ startup failed'; log.extend(chunk)
        port = int(re.search(rb'Network listening on 127\.0\.0\.1:(\d+)', log)[1])
        case = unittest.TestCase()
        for index in range(2):
            peer = Peer(case, socket.create_connection(('127.0.0.1', port), timeout=3))
            peers.append(peer)
            peer.login(f'WebChatSecretUser{index}'.encode(), index + 1)
            for earlier in peers[:-1]:
                case.assertTrue(earlier.line().startswith(b'BINF ' + peer.sid + b' '))
        source, target = peers
        for kind in (b'BMSG', b'DMSG', b'EMSG'):
            frame = kind + b' ' + source.sid + (b' ' + target.sid if kind != b'BMSG' else b'') + b' WebChatSecretBody'
            source.send(frame)
            for recipient in (peers if kind != b'DMSG' else [target]):
                case.assertEqual(recipient.line(), frame)
        source.send(b'DMSG ' + source.sid + b' ZZZZ WebChatSecretMissing')
        case.assertTrue(source.line().startswith(b'ISTA 140 '))
        source.send(b'BMSG ' + source.sid + b' ' + b'WebChatSecret' * 8)
        case.assertTrue(source.line().startswith(b'ISTA 100 '))
        update = b'BINF ' + source.sid + b' SUTCP4'
        source.send(update)
        for peer in peers: case.assertEqual(peer.line(), update)
        offer = b'ECTM ' + source.sid + b' ' + target.sid + b' ADC/1.0 01515 WebTransferSecretToken'
        source.send(offer)
        for peer in peers: case.assertEqual(peer.line(), offer)
        reverse = b'DRCM ' + target.sid + b' ' + source.sid + b' ADCS/1.0 WebTransferSecretToken'
        target.send(reverse); case.assertEqual(source.line(), reverse)
        failure = b'DSTA ' + target.sid + b' ' + source.sid + b' 142 WebTransferSecretFailure PRADC/1.0 TOWebTransferSecretToken'
        target.send(failure); case.assertEqual(source.line(), failure)
        query = b'DSCH ' + source.sid + b' ' + target.sid + b' ANWebSearchSecret TOWebSearchSecretToken'
        source.send(query); case.assertEqual(target.line(), query)
        result = b'DRES ' + target.sid + b' ' + source.sid + b' FN/WebSearchSecret SI0 TR' + b'A'*39 + b' TOWebSearchSecretToken'
        target.send(result); case.assertEqual(source.line(), result)
        with socket.socket() as reserve:
            reserve.bind(('127.0.0.1', 0)); webport = reserve.getsockname()[1]
        with (folder / 'vite.log').open('w') as output:
            vite = subprocess.Popen(['node', 'node_modules/vite/bin/vite.js', '--port', str(webport)], cwd=root,
                env={**os.environ, 'DC24H_API_SOCKET': str(endpoint), 'DC24H_API_TOKEN_FILE': str(credential)}, stdout=output, stderr=output)
            deadline = time.monotonic() + 10
            while True:
                assert vite.poll() is None and time.monotonic() < deadline, 'Vite startup failed'
                try:
                    client = http.client.HTTPConnection('127.0.0.1', webport, timeout=1)
                    client.request('GET', '/'); response = client.getresponse(); response.read(); client.close()
                    if response.status == 200:
                        break
                except OSError:
                    time.sleep(0.05)
            live = subprocess.Popen(['node', 'tests/live-panel.mjs', 'http://127.0.0.1:' + str(webport) + '/', str(credential)], cwd=root,
                stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            live.stdin.write(token + '\n'); live.stdin.close(); live.stdin = None
            with selectors.DefaultSelector() as ready:
                ready.register(live.stdout, selectors.EVENT_READ)
                assert ready.select(30), 'Browser baseline timed out'
                marker = live.stdout.readline().strip()
                if marker != 'SEARCH_MONITOR_READY':
                    live_out, live_err = live.communicate(timeout=10)
                    raise AssertionError('Browser did not establish its search baseline: ' + live_err.replace(token, '[redacted]'))
            for index in range(2):
                window = b'WebSearchSecretToken' + str(index).encode()
                query = b'DSCH ' + source.sid + b' ' + target.sid + b' ANWebSearchSecret TO' + window
                source.send(query); case.assertEqual(target.line(), query)
                result = b'DRES ' + target.sid + b' ' + source.sid + b' FN/WebSearchSecret SI0 TR' + b'A'*39 + b' TO' + window
                target.send(result); case.assertEqual(source.line(), result)
                # New traffic after the browser baseline must produce observed
                # transfer rates through the real C++ API, not mocked JSON.
                source.send(offer)
                for peer in peers: case.assertEqual(peer.line(), offer)
                target.send(reverse); case.assertEqual(source.line(), reverse)
            live_out, live_err = live.communicate(timeout=40)
            assert live.returncode == 0, live_err.replace(token, '[redacted]')
            print(live_out.strip())
        server.send_signal(signal.SIGTERM); out, err = server.communicate(timeout=3)
        assert server.returncode == 0 and not endpoint.exists(), 'C++/API shutdown failed'
        assert token.encode() not in log + err + out, 'Credential leaked in C++ output'
        assert b'WebSearchSecret' not in log + err + out, 'Search data leaked in C++ output'
        assert b'WebChatSecret' not in log + err + out, 'Chat data leaked in C++ output'
        assert b'WebTransferSecret' not in log + err + out, 'Transfer data leaked in C++ output'
    finally:
        if live is not None and live.poll() is None:
            live.kill(); live.communicate(timeout=3)
        for peer in peers: peer.peer.close()
        if vite is not None:
            vite.terminate()
            try: vite.wait(timeout=3)
            except subprocess.TimeoutExpired: vite.kill(); vite.wait()
        if server.poll() is None: server.kill()
        server.communicate(timeout=3)
