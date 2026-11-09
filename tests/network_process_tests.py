"""Real loopback TCP/IPv6, limits, peer failures, resource pressure and shutdown."""
import os
import re
import resource
import selectors
import signal
import socket
import struct
import subprocess
import sys
import time
import unittest
from concurrent.futures import ThreadPoolExecutor

BINARY = os.path.abspath(sys.argv.pop(1))
READY = re.compile(rb"Network listening on (\[[^\]]+\]|[^ :]+):([0-9]+)")


class Hub:
    def __init__(self, case, *options, listeners=1, fd_limit=None):
        self.case = case
        self.log = bytearray()
        self.sockets = []
        kwargs = {}
        if fd_limit is not None:
            # This test process is single-threaded when Popen is called.
            kwargs["preexec_fn"] = lambda: resource.setrlimit(resource.RLIMIT_NOFILE, (fd_limit, fd_limit))
        self.process = subprocess.Popen(
            [BINARY, *options], stdout=subprocess.PIPE, stderr=subprocess.PIPE, bufsize=0, **kwargs
        )
        case.addCleanup(self.cleanup)
        self.selector = selectors.DefaultSelector()
        self.selector.register(self.process.stderr, selectors.EVENT_READ)
        self.wait_for(lambda: len(READY.findall(self.log)) == listeners)
        self.endpoints = [
            (host.decode().strip("[]"), int(port)) for host, port in READY.findall(self.log)
        ]

    def wait_for(self, condition, timeout=5):
        deadline = time.monotonic() + timeout
        while not condition():
            remaining = deadline - time.monotonic()
            self.case.assertGreater(remaining, 0, self.log.decode(errors="replace"))
            self.case.assertTrue(self.selector.select(remaining), "Timed out: " + repr(self.log))
            chunk = os.read(self.process.stderr.fileno(), 8192)
            self.case.assertTrue(chunk, "Server exited: " + repr(self.log))
            self.log.extend(chunk)

    def connect(self, index=0):
        peer = socket.create_connection(self.endpoints[index], timeout=3)
        self.sockets.append(peer)
        return peer

    def stop(self, sig=signal.SIGTERM):
        start = time.monotonic()
        self.process.send_signal(sig)
        stdout, stderr = self.process.communicate(timeout=3)
        self.log.extend(stderr)
        self.case.assertEqual(self.process.returncode, 0, self.log.decode(errors="replace"))
        self.case.assertEqual(stdout, b"")
        self.case.assertLess(time.monotonic() - start, 2)
        self.case.assertEqual(self.log.count(b"Core stopped"), 1)
        self.case.assertIn(b"Network stopped; all TCP sockets closed", self.log)

    def cleanup(self):
        for peer in self.sockets:
            peer.close()
        if self.process.poll() is None:
            self.process.kill()
        self.process.communicate()
        if hasattr(self, "selector"):
            self.selector.close()


