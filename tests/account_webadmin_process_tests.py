"""Maintenance HTTP on an isolated migrated database supplied by its parent."""
import http.client
import json
from pathlib import Path
import re
import selectors
import signal
import socket
import subprocess
import sys
import time
from urllib.parse import urlencode


def exercise(binary, seed, config, db_socket, password, token, api_socket):
    seeded = subprocess.run([seed, str(db_socket), str(password)], capture_output=True, text=True, timeout=30)
    assert seeded.returncode == 0, seeded.stderr
    ids = re.search(r'actor=(\d+) target=(\d+)', seeded.stdout)
    assert ids, 'Missing account fixtures'
    actor, target = ids.groups()
    proof = 'WebAdmin operator password!'
    child = subprocess.Popen([binary, '--config', str(config)], stdout=subprocess.PIPE, stderr=subprocess.PIPE, bufsize=0)
    logs = bytearray()
    try:
        with selectors.DefaultSelector() as ready:
            ready.register(child.stderr, selectors.EVENT_READ)
            deadline = time.monotonic() + 10
            while b'Account maintenance API ready' not in logs:
                left = deadline - time.monotonic()
                assert left > 0 and ready.select(left), 'Maintenance startup timed out'
                chunk = child.stderr.read(4096)
                assert chunk, 'Maintenance startup failed'
                logs.extend(chunk)
        assert b'Network listening' not in logs and b'ADCS listening' not in logs
        def request(method, target_path, data=None, authorized=True, delay=False):
            if delay:
                time.sleep(1.05)
            body = urlencode(data or {}).encode() if data is not None else b''
            with socket.socket(socket.AF_UNIX) as peer:
                peer.settimeout(15)
                peer.connect(str(api_socket))
                headers = f'{method} {target_path} HTTP/1.1\r\nHost: localhost\r\n'
                if authorized:
                    headers += f'Authorization: Bearer {token}\r\n'
                if body:
                    headers += 'Content-Type: application/x-www-form-urlencoded\r\n'
                peer.sendall(headers.encode() + f'Content-Length: {len(body)}\r\n\r\n'.encode() + body)
                response = http.client.HTTPResponse(peer)
                response.begin()
                raw = response.read()
                assert token.encode() not in raw and proof.encode() not in raw and b'verifier' not in raw
                return response.status, json.loads(raw)
        assert request('GET', '/api/v1/accounts', authorized=False)[0] == 401
        code, page = request('GET', '/api/v1/accounts?after=0')
        assert code == 200 and page['maintenance'] and len(page['accounts']) == 2
        assert request('GET', '/api/v1/status')[1]['server']['core_running'] is False
        policy = request('GET', '/api/v1/status')[1]['configuration']
        assert policy['mode'] == 'maintenance' and not policy['authentication']['enabled']
        assert policy['database']['enabled'] and policy['restart_required']
        current = request('GET', '/api/v1/accounts/' + target)[1]['account']
        assert current['status'] == 'active'
        stale = { 'actor_id':actor, 'proof':proof, 'revision':'1' }
        assert request('POST', f'/api/v1/accounts/{target}/disable', stale)[0] == 409
        payload = {'actor_id':actor, 'proof':proof, 'revision':current['revision']}
        code, changed = request('POST', f'/api/v1/accounts/{target}/disable', payload, delay=True)
        assert code == 200 and changed['account']['status'] == 'disabled'
        assert request('POST', '/api/v1/accounts', {'actor_id':actor, 'proof':proof, 'name':'WebCreated', 'password':'Created account password!'}, delay=True)[0] == 200
        # Unique identity, proof and role guards survive HTTP composition.
        assert request('POST', '/api/v1/accounts', {'actor_id':actor, 'proof':proof, 'name':'WebCreated', 'password':'Created account password!'}, delay=True)[0] == 409
        current = changed['account']
        payload = {'actor_id':actor, 'proof':proof, 'revision':current['revision'], 'password':'Replacement account password!'}
        code, changed = request('POST', f'/api/v1/accounts/{target}/reset', payload, delay=True)
        assert code == 200 and changed['account']['status'] == 'disabled'
        payload = {'actor_id':actor, 'proof':proof, 'revision':changed['account']['revision']}
        code, changed = request('POST', f'/api/v1/accounts/{target}/activate', payload, delay=True)
        assert code == 200 and changed['account']['status'] == 'active'
        payload = {'proof':'Replacement account password!', 'password':'Final account password!', 'revision':changed['account']['revision']}
        assert request('POST', f'/api/v1/accounts/{target}/password', payload, delay=True)[0] == 200
        current = request('GET', '/api/v1/accounts/' + target)[1]['account']
        payload = {'actor_id':actor, 'proof':proof, 'revision':current['revision'], 'role':'vip'}
        code, changed = request('POST', f'/api/v1/accounts/{target}/role', payload, delay=True)
        assert code == 200 and changed['account']['role'] == 'vip' and changed['account']['status'] == 'disabled'
        payload = {'actor_id':actor, 'proof':proof, 'revision':changed['account']['revision'], 'role':'owner'}
        assert request('POST', f'/api/v1/accounts/{target}/role', payload, delay=True)[0] == 403
        payload['role'] = 'guest'
        assert request('POST', f'/api/v1/accounts/{target}/role', payload, delay=True)[0] == 403
        payload = {'actor_id':actor, 'proof':proof, 'revision':page['accounts'][0]['revision']}
        assert request('POST', f'/api/v1/accounts/{actor}/disable', payload, delay=True)[0] == 403
        assert request('POST', '/api/v1/accounts', {'x':'y'})[0] == 429
        child.send_signal(signal.SIGTERM)
        out, err = child.communicate(timeout=5)
        assert child.returncode == 0 and not api_socket.exists()
        for secret in (proof, 'Created account password!', 'Replacement account password!', 'Final account password!', token):
            assert secret.encode() not in logs + out + err
    finally:
        if child.poll() is None:
            child.kill()
        child.communicate(timeout=5)
