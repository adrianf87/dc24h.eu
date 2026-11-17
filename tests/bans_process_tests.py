"""Admission against persisted exact IP/CID bans in disposable MariaDB."""
import selectors
import signal
import socket
import subprocess
import time
from pathlib import Path
from adc_login_process_tests import Peer, pair
import unittest


def exercise(binary, config, folder, sql):
    owner=sql("SELECT id FROM dc24h.dc24h_accounts WHERE name='BanOwner'")
    cid=pair(91)[1].decode();temporary=pair(93)[1].decode();expires=int(time.time())+3
    sql(f"INSERT INTO dc24h.dc24h_bans(kind,subject,reason,created_at,created_by) VALUES ('ip','127.0.0.2','PrivateIpReason',1,{owner}),('cid','{cid}','PrivateCidReason',1,{owner});")
    sql(f"INSERT INTO dc24h.dc24h_bans(kind,subject,reason,created_at,expires_at,created_by) VALUES ('cid','{temporary}','TemporaryReason',1,{expires},{owner});")
    child=subprocess.Popen([binary,'--config',str(config),'--log-connections','on'],stdout=subprocess.PIPE,stderr=subprocess.PIPE,bufsize=0)
    log=bytearray();case=unittest.TestCase()
    try:
        with selectors.DefaultSelector() as ready:
            ready.register(child.stderr,selectors.EVENT_READ);deadline=time.monotonic()+10
            while b'Network listening on ' not in log:
                left=deadline-time.monotonic();assert left>0 and ready.select(left),'Ban hub startup timed out'
                part=child.stderr.read(8192);assert part,'Ban hub failed to start';log.extend(part)
        import re
        port=int(re.search(rb'Network listening on 127\.0\.0\.1:(\d+)',log)[1])
        with socket.create_connection(('127.0.0.1',port),source_address=('127.0.0.2',0),timeout=3) as denied:
            try:
                denied.sendall(b'HSUP ADBASE ADTIGR\n');assert denied.recv(1)==b''
            except (ConnectionResetError,BrokenPipeError):
                pass
        with socket.create_connection(('127.0.0.1',port),timeout=3) as wire:
            client=Peer(case,wire);client.handshake();client.send(client.identity(b'BlockedCID',91))
            assert client.line()==b'ISTA 231 Access\\sblocked'
            assert client.line()==b'IQUI '+client.sid+b' TL-1 MSAccess\\sblocked'
            assert wire.recv(1)==b''
        with socket.create_connection(('127.0.0.1',port),timeout=3) as wire:
            Peer(case,wire).login(b'AllowedAfterBan',92)
        with socket.create_connection(('127.0.0.1',port),timeout=3) as wire:
            client=Peer(case,wire);client.handshake();client.send(client.identity(b'TemporaryCID',93))
            assert client.line().startswith(b'ISTA 232 Access\\sblocked TL')
            assert client.line().startswith(b'IQUI '+client.sid+b' TL')
            assert wire.recv(1)==b''
        remaining=max(0,expires-time.time()+0.1)
        time.sleep(remaining)
        with socket.create_connection(('127.0.0.1',port),timeout=3) as wire:
            Peer(case,wire).login(b'TemporaryCID',93)
        child.send_signal(signal.SIGTERM);out,err=child.communicate(timeout=5)
        assert child.returncode==0
        for secret in (b'PrivateIpReason',b'PrivateCidReason',cid.encode()):assert secret not in log+out+err
    finally:
        if child.poll() is None:child.kill()
        child.communicate(timeout=5)
    sql("UPDATE dc24h.dc24h_bans SET revoked_at=UNIX_TIMESTAMP(),revoked_by="+owner+",revision=revision+1 WHERE revoked_at IS NULL")
