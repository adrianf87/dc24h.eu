"""Bounded active-port reservations for real ncdc integration tests."""
from contextlib import ExitStack, contextmanager
import errno
import socket


@contextmanager
def reserve_active_port(kind, cross_scope=False, attempts=64):
    """Hold the client port and optional other-interface conflict until ready.

    Port zero selects a free port only for that bind address. Retry allocation
    if the chosen cross-scope number is occupied on the client's address. No
    test assertion, ncdc startup or exception from the caller body is retried.
    """
    if kind not in (socket.SOCK_STREAM, socket.SOCK_DGRAM) or attempts < 1:
        raise ValueError('Invalid active-port reservation options')
    for _ in range(attempts):
        with ExitStack() as resources:
            reservation = resources.enter_context(socket.socket(socket.AF_INET, kind))
            conflict = resources.enter_context(socket.socket(socket.AF_INET, kind)) if cross_scope else None
            try:
                candidate = 0
                if conflict is not None:
                    conflict.bind(('127.0.0.2', 0))
                    if kind == socket.SOCK_STREAM:
                        conflict.listen(1)
                    candidate = conflict.getsockname()[1]
                # ncdc uses SO_REUSEADDR and binds 127.0.0.1 explicitly. Keep
                # this reservation non-listening so its listener can take over.
                reservation.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
                reservation.bind(('127.0.0.1', candidate))
            except OSError as error:
                if cross_scope and error.errno == errno.EADDRINUSE:
                    continue  # ExitStack closes both sockets before another pair.
                raise
            yield reservation.getsockname()[1]
            return
    raise RuntimeError('Cannot reserve active ncdc port on both loopback addresses')
