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

binary = str(Path(sys.argv[1]).resolve())
root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='dc24h-web-live-') as temp:
    folder = Path(temp)
    token = secrets.token_hex(32)
    credential = folder / 'admin.token'
    credential.write_text(token + '\n'); credential.chmod(0o600)
    endpoint = folder / 'admin.sock'
    config = folder / 'hub.ini'
    config.write_text('[network]\nlisten=127.0.0.1:0\n[api]\nenabled=on\nsocket=admin.sock\ntoken_file=admin.token\n')
    server = subprocess.Popen([binary, '--config', str(config)], stdout=subprocess.PIPE, stderr=subprocess.PIPE, bufsize=0)
    vite = None
    peer = None
    try:
        log = bytearray(); deadline = time.monotonic() + 5
        with selectors.DefaultSelector() as ready:
            ready.register(server.stderr, selectors.EVENT_READ)
            while b'Network listening on' not in log:
                remaining = deadline - time.monotonic()
                assert remaining > 0 and ready.select(remaining), 'C++ startup timed out'
                chunk = os.read(server.stderr.fileno(), 8192); assert chunk, 'C++ startup failed'; log.extend(chunk)
        port = int(re.search(rb'Network listening on 127\.0\.0\.1:(\d+)', log)[1])
        peer = socket.create_connection(('127.0.0.1', port), timeout=3)
        peer.sendall(b'HSUP ADBASE ADTIGR\n')
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
            result = subprocess.run(['node', 'tests/live-panel.mjs', 'http://127.0.0.1:' + str(webport) + '/', str(credential)], cwd=root,
                input=token + '\n', text=True, capture_output=True, timeout=40)
            assert result.returncode == 0, result.stderr.replace(token, '[redacted]')
            print(result.stdout.strip())
        server.send_signal(signal.SIGTERM); out, err = server.communicate(timeout=3)
        assert server.returncode == 0 and not endpoint.exists(), 'C++/API shutdown failed'
        assert token.encode() not in log + err + out, 'Credential leaked in C++ output'
    finally:
        if peer is not None: peer.close()
        if vite is not None:
            vite.terminate()
            try: vite.wait(timeout=3)
            except subprocess.TimeoutExpired: vite.kill(); vite.wait()
        if server.poll() is None: server.kill()
        server.communicate(timeout=3)
