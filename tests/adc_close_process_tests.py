"""Force a pending TCP/TLS write and verify the absolute terminal drain limit."""
import socket
import ssl
import tempfile
import time
import unittest

from network_process_tests import Hub
from adc_login_process_tests import Peer
from tls_support import credentials, tls_endpoint


class CloseDeadlineTest(unittest.TestCase):
    def test_blocked_output_cannot_extend_terminal_deadline(self):
        with tempfile.TemporaryDirectory(prefix='dc24h-close-') as directory:
            cert,key=credentials(directory)
            hub=Hub(self,cert,key)
            producer=Peer(self,hub.connect());producer.login(b'Producer',1)
            slow=[]
            for encrypted in (False,True):
                raw=socket.socket();self.addCleanup(raw.close)
                raw.setsockopt(socket.SOL_SOCKET,socket.SO_RCVBUF,1024)
                raw.settimeout(5)
                raw.connect(tls_endpoint(hub) if encrypted else hub.endpoints[0])
                peer=ssl.create_default_context(cafile=cert).wrap_socket(raw,server_hostname='localhost') if encrypted else raw
                self.addCleanup(peer.close)
                client=Peer(self,peer);client.login(b'SlowTLS' if encrypted else b'SlowTCP',3 if encrypted else 2)
                slow.append(client)
                producer.line()
            # Keep the sender below the memory quota while both recipients have
            # tiny receive windows and stop reading. No host sysctl is changed.
            started=time.monotonic()
            for _ in range(40):
                for target in slow:
                    producer.send(b'DSTA '+producer.sid+b' '+target.sid+b' 199 '+b'x'*2048)
                time.sleep(0.02)
            hub.wait_for(lambda:hub.log.count(b'event=close_timeout')==2,timeout=5)
            self.assertLess(time.monotonic()-started,5)
            self.assertNotIn(b'event=output_limit',hub.log)
            fresh=Peer(self,hub.connect());fresh.login(b'Fresh',4)
            self.assertNotIn(b'event=login_rejected',hub.log)
            hub.stop()
            self.assertEqual(hub.log.count(b'event=close_timeout'),2)
            self.assertNotIn(b'x'*30,hub.log)


if __name__=='__main__':
    unittest.main()
