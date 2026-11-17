"""Private Unix-socket faults against the disposable test MariaDB only."""
from pathlib import Path
import selectors
import socket
import subprocess
import threading


def fault_case(binary, folder, database_socket, password, mode):
    endpoint = Path(folder) / (mode + '.sock')
    stop = threading.Event()
    triggered = threading.Event()
    failures = []

    with socket.socket(socket.AF_UNIX) as listener:
        listener.bind(str(endpoint))
        listener.listen(1)
        listener.settimeout(10)

        def proxy():
            try:
                with listener.accept()[0] as client, socket.socket(socket.AF_UNIX) as backend:
                    backend.settimeout(5)
                    client.settimeout(5)
                    backend.connect(str(database_socket))
                    pending = bytearray()
                    drop_ack = False
                    with selectors.DefaultSelector() as selector:
                        selector.register(client, selectors.EVENT_READ)
                        selector.register(backend, selectors.EVENT_READ)
                        while not stop.is_set():
                            for key, _ in selector.select(0.1):
                                data = key.fileobj.recv(65536)
                                if not data:
                                    return
                                if key.fileobj is backend:
                                    if drop_ack:
                                        # The backend has actually acknowledged COMMIT.
                                        # Close both sides without returning its reply.
                                        if data[4:5] != b'\x00':
                                            raise AssertionError('Backend did not acknowledge COMMIT')
                                        triggered.set()
                                        return
                                    client.sendall(data)
                                    continue
                                pending.extend(data)
                                # Frame only the client direction. Stop before draining
                                # the large execute packet so the sender's buffer fills.
                                while len(pending) >= 5:
                                    size = int.from_bytes(pending[:3], 'little') + 4
                                    if mode == 'write' and pending[3:5] == b'\x00\x17':
                                        triggered.set()
                                        selector.unregister(client)
                                        pending.clear()
                                        break
                                    if len(pending) < size:
                                        break
                                    packet = bytes(pending[:size])
                                    del pending[:size]
                                    backend.sendall(packet)
                                    if mode == 'commit_ack' and packet[3:] == b'\x00\x03COMMIT':
                                        drop_ack = True
            except Exception as error:
                failures.append(error)

        worker = threading.Thread(target=proxy, daemon=True)
        worker.start()
        try:
            result = subprocess.run([binary, str(endpoint), str(password), mode],
                                    capture_output=True, text=True, timeout=20)
        finally:
            stop.set()
            worker.join(timeout=6)
        if worker.is_alive():
            raise AssertionError('Database fault proxy did not stop')
        if failures:
            raise AssertionError('Database fault proxy failed') from failures[0]
        if not triggered.is_set():
            raise AssertionError('Database fault was not exercised')
        if result.returncode != 0:
            raise AssertionError(result.stderr)
        return result
