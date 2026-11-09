"""Private API fixture shared by ncdc regressions; no external service required."""
import json
from pathlib import Path
import secrets
import socket


def enable_api(ini, directory):
    folder = Path(directory)
    token = secrets.token_hex(32)
    path = folder / 'admin.token'
    path.write_text(token + '\n', encoding='ascii')
    path.chmod(0o600)
    endpoint = folder / 'admin.sock'
    with Path(ini).open('a', encoding='utf-8') as output:
        output.write('\n[api]\nenabled=on\nsocket=' + str(endpoint) + '\ntoken_file=' + str(path) + '\n')
    return endpoint, token


def fetch_status(endpoint, token):
    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as peer:
        peer.settimeout(3)
        peer.connect(str(endpoint))
        peer.sendall(('GET /api/v1/status HTTP/1.1\r\nHost: local\r\nAuthorization: Bearer ' + token + '\r\n\r\n').encode())
        response = bytearray()
        while True:
            chunk = peer.recv(4096)
            if not chunk:
                break
            response.extend(chunk)
        head, body = bytes(response).split(b'\r\n\r\n', 1)
        assert head.startswith(b'HTTP/1.1 200 '), head
        return json.loads(body)
