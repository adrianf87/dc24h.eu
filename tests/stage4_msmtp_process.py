"""Exercise /usr/bin/msmtp against a TLS SMTP fixture. No external delivery."""
import base64
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import socket
import ssl
import subprocess
import tempfile
import time
import unittest


USERNAME = "smtp-fixture"
PASSWORD = "fixture password with spaces 42"
MAIL = b"From: admin@dc24h.eu\nTo: recipient@example.test\nSubject: Local fixture\n\nSynthetic message\n"


class SMTPTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.workspace = tempfile.TemporaryDirectory(prefix="stage4-msmtp-")
        cls.directory = Path(cls.workspace.name)
        subprocess.run([
            "openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "1",
            "-subj", "/CN=localhost", "-addext", "subjectAltName=DNS:localhost",
            "-keyout", str(cls.directory / "key"), "-out", str(cls.directory / "cert"),
        ], check=True, capture_output=True)
        cls.ca_file = tempfile.NamedTemporaryFile(prefix="stage4-smtp-ca-", dir="/tmp")
        cls.ca_file.write((cls.directory / "cert").read_bytes())
        cls.ca_file.flush()
        cls.context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        cls.context.load_cert_chain(cls.directory / "cert", cls.directory / "key")

    @classmethod
    def tearDownClass(cls):
        cls.ca_file.close()
        cls.workspace.cleanup()

    @staticmethod
    def smtp(stream, starttls=False, response=250):
        def recv():
            return stream.readline().rstrip(b"\r\n")

        def send(value):
            stream.write(value + b"\r\n")
            stream.flush()

        if not starttls:
            send(b"220 synthetic SMTP fixture")
        assert recv().startswith(b"EHLO ")
        send(b"250-fixture")
        send(b"250 AUTH PLAIN")
        auth = recv()
        assert auth.startswith(b"AUTH PLAIN")
        _, _, token = auth.partition(b"AUTH PLAIN ")
        if not token:
            send(b"334 ")
            token = recv()
        if base64.b64decode(token) != b"\x00" + USERNAME.encode() + b"\x00" + PASSWORD.encode():
            send(b"535 Authentication refused")
            return False
        send(b"235 Authentication successful")
        assert recv().upper().startswith(b"MAIL FROM:<ADMIN@DC24H.EU>")
        send(b"250 Sender accepted")
        assert recv().upper().startswith(b"RCPT TO:<RECIPIENT@EXAMPLE.TEST>")
        if response != 250:
            send(str(response).encode() + b" Recipient refused")
            return False
        send(b"250 Recipient accepted")
        assert recv() == b"DATA"
        send(b"354 End message with a dot")
        message = []
        for line in iter(recv, b"."):
            message.append(line)
            if len(message) > 20:
                raise AssertionError("SMTP message terminator missing")
        send(b"250 Queued")
        if recv() == b"QUIT":
            send(b"221 Goodbye")
        return b"Synthetic message" in message

    def run_fixture(self, *, starttls=False, host="localhost", password=PASSWORD, response=250, slow=False):
        def serve(listener):
            with listener.accept()[0] as plain:
                plain.settimeout(3)
                if slow:
                    time.sleep(1.3)
                    return False
                if starttls:
                    with plain.makefile("rwb") as stream:
                        stream.write(b"220 synthetic SMTP fixture\r\n")
                        stream.flush()
                        self.assertTrue(stream.readline().startswith(b"EHLO "))
                        stream.write(b"250-fixture\r\n250 STARTTLS\r\n")
                        stream.flush()
                        self.assertEqual(stream.readline(), b"STARTTLS\r\n")
                        stream.write(b"220 Ready\r\n")
                        stream.flush()
                try:
                    with self.context.wrap_socket(plain, server_side=True) as tls, tls.makefile("rwb") as stream:
                        return self.smtp(stream, starttls=starttls, response=response)
                except (ssl.SSLError, EOFError, OSError):
                    return False

        with (socket.socket(socket.AF_INET, socket.SOCK_STREAM) as listener,
              ThreadPoolExecutor() as threads,
              tempfile.NamedTemporaryFile(prefix="stage4-smtp-conf-", dir="/tmp") as config):
            listener.bind(("127.0.0.1", 0))
            listener.listen()
            listener.settimeout(4)
            work = threads.submit(serve, listener)
            config.write(f"""defaults
auth plain
tls on
tls_certcheck on
tls_trust_file {self.ca_file.name}
tls_starttls {"on" if starttls else "off"}
timeout 1
account default
host {host}
port {listener.getsockname()[1]}
from admin@dc24h.eu
user {USERNAME}
password "{password}"
""".encode())
            config.flush()
            delivery = subprocess.run(
                ["/usr/bin/msmtp", f"--file={config.name}", "-t"], input=MAIL,
                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=5,
            )
            return delivery.returncode, work.result(timeout=5)

    def test_implicit_tls_authenticated_delivery(self):
        self.assertEqual(self.run_fixture(), (0, True))

    def test_starttls_authenticated_delivery(self):
        self.assertEqual(self.run_fixture(starttls=True), (0, True))

    def test_incorrect_password_fails_closed(self):
        status, delivered = self.run_fixture(password="fixture incorrect password")
        self.assertNotEqual(status, 0)
        self.assertFalse(delivered)

    def test_wrong_server_name_fails_certificate_verification(self):
        status, delivered = self.run_fixture(host="127.0.0.1")
        self.assertNotEqual(status, 0)
        self.assertFalse(delivered)

    def test_timeout(self):
        status, delivered = self.run_fixture(starttls=True, slow=True)
        self.assertNotEqual(status, 0)
        self.assertFalse(delivered)

    def test_temporary_and_permanent_recipient_failures(self):
        for response in (450, 550):
            with self.subTest(response=response):
                status, delivered = self.run_fixture(response=response)
                self.assertNotEqual(status, 0)
                self.assertFalse(delivered)


if __name__ == "__main__":
    unittest.main()
