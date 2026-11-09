"""Real ncdc on an isolated PTY/session, through a recording loopback relay.

Only initial HSUP parsing is in scope. The server sends no handshake response.
"""
import fcntl
import os
import pathlib
import pty
import select
import socket
import struct
import subprocess
import sys
import tempfile
import termios
import time
import unittest

from network_process_tests import Hub
from adc_process_tests import counters

NCDC = sys.argv.pop(1)


class NcdcSmokeTest(unittest.TestCase):
    def test_initial_sup_is_received_and_parsed(self):
        configuration = tempfile.TemporaryDirectory(prefix="dc24h-ncdc-config-")
        self.addCleanup(configuration.cleanup)
        ini = pathlib.Path(configuration.name) / "hub.ini"
        ini.write_text("[network]\nlisten=127.0.0.1:0\nmax_connections=8\n", encoding="utf-8")
        hub = Hub(self, "--config", str(ini), "--max-connections", "4", "--log-connections", "on")
        with tempfile.TemporaryDirectory(prefix="dc24h-ncdc-") as session, socket.socket() as relay:
            relay.bind(("127.0.0.1", 0))
            relay.listen(1)
            relay.setblocking(False)
            master, slave = pty.openpty()
            fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack("HHHH", 30, 120, 0, 0))
            process = subprocess.Popen(
                [NCDC, "-c", session, "-n"],
                stdin=slave, stdout=slave, stderr=slave,
                env={**os.environ, "TERM": "xterm", "LC_ALL": "C.UTF-8"}, start_new_session=True
            )
            os.close(slave)
            screen = bytearray()
            try:
                deadline = time.monotonic() + 5
                while b"Welcome" not in screen:
                    self.assertIsNone(process.poll(), "ncdc exited before starting")
                    self.assertLess(time.monotonic(), deadline, "ncdc UI did not start")
                    if select.select([master], [], [], 0.1)[0]:
                        screen.extend(os.read(master, 65536))
                # An empty session has no shares, saved hubs or credentials. Passive
                # defaults avoid opening client transfer ports. Never use ~/.ncdc.
                os.write(master, b"/set nick dc24hTest\r")
                command = f"/open dc24h_test adc://127.0.0.1:{relay.getsockname()[1]}/\r"
                os.write(master, command.encode())
                deadline = time.monotonic() + 5
                while True:
                    self.assertIsNone(process.poll(), "ncdc exited before connecting")
                    self.assertLess(time.monotonic(), deadline, "ncdc did not connect to relay")
                    ready, _, _ = select.select([relay, master], [], [], 0.1)
                    if master in ready:
                        os.read(master, 65536)
                    if relay in ready:
                        client, _ = relay.accept()
                        break
                with client, socket.create_connection(hub.endpoints[0], timeout=3) as upstream:
                    client.settimeout(3)
                    frame = bytearray()
                    while b"\n" not in frame:
                        chunk = client.recv(4096)
                        self.assertTrue(chunk, "ncdc closed without HSUP")
                        frame.extend(chunk)
                        self.assertLessEqual(len(frame), 16384)
                        upstream.sendall(chunk)
                    self.assertTrue(frame.startswith(b"HSUP "), repr(frame))
                    self.assertIn(b"ADBASE", frame)
                    self.assertIn(b"ADTIGR", frame)
                    # Complete a real peer EOF before sampling shutdown counters.
                    upstream.shutdown(socket.SHUT_WR)
                    self.assertEqual(upstream.recv(1), b"")
                os.write(master, b"/quit\r")
                deadline = time.monotonic() + 3
                while process.poll() is None and time.monotonic() < deadline:
                    if select.select([master], [], [], 0.1)[0]:
                        try:
                            os.read(master, 65536)
                        except OSError:
                            break
                process.wait(timeout=2)
                self.assertEqual(process.returncode, 0)
                hub.stop()
                self.assertIn(b"transport=ADC event=accepted", hub.log)
                self.assertIn(b"transport=ADC event=peer_closed", hub.log)
                self.assertEqual(counters(self, hub), (1, 0, 0, 0))
                print("ncdc initial HSUP accepted by ADC parser; full login is outside this release")
            finally:
                if process.poll() is None:
                    process.kill()
                process.wait()
                os.close(master)


if __name__ == "__main__":
    unittest.main()
