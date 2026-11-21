"""Disposable local HTTPS Apache integration for beta-0.2 remote route isolation."""
import grp
import http.client
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import os
from pathlib import Path
import pwd
import signal
import socket
import ssl
import subprocess
import sys
import tempfile
import threading
import time
import unittest

APACHE, MODULES, EXAMPLE = sys.argv[1:4]
del sys.argv[1:4]
MARKER = b"REMOTE_UPSTREAM_ONLY"


class Backend(ThreadingHTTPServer):
    daemon_threads = True

    def __init__(self):
        self.seen = []
        class Handler(BaseHTTPRequestHandler):
            def do_GET(self):
                self.handle_any()

            def do_POST(self):
                self.handle_any()

            def handle_any(self):
                parent = self.server
                parent.seen.append((self.command, self.path, dict(self.headers.items())))
                data = MARKER
                self.send_response(200)
                self.send_header("Content-Type", "application/json")
                self.send_header("Content-Length", str(len(data)))
                self.end_headers()
                self.wfile.write(data)

            def log_message(self, *_args):
                return

        super().__init__(("127.0.0.1", 0), Handler)


class ApacheRemoteGateway(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        shipped = Path(EXAMPLE).read_text()
        if "Listen 127.0.0.1:8443" not in shipped or "ProxyRequests Off" not in shipped:
            raise AssertionError("Remote gateway example must use loopback and no forward proxy")
        cls.temp = tempfile.TemporaryDirectory(prefix="dc24h-remote-apache-")
        cls.addClassCleanup(cls.temp.cleanup)
        cls.root = Path(cls.temp.name)
        cls.root.chmod(0o755)
        cls.public = cls.root / "public"
        cls.public.mkdir()
        (cls.public / "remote.html").write_text("<title>Remote portal fixture</title>")
        (cls.public / "index.html").write_text("LOCAL_PRIVATE_ADMIN_UI")
        (cls.public / ".env").write_text("PRIVATE_FIXTURE_NOT_FOR_HTTP")
        (cls.public / "secret.ini").write_text("PRIVATE_FIXTURE_NOT_FOR_HTTP")
        cls.backend = Backend()
        thread = threading.Thread(target=cls.backend.serve_forever, daemon=True)
        thread.start()
        cls.addClassCleanup(cls.backend.server_close)
        cls.addClassCleanup(cls.backend.shutdown)
        cls.cert = cls.root / "certificate.pem"
        cls.key = cls.root / "private.key"
        subprocess.run(["openssl", "req", "-x509", "-newkey", "rsa:2048",
                        "-nodes", "-days", "1", "-subj", "/CN=localhost",
                        "-addext", "subjectAltName=DNS:localhost,IP:127.0.0.1",
                        "-keyout", str(cls.key), "-out", str(cls.cert)],
                       check=True, capture_output=True, timeout=15)
        cls.key.chmod(0o600)
        with socket.socket() as listener:
            listener.bind(("127.0.0.1", 0))
            cls.port = listener.getsockname()[1]
        who = pwd.getpwnam("nobody") if os.geteuid() == 0 else pwd.getpwuid(os.geteuid())
        group = grp.getgrgid(who.pw_gid).gr_name
        modules = ["mpm_event", "authz_core", "authz_host", "headers", "dir", "mime",
                   "alias", "socache_shmcb", "ssl", "proxy", "proxy_http"]
        conf = 'ServerRoot "' + str(cls.root) + '"\nServerName localhost\n'
        for module in modules:
            conf += 'LoadModule ' + module + '_module "' + str(Path(MODULES) / ("mod_" + module + ".so")) + '"\n'
        conf += ('PidFile "' + str(cls.root / "apache.pid") + '"\n'
                 + 'ErrorLog "' + str(cls.root / "error.log") + '"\n'
                 + 'DefaultRuntimeDir "' + str(cls.root) + '"\n'
                 + 'User ' + who.pw_name + "\nGroup " + group + "\n"
                 + "StartServers 1\nServerLimit 1\nThreadsPerChild 8\nMaxRequestWorkers 8\nTimeout 5\n"
                 + 'SSLSessionCache "shmcb:' + str(cls.root / "ssl-cache") + '(512000)"\n'
                 + 'TypesConfig "' + str(cls.root / "mime.types") + '"\n')
        (cls.root / "mime.types").write_text("text/html html\napplication/json json\n")
        shipped = (shipped
                   .replace("127.0.0.1:8443", "127.0.0.1:" + str(cls.port))
                   .replace("/usr/local/share/dc24h/webadmin", str(cls.public))
                   .replace("/etc/dc24h/remote-tls/fullchain.pem", str(cls.cert))
                   .replace("/etc/dc24h/remote-tls/privkey.pem", str(cls.key))
                   .replace("http://127.0.0.1:4182", "http://127.0.0.1:" + str(cls.backend.server_address[1])))
        cls.conf = cls.root / "httpd.conf"
        cls.conf.write_text(conf + shipped)
        parsed = subprocess.run([APACHE, "-t", "-f", str(cls.conf)],
                                capture_output=True, text=True, timeout=10)
        if parsed.returncode:
            raise AssertionError("Apache remote gateway syntax invalid: " + parsed.stderr)
        cls.server = subprocess.Popen([APACHE, "-f", str(cls.conf), "-DFOREGROUND"],
                                      stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, start_new_session=True)
        cls.addClassCleanup(cls.stop)
        for _ in range(120):
            if cls.server.poll() is not None:
                raise AssertionError((cls.root / "error.log").read_text()[-5000:])
            try:
                with socket.create_connection(("127.0.0.1", cls.port), timeout=.1):
                    break
            except OSError:
                time.sleep(.05)
        else:
            raise AssertionError("Isolated Apache gateway did not start")

    @classmethod
    def stop(cls):
        if cls.server.poll() is None:
            cls.server.terminate()
        try:
            cls.server.communicate(timeout=5)
        except subprocess.TimeoutExpired:
            os.killpg(cls.server.pid, signal.SIGKILL)
            cls.server.communicate(timeout=3)

    def fetch(self, path, method="GET", headers=None, body=None):
        context = ssl.create_default_context(cafile=str(self.cert))
        conn = http.client.HTTPSConnection("localhost", self.port, context=context, timeout=4)
        try:
            conn.request(method, path, body=body, headers=headers or {})
            response = conn.getresponse()
            return response.status, response.read(), dict(response.getheaders())
        finally:
            conn.close()

    def test_allowlisted_routes_and_isolation(self):
        for name in ("challenge", "login", "session", "summary", "logout"):
            method = "POST" if name in ("login", "logout") else "GET"
            status, body, headers = self.fetch("/remote/v1/" + name, method,
                                                {"Origin": "https://localhost:8443",
                                                 "Cookie": "dc24h_remote=fixture"})
            self.assertEqual(status, 200, (name, status))
            self.assertEqual(body, MARKER)
            self.assertEqual(headers.get("Cache-Control"), "no-store")
            self.assertIn("nosniff", headers.get("X-Content-Type-Options", ""))
        forwarded = {item[1] for item in self.backend.seen}
        self.assertEqual(forwarded, {"/remote/v1/" + route for route in
                                     ("challenge", "login", "session", "summary", "logout")})

    def test_browser_credentials_stripped(self):
        previous = len(self.backend.seen)
        status, _, _ = self.fetch("/remote/v1/summary", headers={
            "Authorization": "Bearer browser-secret",
            "X-DC24H-Admin-Grant": "browser-grant",
            "X-DC24H-Session": "browser-session",
            "X-DC24H-Csrf": "browser-csrf",
            "X-Forwarded-For": "8.8.8.8",
            "Forwarded": "for=8.8.8.8",
            "Cookie": "dc24h_remote=fixture",
        })
        self.assertEqual(status, 200)
        headers = {key.lower(): value for key, value in self.backend.seen[previous][2].items()}
        for key in ("authorization", "x-dc24h-admin-grant", "x-dc24h-session",
                    "x-dc24h-csrf", "x-forwarded-for", "forwarded"):
            self.assertNotIn(key, headers)
        self.assertEqual(headers.get("cookie"), "dc24h_remote=fixture")

    def test_private_paths_never_forward(self):
        for path in ("/api/v1/status", "/admin-api/v1/management", "/management",
                     "/remote/v1/admin", "/remote/v1/wildcard",
                     "/.env", "/secret.ini"):
            before = len(self.backend.seen)
            status, body, _ = self.fetch(path)
            self.assertNotEqual(status, 200, path)
            self.assertNotIn(MARKER, body)
            self.assertNotIn(b"PRIVATE_FIXTURE_NOT_FOR_HTTP", body)
            self.assertEqual(len(self.backend.seen), before, path)

    def test_tls_only(self):
        context = ssl.create_default_context(cafile=str(self.cert))
        context.minimum_version = ssl.TLSVersion.TLSv1_2
        conn = http.client.HTTPSConnection("localhost", self.port, context=context, timeout=4)
        try:
            conn.request("GET", "/remote.html")
            answer = conn.getresponse()
            self.assertEqual(answer.status, 200)
            self.assertIn(b"Remote portal fixture", answer.read())
        finally:
            conn.close()


if __name__ == "__main__":
    unittest.main()
