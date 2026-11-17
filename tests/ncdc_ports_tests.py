"""Deterministic collision and cleanup checks using real loopback sockets."""
import errno
import socket
import unittest
from unittest.mock import patch

from ncdc_ports import reserve_active_port

REAL_SOCKET = socket.socket


class ForcedSocket:
    def __init__(self, raw, bind_hook):
        self.raw, self.bind_hook = raw, bind_hook

    def __getattr__(self, name):
        return getattr(self.raw, name)

    def bind(self, address):
        return self.raw.bind(self.bind_hook(address))

    def __enter__(self):
        return self

    def __exit__(self, *_):
        self.raw.close()


class ReservationTests(unittest.TestCase):
    def collision(self, kind):
        with REAL_SOCKET(socket.AF_INET, kind) as occupied:
            occupied.bind(('127.0.0.1', 0))
            if kind == socket.SOCK_STREAM:
                occupied.listen(1)
            busy = occupied.getsockname()[1]
            created, forced = [], []

            def bind_hook(address):
                if address == ('127.0.0.2', 0) and not forced:
                    forced.append(True)
                    return ('127.0.0.2', busy)
                return address

            def factory(*args):
                raw = REAL_SOCKET(*args)
                created.append(raw)
                return ForcedSocket(raw, bind_hook)

            with patch('ncdc_ports.socket.socket', side_effect=factory):
                with reserve_active_port(kind, cross_scope=True) as port:
                    self.assertNotEqual(port, busy)
                    self.assertGreaterEqual(len(created), 4)
                    self.assertTrue(all(sock.fileno() == -1 for sock in created[:-2]))
                    for address in ('127.0.0.1', '127.0.0.2'):
                        with REAL_SOCKET(socket.AF_INET, kind) as probe:
                            with self.assertRaises(OSError) as caught:
                                probe.bind((address, port))
                            self.assertEqual(caught.exception.errno, errno.EADDRINUSE)
            self.assertTrue(all(sock.fileno() == -1 for sock in created))

    def test_tcp_collision_reallocates_the_pair(self):
        self.collision(socket.SOCK_STREAM)

    def test_udp_collision_reallocates_the_pair(self):
        self.collision(socket.SOCK_DGRAM)

    def test_other_errors_are_not_retried(self):
        created = []

        def bind_hook(_):
            raise OSError(errno.EACCES, 'fixture bind denied')

        def factory(*args):
            raw = REAL_SOCKET(*args)
            created.append(raw)
            return ForcedSocket(raw, bind_hook)

        with patch('ncdc_ports.socket.socket', side_effect=factory):
            with self.assertRaises(OSError) as caught:
                with reserve_active_port(socket.SOCK_STREAM, cross_scope=True):
                    self.fail('Unexpected reservation')
            self.assertEqual(caught.exception.errno, errno.EACCES)
        self.assertEqual(len(created), 2)
        self.assertTrue(all(sock.fileno() == -1 for sock in created))

    def test_collision_exhaustion_closes_every_socket(self):
        created = []

        def bind_hook(_):
            raise OSError(errno.EADDRINUSE, 'fixture port busy')

        def factory(*args):
            raw = REAL_SOCKET(*args)
            created.append(raw)
            return ForcedSocket(raw, bind_hook)

        with patch('ncdc_ports.socket.socket', side_effect=factory):
            with self.assertRaises(RuntimeError):
                with reserve_active_port(socket.SOCK_DGRAM, cross_scope=True, attempts=3):
                    self.fail('Unexpected reservation')
        self.assertEqual(len(created), 6)
        self.assertTrue(all(sock.fileno() == -1 for sock in created))

    def test_caller_failure_is_not_retried(self):
        with self.assertRaises(OSError) as caught:
            with reserve_active_port(socket.SOCK_STREAM, cross_scope=True):
                raise OSError(errno.EADDRINUSE, 'caller fixture failure')
        self.assertEqual(str(caught.exception), '[Errno 98] caller fixture failure')


if __name__ == '__main__':
    unittest.main()
