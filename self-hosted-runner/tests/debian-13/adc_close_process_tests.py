"""Force a pending TCP/TLS write and verify the absolute terminal drain limit."""
import socket
import signal
import tempfile
import time
import unittest

from network_process_tests import Hub
from adc_login_process_tests import Peer
from tls_support import credentials, secure_client_context, tls_endpoint
from stage4_perf_probe import rss_kib


class CloseDeadlineTest(unittest.TestCase):
    def test_beta07_half_open_tls_admission_preserves_healthy_adc_and_bounded_drain(self):
        """Real isolated reactor under incomplete TLS clients, no global host tuning."""
        with tempfile.TemporaryDirectory(prefix='beta07-drain-') as directory:
            cert, key = credentials(directory)
            hub = Hub(self, cert, key)
            before = rss_kib(hub.process.pid)
            self.assertIsNotNone(before)
            clients = []
            try:
                for index in range(20):
                    peer = socket.create_connection(tls_endpoint(hub), timeout=3)
                    peer.settimeout(1)
                    if index % 2 == 0:
                        peer.sendall(b'\x16\x03')  # Incomplete TLS record, never valid auth.
                    clients.append(peer)
                healthy = Peer(self, hub.connect())
                healthy.login(b'PressureHealthy', 220)
                healthy.send(b'BMSG ' + healthy.sid + b' StillAlive')
                self.assertEqual(healthy.line(), b'BMSG ' + healthy.sid + b' StillAlive')
                after = rss_kib(hub.process.pid)
                self.assertIsNotNone(after)
                self.assertLessEqual(after - before, 64 * 1024,
                                     '20 stalled handshakes exceeded the disposable 64-MiB growth budget')
                hub.stop(signal.SIGTERM)
                self.assertEqual(hub.log.count(b'Network stopped; all TCP sockets closed'), 1)
                self.assertNotIn(b'PressureHealthy', hub.log)
            finally:
                for peer in clients:
                    peer.close()

    def test_beta06_bounded_drain_and_reconnect_across_three_lifetimes(self):
        """Three isolated SIGTERM lifetimes with queued ADC/ADCS and half-open TLS."""
        with tempfile.TemporaryDirectory(prefix='dc24h-restart-') as directory:
            cert, key = credentials(directory)
            for lifetime in range(3):
                with self.subTest(lifetime=lifetime):
                    hub = Hub(self, cert, key)
                    plain = Peer(self, hub.connect())
                    plain.login(b'ReconnectPlain', 210)
                    raw_tls = socket.create_connection(tls_endpoint(hub), timeout=3)
                    self.addCleanup(raw_tls.close)
                    tls = secure_client_context(cert).wrap_socket(raw_tls, server_hostname='localhost')
                    self.addCleanup(tls.close)
                    encrypted = Peer(self, tls)
                    encrypted.login(b'ReconnectTls', 211)
                    self.assertTrue(plain.line().startswith(b'BINF ' + encrypted.sid + b' '))
                    # The third peer opens TCP but never sends a ClientHello.
                    half_open = socket.create_connection(tls_endpoint(hub), timeout=3)
                    self.addCleanup(half_open.close)
                    for ordinal in range(3):
                        command = b'BSTA ' + plain.sid + b' 000 ShutdownQueued' + str(ordinal).encode()
                        plain.send(command)
                        self.assertEqual(plain.line(), command)
                        # Keep at least some outgoing replies unconsumed on TLS.
                    start = time.monotonic()
                    hub.stop(signal.SIGTERM)
                    self.assertLess(time.monotonic() - start, 2.0)
                    self.assertEqual(hub.log.count(b'Core stopped'), 1)
                    self.assertEqual(hub.log.count(b'Network stopped; all TCP sockets closed'), 1)
                    half_open.close()
                    tls.close()
                    plain.peer.close()

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
                peer=secure_client_context(cert).wrap_socket(raw,server_hostname='localhost') if encrypted else raw
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
