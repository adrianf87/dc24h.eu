"""Real ncdc uses the remaining identity budget and sees a shared quota on reconnect."""
import fcntl
import os
import pathlib
import pty
import select
import struct
import subprocess
import sys
import tempfile
import termios
import time
import unittest

from network_process_tests import Hub
from adc_login_process_tests import Peer,pair
from tls_support import credentials,tls_endpoint

NCDC=sys.argv.pop(1)
ENCRYPTED='--tls' in sys.argv
if ENCRYPTED:sys.argv.remove('--tls')

class NcdcNegotiationTest(unittest.TestCase):
    def test_failed_proof_normal_login_and_reconnect_quota(self):
        with tempfile.TemporaryDirectory(prefix='dc24h-ncdc-negotiate-') as directory:
            credentials(directory);ini=pathlib.Path(directory)/'hub.ini'
            ini.write_text('[network]\nlisten=127.0.0.1:0\nadcs_listen=127.0.0.1:0\n[tls]\n'
                'certificate=certificate.pem\nprivate_key=private.pem\n[security]\n'
                'max_identity_checks_per_minute=2\nmax_identity_checks_per_second=100000\nmax_pending_logins=1\n')
            hub=Hub(self,'--config',str(ini),'--log-connections','on')
            attacker=Peer(self,hub.connect());attacker.handshake()
            attacker.send(attacker.identity(b'FailedProof',1).replace(pair(1)[1],pair(2)[1]))
            self.assertTrue(attacker.line().startswith(b'ISTA 227 '))
            self.assertTrue(attacker.line().startswith(b'IQUI '+attacker.sid))
            self.assertEqual(attacker.peer.recv(1),b'')
            endpoint=tls_endpoint(hub) if ENCRYPTED else hub.endpoints[0]
            master,slave=pty.openpty();fcntl.ioctl(slave,termios.TIOCSWINSZ,struct.pack('HHHH',30,140,0,0))
            client=subprocess.Popen([NCDC,'-c',directory+'/session','-n'],stdin=slave,stdout=slave,stderr=slave,
                start_new_session=True,env={**os.environ,'TERM':'xterm','LC_ALL':'C.UTF-8'})
            os.close(slave);screen=bytearray()
            def wait_text(text):
                deadline=time.monotonic()+6
                while text not in screen:
                    self.assertIsNone(client.poll(),repr(screen[-3000:]))
                    self.assertLess(time.monotonic(),deadline,repr(screen[-5000:]))
                    if select.select([master],[],[],0.1)[0]:screen.extend(os.read(master,65536))
            try:
                wait_text(b'Welcome');os.write(master,b'/set nick dc24hSecurityTest\r')
                scheme='adcs' if ENCRYPTED else 'adc'
                os.write(master,f'/open security_test {scheme}://{endpoint[0]}:{endpoint[1]}/\r'.encode())
                hub.wait_for(lambda:hub.log.count(b'event=login_ready')==1)
                time.sleep(0.1);os.write(master,b'BudgetNormalProbe\r');wait_text(b'BudgetNormalProbe')
                os.write(master,b'/reconnect\r');wait_text(b'Identity verification rate limit exceeded')
                hub.wait_for(lambda:b'event=identity_limit' in hub.log)
                self.assertEqual(hub.log.count(b'event=login_ready'),1)
                os.write(master,b'/quit\r');deadline=time.monotonic()+4
                while client.poll() is None and time.monotonic()<deadline:
                    if select.select([master],[],[],0.1)[0]:
                        try:os.read(master,65536)
                        except OSError:break
                self.assertEqual(client.wait(timeout=2),0);hub.stop()
                for secret in (b'FailedProof',b'dc24hSecurityTest',b'BudgetNormalProbe',pair(1)[0]):self.assertNotIn(secret,hub.log)
                self.assertEqual(hub.log.count(b'event=identity_limit'),1)
                print(f'ncdc reaches NORMAL with remaining quota and observes reconnect identity limit via {scheme.upper()}')
            finally:
                if client.poll() is None:client.kill()
                client.wait();os.close(master)

if __name__=='__main__':unittest.main()
