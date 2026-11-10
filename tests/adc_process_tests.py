"""ADC validation integrated with real TCP streams; syntax counters remain distinct from session authorization."""
import re
import signal
import socket
import sys
import unittest

from network_process_tests import Hub, drain_until_eof

ADC_COUNTERS = re.compile(
    rb"ADC stopped; frames=(\d+) unknown_commands=(\d+) invalid_frames=(\d+) keepalives=(\d+)"
)


def counters(case, hub):
    match = ADC_COUNTERS.search(hub.log)
    case.assertIsNotNone(match, hub.log.decode(errors="replace"))
    return tuple(map(int, match.groups()))


class AdcProcessTests(unittest.TestCase):
    def hub(self):
        return Hub(self, "--listen", "127.0.0.1:0")

    def send_and_finish(self, hub, wire):
        peer = hub.connect()
        peer.sendall(wire)
        peer.shutdown(socket.SHUT_WR)
        while peer.recv(4096):
            pass  # Ordered handshake/rejection responses are now possible.
        return peer

    def test_fragmented_and_coalesced_frames(self):
        hub = self.hub()
        peer = hub.connect()
        for byte in b"HSUP ADBASE ADTIGR\n":
            peer.sendall(bytes([byte]))
        response = bytearray()
        while response.count(b'\n') < 2:
            response.extend(peer.recv(1024))
        self.assertTrue(response.startswith(b'ISUP ADBASE ADTIGR\nISID '))
        # Coalesced syntactically valid frames still count independently even
        # when the first command is unsupported by the login dispatcher.
        peer.sendall("HZ99 extension\n\nBMSG ABC2 Cześć\\s😀\n".encode())
        peer.shutdown(socket.SHUT_WR)
        while peer.recv(4096):
            pass
        hub.stop()
        self.assertEqual(counters(self, hub), (3, 1, 0, 1))

    def test_invalid_frames_are_ignored_and_stream_recovers(self):
        hub = self.hub()
        self.send_and_finish(hub, b"BMSG BAD text\nHMSG invalid\\q\nHMSG \xff\nHSUP ADBASE\n")
        hub.stop()
        self.assertEqual(counters(self, hub), (1, 0, 3, 0))

    def test_base_argument_syntax_and_extension_recovery(self):
        hub = self.hub()
        self.send_and_finish(hub, b"HSUP\nBINF ABC2 lowerPRIVATEFIELD\nCGET file /a 1e3 -1\n"
                             b"HSUP ADBASE\nBINF ABC2 NIJoanna DE NI\n"
                             b"DMSG ABC2 XYZ7 NIpositional PMABC2\nHZ99 lower a 1Xfuture\n\n")
        hub.stop()
        self.assertEqual(counters(self, hub), (4, 1, 3, 1))
        self.assertNotIn(b"PRIVATEFIELD", hub.log)

    def test_independent_client_buffers_and_partial_eof(self):
        hub = self.hub()
        first, second = hub.connect(), hub.connect()
        first.sendall(b"HSUP AD")
        second.sendall(b"BASE\nHSUP ADBASE\n")
        first.sendall(b"BASE\nHMSG incomplete")
        for peer in (first, second):
            peer.shutdown(socket.SHUT_WR)
            drain_until_eof(peer)
        hub.stop()
        self.assertEqual(counters(self, hub), (2, 0, 2, 0))

    def test_oversize_input_resynchronizes_without_affecting_other_clients(self):
        hub = self.hub()
        attacker = hub.connect()
        attacker.sendall(b"x" * 20000)
        self.send_and_finish(hub, b"HSUP ADBASE\n")
        attacker.sendall(b"\nHSUP ADBASE\n")
        attacker.shutdown(socket.SHUT_WR)
        drain_until_eof(attacker)
        hub.stop()
        self.assertEqual(counters(self, hub), (2, 0, 1, 0))

    def test_sensitive_parameters_never_enter_logs(self):
        hub = self.hub()
        self.send_and_finish(hub, b"HPAS SECRETHASH\nBINF ABC2 PDPRIVATEID NIprivate\\nnick\n"
                                  b"HMSG INVALIDSECRET\\q\n")
        hub.stop(signal.SIGINT)
        self.assertEqual(counters(self, hub), (2, 0, 1, 0))
        for value in (b"SECRETHASH", b"PRIVATEID", b"private", b"INVALIDSECRET"):
            self.assertNotIn(value, hub.log)


if __name__ == "__main__":
    unittest.main()