class NetworkProcessTests(unittest.TestCase):
    def hub(self, *options, **kwargs):
        return Hub(self, "--listen", "127.0.0.1:0", *options, **kwargs)

    def assert_closed(self, peer, timeout=3):
        peer.settimeout(timeout)
        try:
            self.assertEqual(peer.recv(1), b"")
        except ConnectionResetError:
            pass  # Rejected/closed TCP sockets may reset when input is pending.

    def test_many_simultaneous_connections_and_data(self):
        hub = self.hub()
        peers = [hub.connect() for _ in range(48)]
        payload = b"BMSG invalid-transport-test\n" * 256
        with ThreadPoolExecutor(max_workers=8) as pool:
            list(pool.map(lambda peer: peer.sendall(payload), peers))
        # Syntax parsing does not send a protocol response or echo.
        for peer in peers[:3]:
            peer.settimeout(0.05)
            with self.assertRaises(socket.timeout):
                peer.recv(1)
        self.assertIsNone(hub.process.poll())
        # EOF after a half-close confirms every payload byte was consumed before checking counters.
        for peer in peers:
            peer.shutdown(socket.SHUT_WR)
        for peer in peers:
            self.assert_closed(peer)
        hub.stop()
        counters = re.search(rb"accepted=(\d+) rejected=(\d+) received_bytes=(\d+)", hub.log)
        self.assertIsNotNone(counters)
        self.assertEqual(tuple(map(int, counters.groups())), (48, 0, len(payload) * 48))
        for peer in peers:
            self.assert_closed(peer)

    def test_two_listeners(self):
        hub = self.hub("--listen", "127.0.0.1:0", listeners=2)
        self.assertNotEqual(hub.endpoints[0][1], hub.endpoints[1][1])
        peers = [hub.connect(i) for i in range(2)]
        hub.stop(signal.SIGINT)
        for peer in peers:
            self.assert_closed(peer)

    def test_ipv6(self):
        try:
            with socket.socket(socket.AF_INET6) as probe:
                probe.bind(("::1", 0))
        except OSError as error:
            self.skipTest("IPv6 loopback unavailable: " + str(error))
        hub = Hub(self, "--listen", "[::1]:0")
        peer = hub.connect()
        hub.stop()
        self.assert_closed(peer)

    def test_connection_limit_and_slot_reuse(self):
        hub = self.hub("--max-connections", "1")
        first = hub.connect()
        first.settimeout(0.1)
        with self.assertRaises(socket.timeout):
            first.recv(1)
        excess = hub.connect()
        self.assert_closed(excess)
        first.close()
        # Retry until the event loop has observed the FIN and released the slot.
        deadline = time.monotonic() + 3
        while True:
            candidate = hub.connect()
            candidate.settimeout(0.1)
            try:
                result = candidate.recv(1)
            except socket.timeout:
                break
            except ConnectionResetError:
                result = b""
            self.assertEqual(result, b"")
            candidate.close()
            self.assertLess(time.monotonic(), deadline, "Connection slot was not released")
        hub.stop()
        self.assert_closed(candidate)

    def test_idle_timeout(self):
        hub = self.hub("--idle-timeout", "1")
        idle = hub.connect()
        start = time.monotonic()
        self.assert_closed(idle)
        self.assertGreaterEqual(time.monotonic() - start, 0.8)
        # The listener remains usable after expiring a client.
        next_peer = hub.connect()
        hub.stop()
        self.assert_closed(next_peer)

    def test_resets_and_half_closes(self):
        hub = self.hub()
        for _ in range(30):
            peer = hub.connect()
            peer.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack("ii", 1, 0))
            peer.close()
        half = hub.connect()
        half.sendall(b"not ADC\n")
        half.shutdown(socket.SHUT_WR)
        self.assert_closed(half)
        healthy = hub.connect()
        healthy.settimeout(0.1)
        with self.assertRaises(socket.timeout):
            healthy.recv(1)
        hub.stop()
        self.assert_closed(healthy)

    def test_flood_does_not_delay_shutdown(self):
        hub = self.hub()
        peer = hub.connect()
        def flood():
            try:
                peer.settimeout(1)
                while True:
                    peer.sendall(b"x" * 65536)
            except OSError:
                pass
        with ThreadPoolExecutor(max_workers=1) as pool:
            sender = pool.submit(flood)
            time.sleep(0.05)
            hub.stop()
            sender.result(timeout=3)

    def test_descriptor_pressure_recovery(self):
        hub = self.hub("--idle-timeout", "1", fd_limit=40)
        for _ in range(64):
            hub.connect()
        hub.wait_for(lambda: b"TCP accept resource limit" in hub.log)
        self.assertIsNone(hub.process.poll())
        for peer in hub.sockets:
            peer.close()
        # The one-second accept cooldown recovers, and a new idle client is accepted/expired.
        peer = hub.connect()
        self.assert_closed(peer, timeout=5)
        hub.stop()

    def test_bind_failure_rolls_back(self):
        with socket.socket() as occupied:
            occupied.bind(("127.0.0.1", 0))
            occupied.listen()
            port = occupied.getsockname()[1]
            result = subprocess.run(
                [BINARY, "--listen", "127.0.0.1:0", "--listen", f"127.0.0.1:{port}"],
                capture_output=True, timeout=3, check=False
            )
        self.assertEqual(result.returncode, 1)
        self.assertIn(b"bind TCP listener", result.stderr)
        self.assertNotIn(b"Network listening", result.stderr)
        self.assertIn(b"Core stopped after Network failure", result.stderr)

    def test_invalid_network_options(self):
        cases = [
            ("--listen",), ("--listen", "localhost:411"), ("--listen", "::1:411"),
            ("--listen", "[::1]:65536"), ("--listen", "127.0.0.1:-1"),
            ("--listen", "127.0.0.1:"), ("--max-connections", "0"),
            ("--max-connections", "65536"), ("--max-connections", "1x"),
            ("--idle-timeout", "0"), ("--idle-timeout", "86401"),
            ("--idle-timeout", "-1"), ("--idle-timeout", "99999999999999"),
            ("--help", "--listen", "127.0.0.1:0"),
            tuple(["--listen", "127.0.0.1:0"] * 17)
        ]
        for options in cases:
            with self.subTest(options=options):
                result = subprocess.run([BINARY, *options], capture_output=True, timeout=3, check=False)
                self.assertEqual(result.returncode, 2, result.stderr)
                self.assertIn(b"Invalid arguments", result.stderr)
                self.assertNotIn(b"Starting dc24h.eu", result.stderr)


if __name__ == "__main__":
    unittest.main()
