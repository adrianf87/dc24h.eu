"""Maintenance HTTP on an isolated migrated database supplied by its parent."""
import http.client
import json
from pathlib import Path
import re
import selectors
import signal
import socket
import stat
import subprocess
import sys
import time
from urllib.parse import urlencode


def exercise(binary, seed, config, db_socket, password, token, api_socket):
    seeded = subprocess.run([seed, str(db_socket), str(password)], capture_output=True, text=True, timeout=90)
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
        grant = {'value': ''}
        session = {'value': ''}
        csrf = {'value': ''}
        def request(method, target_path, data=None, authorized=True, delay=False, use_grant=True, grant_header=None, bind=True, session_override=None, csrf_override=None):
            if delay:
                time.sleep(1.05)
            body = urlencode(data or {}).encode() if data is not None else b''
            with socket.socket(socket.AF_UNIX) as peer:
                peer.settimeout(15)
                peer.connect(str(api_socket))
                headers = f'{method} {target_path} HTTP/1.1\r\nHost: localhost\r\n'
                if authorized:
                    headers += f'Authorization: Bearer {token}\r\n'
                header_value = grant_header
                if header_value is None and authorized and use_grant and grant['value']:
                    header_value = grant['value']
                if header_value is not None:
                    headers += f'X-DC24H-Admin-Grant: {header_value}\r\n'
                if target_path != '/api/v1/admin/access' and bind and header_value:
                    session_value = session['value'] if session_override is None else session_override
                    csrf_value = csrf['value'] if csrf_override is None else csrf_override
                    if session_value:
                        headers += f'X-DC24H-Session: {session_value}\r\n'
                    if csrf_value:
                        headers += f'X-DC24H-Csrf: {csrf_value}\r\n'
                if body:
                    headers += 'Content-Type: application/x-www-form-urlencoded\r\n'
                peer.sendall(headers.encode() + f'Content-Length: {len(body)}\r\n\r\n'.encode() + body)
                response = http.client.HTTPResponse(peer)
                response.begin()
                raw = response.read()
                assert token.encode() not in raw and proof.encode() not in raw and b'verifier' not in raw
                if grant['value'] and target_path != '/api/v1/admin/access':
                    assert grant['value'].encode() not in raw
                if csrf['value'] and target_path != '/api/v1/admin/access':
                    assert csrf['value'].encode() not in raw
                if session['value'] and target_path != '/api/v1/admin/access':
                    assert session['value'].encode() not in raw
                return response.status, json.loads(raw)
        assert request('GET', '/api/v1/accounts', authorized=False)[0] == 401
        assert request('GET', '/api/v1/admin', authorized=False)[1]['error'] == 'unauthorized'
        assert request('GET', '/api/v1/status', grant_header='ab', use_grant=False)[0] == 400
        assert request('GET', '/api/v1/admin')[1]['error'] == 'admin_grant_required'
        assert request('GET', '/api/v1/accounts')[1]['error'] == 'admin_grant_required'
        assert request('GET', '/api/v1/admin/access')[0] == 405
        assert request('POST', '/api/v1/admin/access', {'actor_id': actor, 'proof': 'not-the-password'}, use_grant=False)[0] == 403
        code, access = request('POST', '/api/v1/admin/access', {'actor_id': actor, 'proof': proof}, delay=True, use_grant=False)
        assert code == 200 and access['expires_in'] == 600 and access['actor']['id'] == actor
        assert access['actor']['role'] == 'administrator' and access['actor']['status'] == 'active'
        assert len(access['grant']) == 64 and access['grant'].islower()
        assert re.fullmatch(r'[0-9a-f]{32}', access['session'])
        assert re.fullmatch(r'[0-9a-f]{64}', access['csrf']) and access['csrf'] != access['grant']
        permissions = {item['key']: item for item in access['permissions']}
        assert permissions['configuration.manage'] == {'key': 'configuration.manage', 'granted': True, 'implemented': False}
        assert permissions['hub.manage'] == {'key': 'hub.manage', 'granted': False, 'implemented': False}
        assert len(access['permissions']) == 17 and len(access['routes']) == 12
        first = access['grant']
        code, replacement = request('POST', '/api/v1/admin/access', {'actor_id': actor, 'proof': proof}, delay=True, use_grant=False)
        assert code == 200 and replacement['grant'] != first and first.encode() not in json.dumps(replacement).encode()
        assert replacement['session'] != access['session'] and replacement['csrf'] != access['csrf']
        assert access['csrf'].encode() not in json.dumps(replacement).encode()
        grant['value'] = first
        session['value'] = access['session']
        csrf['value'] = access['csrf']
        assert request('GET', '/api/v1/admin')[1]['error'] == 'admin_grant_required'
        grant['value'] = replacement['grant']
        session['value'] = replacement['session']
        csrf['value'] = replacement['csrf']
        assert request('POST', '/api/v1/accounts', {'x': 'y'}, delay=True, bind=False)[1]['error'] == 'csrf_required'
        assert request('POST', '/api/v1/accounts', {'x': 'y'}, delay=True, csrf_override='ab' * 32)[1]['error'] == 'csrf_required'
        assert request('POST', '/api/v1/accounts', {'x': 'y'}, delay=True, csrf_override='zz')[0] == 400
        unbound_read = request('GET', '/api/v1/accounts', bind=False)
        assert unbound_read[1]['error'] == 'csrf_required' and 'WebAdmin' not in json.dumps(unbound_read[1])
        assert request('GET', '/api/v1/accounts', session_override='ab' * 16)[1]['error'] == 'csrf_required'
        assert request('GET', '/api/v1/admin', csrf_override='zz')[0] == 400
        assert request('GET', '/api/v1/admin')[0] == 200
        # Service control is a separate Owner-only route, not Administrator
        # management permission or a way to smuggle in a different unit.
        assert request('POST', '/api/v1/management/hub/start', {'confirm': 'START'},
                       authorized=False, delay=True)[0] == 401
        assert request('POST', '/api/v1/management/hub/start', {'confirm': 'START'}, delay=True)[0] == 403
        assert request('POST', '/api/v1/management/hub/restart', {'confirm': 'RESTART'}, delay=True)[0] == 403
        assert request('POST', '/api/v1/management/hub/stop?unit=other.service',
                       {'confirm': 'STOP'}, delay=True)[0] in (403, 404)
        code, core = request('GET', '/api/v1/admin')
        assert code == 200 and core['maintenance'] is True
        assert len(core['actions']) == 10 and [a['available'] for a in core['actions']] == [True]*6+[False]*2+[True]*2
        assert all(value == '0' for value in core['statistics'].values())
        code, page = request('GET', '/api/v1/accounts?after=0')
        assert code == 200 and page['maintenance'] and len(page['accounts']) == 2
        staff_code, staff = request('GET', '/api/v1/accounts?staff_after=0')
        assert staff_code == 200 and staff['staff_only'] is True
        assert [a['id'] for a in staff['accounts']] == [actor]
        assert request('GET', '/api/v1/accounts?staff_after=00')[0] == 400
        assert request('POST', '/api/v1/accounts/owner', {'password':proof}, delay=True)[0] == 400
        assert request('GET', '/api/v1/status', use_grant=False)[1]['server']['core_running'] is False
        policy = request('GET', '/api/v1/status')[1]['configuration']
        assert policy['mode'] == 'maintenance' and not policy['authentication']['enabled']
        assert policy['database']['enabled'] and policy['restart_required']
        # Configuration persistence is local, role-gated, revision-checked and audited.
        config_path = Path(config)
        initial_bytes = config_path.read_bytes()
        code, config_view = request('GET', '/api/v1/management/configuration')
        assert code == 200 and config_view['maintenance'] and config_view['read_only']
        config_revision = config_view['revision']
        previous_limit = config_view['saved']['network_max_connections']
        next_limit = '302' if previous_limit == '301' else '301'
        assert request('POST', '/api/v1/management/configuration/write',
                       {'revision': config_revision, 'network_max_connections': next_limit},
                       authorized=False, delay=True)[0] == 401
        assert request('POST', '/api/v1/management/configuration/write',
                       {'revision': config_revision, 'network_max_connections': next_limit},
                       bind=False, delay=True)[1]['error'] == 'csrf_required'
        invalid = request('POST', '/api/v1/management/configuration/write',
                          {'revision': config_revision, 'network_max_connections': '0'}, delay=True)
        assert invalid == (422, {'error': 'invalid_configuration'})
        assert config_path.read_bytes() == initial_bytes
        code, saved_config = request('POST', '/api/v1/management/configuration/write',
                                     {'revision': config_revision, 'network_max_connections': next_limit}, delay=True)
        assert code == 200 and saved_config['written'] and saved_config['restart_required']
        assert saved_config['revision'] != config_revision and saved_config['backup_available']
        backup = Path(str(config_path) + '.dc24h.bak')
        assert backup.read_bytes() == initial_bytes and stat.S_IMODE(backup.stat().st_mode) == 0o600
        assert request('GET', '/api/v1/management/configuration')[1]['saved']['network_max_connections'] == next_limit
        assert request('POST', '/api/v1/management/configuration/write',
                       {'revision': config_revision, 'network_max_connections': '400'}, delay=True)[0] == 409
        assert request('POST', '/api/v1/management/configuration/restore',
                       {'revision': config_revision}, delay=True)[0] == 409
        code, restored = request('POST', '/api/v1/management/configuration/restore',
                                 {'revision': saved_config['revision']}, delay=True)
        assert code == 200 and restored['written'] and restored['revision'] == config_revision
        assert config_path.read_bytes() == initial_bytes
        undo = Path(str(config_path) + '.dc24h.undo')
        assert stat.S_IMODE(undo.stat().st_mode) == 0o600 and next_limit.encode() in undo.read_bytes()
        current = request('GET', '/api/v1/accounts/' + target)[1]['account']
        assert current['status'] == 'active'
        stale = { 'actor_id':actor, 'proof':proof, 'revision':'1' }
        assert request('POST', f'/api/v1/accounts/{target}/disable', stale, delay=True)[0] == 409
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
        payload = {'actor_id':actor, 'proof':proof, 'kind':'account', 'subject':target, 'reason':'Private account ban', 'expires_at':''}
        code, created_ban = request('POST', '/api/v1/bans', payload, delay=True)
        assert code == 200 and created_ban['restart_required'] and created_ban['ban']['subject'] == target
        code, bans = request('GET', '/api/v1/bans?after=0')
        assert code == 200 and len(bans['bans']) == 1 and bans['maintenance'] and bans['restart_required']
        assert request('GET', '/api/v1/bans', authorized=False)[0] == 401
        ban = created_ban['ban']
        code, revoked = request('POST', f"/api/v1/bans/{ban['id']}/revoke", {'actor_id':actor, 'proof':proof, 'revision':ban['revision']}, delay=True)
        assert code == 200 and revoked['ban']['revoked_at'] is not None and revoked['ban']['revision'] == '2'
        assert request('POST', f"/api/v1/bans/{ban['id']}/revoke", {'actor_id':actor, 'proof':proof, 'revision':ban['revision']}, delay=True)[0] == 409
        timed = {'actor_id':actor, 'proof':proof, 'kind':'ip', 'subject':'192.0.2.201', 'reason':'Temporary HTTP', 'duration_seconds':'3'}
        code, created = request('POST', '/api/v1/bans', timed, delay=True)
        assert code == 200, created
        record = created['ban']
        assert record['expires_at'] == record['created_at'] + 3 and abs(created['observed_at'] - time.time()) < 3
        code, active = request('GET', '/api/v1/bans?after=0&state=temporary')
        assert code == 200 and active['bans'] == [record] and isinstance(active['observed_at'], int)
        assert request('GET', '/api/v1/bans?state=permanent')[1]['bans'] == []
        for query in ('?state=invalid', '?after=00&state=active', '?after=0&state=active&state=expired', '?after=0&', '?state='):
            assert request('GET', '/api/v1/bans' + query)[0] == 400
        for patch in ({'duration_seconds':'0'}, {'duration_seconds':'01'}, {'duration_seconds':'31536001'}, {'duration_seconds':'-1'}, {'expires_at':''}, {'duration_seconds':'9223372036854775807'}):
            assert request('POST', '/api/v1/bans', {**timed, **patch}, delay=True)[0] == 400, patch
        code, expired = request('GET', '/api/v1/bans?after=0&state=expired')
        assert code == 200 and expired['observed_at'] >= record['expires_at'] and expired['bans'] == [record]
        assert request('GET', '/api/v1/bans?state=active')[1]['bans'] == []
        assert request('GET', '/api/v1/bans?state=revoked')[1]['bans'] == [revoked['ban']]
        assert request('GET', '/api/v1/audit')[0] == 405
        code, history = request('POST', '/api/v1/audit', {'actor_id': actor, 'proof': proof, 'after': '0'}, delay=True)
        assert code == 200 and history['maintenance'] is True and history['restart_required'] is True
        recorded = [event['action'] for event in history['events']]
        assert 'accounts.create' in recorded and 'bans.create' in recorded and 'bans.revoke' in recorded
        assert 'configuration.write' in recorded and 'configuration.restore' in recorded
        assert all(event['outcome'] == 'succeeded' for event in history['events']
                   if event['target_kind'] != 'configuration')
        assert any(event['action'] == 'configuration.restore' and event['outcome'] == 'failed'
                   for event in history['events'])
        assert any(event['action'] == 'configuration.restore' and event['outcome'] == 'succeeded'
                   for event in history['events'])
        assert proof not in json.dumps(history) and 'Private account ban' not in json.dumps(history) and 'Temporary HTTP' not in json.dumps(history)
        assert request('POST', '/api/v1/audit', {'actor_id': target, 'proof': proof, 'after': '0'}, delay=True)[0] == 403
        assert request('POST', '/api/v1/audit', {'actor_id': actor, 'proof': proof, 'after': '00'}, delay=True)[0] == 400
        before = request('GET', '/api/v1/admin')[1]['statistics']
        assert int(before['attempts']) > 0 and int(before['succeeded']) >= 6 and int(before['refused']) >= 4
        assert request('POST', '/api/v1/admin', {'actor_id': actor, 'proof': proof}, delay=True)[0] == 405
        assert request('POST', '/api/v1/accounts', {'x':'y'})[0] == 429
        assert request('GET', '/api/v1/admin')[1]['statistics'] == before
        child.send_signal(signal.SIGTERM)
        out, err = child.communicate(timeout=5)
        assert child.returncode == 0 and not api_socket.exists()
        for secret in (proof, 'Created account password!', 'Replacement account password!', 'Final account password!', token):
            assert secret.encode() not in logs + out + err
    finally:
        if child.poll() is None:
            child.kill()
        child.communicate(timeout=5)
