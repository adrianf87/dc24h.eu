"""Real ncdc observes a fatal duplicate-nick STA/own QUI and then logs in again."""
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
from adc_login_process_tests import Peer
from tls_support import credentials, connect_tls

NCDC=sys.argv.pop(1)
ENCRYPTED='--tls' in sys.argv
if ENCRYPTED:
    sys.argv.remove('--tls')


class NcdcStatusTest(unittest.TestCase):
    def test_duplicate_status_self_quit_and_recovery(self):
        with tempfile.TemporaryDirectory(prefix='dc24h-ncdc-status-') as directory:
            cert,key=credentials(directory)
            ini=pathlib.Path(directory)/'hub.ini'
            ini.write_text('[network]\nlisten=127.0.0.1:0\nadcs_listen=127.0.0.1:0\n'
                           '[tls]\ncertificate=certificate.pem\nprivate_key=private.pem\n')
            hub=Hub(self,'--config',str(ini),'--log-connections','on')
            owner=Peer(self,hub.connect());owner.login(b'dc24hStatusTest',17)
            observer=Peer(self,hub.connect());observer.login(b'Observer',18);owner.line()
            if ENCRYPTED:
                from tls_support import tls_endpoint
                endpoint=tls_endpoint(hub)
            else:
                endpoint=hub.endpoints[0]
            master,slave=pty.openpty()
            fcntl.ioctl(slave,termios.TIOCSWINSZ,struct.pack('HHHH',30,140,0,0))
            client=subprocess.Popen([NCDC,'-c',directory+'/session','-n'],stdin=slave,stdout=slave,stderr=slave,
                start_new_session=True,env={**os.environ,'TERM':'xterm','LC_ALL':'C.UTF-8'})
            os.close(slave)
            screen=bytearray()
            def wait_for_text(text):
                deadline=time.monotonic()+6
                while text not in screen:
                    self.assertIsNone(client.poll(),repr(screen[-3000:]))
                    self.assertLess(time.monotonic(),deadline,repr(screen[-5000:]))
                    if select.select([master],[],[],0.1)[0]:
                        screen.extend(os.read(master,65536))
            try:
                wait_for_text(b'Welcome')
                os.write(master,b'/set nick dc24hStatusTest\r')
                scheme='adcs' if ENCRYPTED else 'adc'
                os.write(master,f'/open status_test {scheme}://{endpoint[0]}:{endpoint[1]}/\r'.encode())
                wait_for_text(b'Nickname already in use')
                hub.wait_for(lambda:b'event=login_rejected' in hub.log)
                self.assertEqual(hub.log.count(b'event=login_ready'),2)
                # The rejected client must never appear in an existing list.
                owner.send(b'BINF '+owner.sid+b' DEAlive')
                self.assertIn(b'DEAlive',owner.line())
                self.assertIn(b'DEAlive',observer.line())
                owner.peer.close()
                self.assertEqual(observer.line(),b'IQUI '+owner.sid)
                os.write(master,b'/reconnect\r')
                hub.wait_for(lambda:hub.log.count(b'event=login_ready')==3)
                time.sleep(0.1)
                os.write(master,b'NormalAfterStatus\r')
                wait_for_text(b'NormalAfterStatus')
                os.write(master,b'/quit\r')
                deadline=time.monotonic()+4
                while client.poll() is None and time.monotonic()<deadline:
                    if select.select([master],[],[],0.1)[0]:
                        try:os.read(master,65536)
                        except OSError:break
                self.assertEqual(client.wait(timeout=2),0)
                hub.stop()
                self.assertEqual(hub.log.count(b'event=login_rejected'),1)
                self.assertNotIn(b'dc24hStatusTest',hub.log)
                self.assertNotIn(b'NormalAfterStatus',hub.log)
                print(f'ncdc displayed fatal STA/QUI reason and recovered to NORMAL via {scheme.upper()}')
            finally:
                if client.poll() is None:client.kill()
                client.wait();os.close(master)


if __name__=='__main__':
    unittest.main()
