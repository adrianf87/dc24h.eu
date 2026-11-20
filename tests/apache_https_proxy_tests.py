"""Disposable Apache HTTPS reverse proxy; never loads or changes the host configuration."""
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
import warnings

APACHE, MODULES, EXAMPLE = sys.argv[1:4]
del sys.argv[1:4]

GRANT = "ab" * 32
SESSION = "cd" * 16
CSRF = "ef" * 32
SECRET = "Bearer PROXYSECRETTOKEN"
MARKER = b"UPSTREAM_MARKER"


class Backend(threading.Thread):
    def __init__(self):
        super().__init__(daemon=True)
        self.seen = []
        self.lock = threading.Lock()
        self.ready = threading.Event()
        self.httpd = None

    def run(self):
        parent = self

        class Handler(BaseHTTPRequestHandler):
            protocol_version = "HTTP/1.1"

            def do_GET(self):
                self.record()

            def do_POST(self):
                self.record()

            def do_PUT(self):
                self.record()

            def record(self):
                length = int(self.headers.get("Content-Length", "0") or 0)
                body = self.rfile.read(length) if length else b""
                item = {
                    "method": self.command,
                    "path": self.path,
                    "host": self.headers.get("Host"),
                    "authorization": self.headers.get("Authorization"),
                    "cookie": self.headers.get("Cookie"),
                    "forwarded": self.headers.get("X-Forwarded-For"),
                    "origin": self.headers.get("Origin"),
                    "grant": self.headers.get("X-DC24H-Admin-Grant"),
                    "session": self.headers.get("X-DC24H-Session"),
                    "csrf": self.headers.get("X-DC24H-Csrf"),
                    "admin": self.headers.get("X-DC24H-Admin"),
                    "body": body,
                }
                with parent.lock:
                    parent.seen.append(item)
                payload = b'{"ok":true,"marker":"UPSTREAM_MARKER"}'
                self.send_response(200)
                self.send_header("Content-Type", "application/json")
                self.send_header("Content-Length", str(len(payload)))
                self.send_header("Cache-Control", "private")
                self.end_headers()
                self.wfile.write(payload)

            def log_message(self, fmt, *args):
                return

        self.httpd = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        self.port = self.httpd.server_address[1]
        self.ready.set()
        self.httpd.serve_forever(poll_interval=0.1)

    def stop(self):
        if self.httpd is not None:
            self.httpd.shutdown()
            self.httpd.server_close()
        self.join(timeout=5)

    def snapshot(self):
        with self.lock:
            return list(self.seen)


class ApacheHttpsProxyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        shipped = Path(EXAMPLE).read_text(encoding="utf-8")
        for line in shipped.splitlines():
            if line.lstrip().startswith("#"):
                continue
            lowered = line.lower()
            if "unix:" in lowered or "bearer " in lowered:
                raise AssertionError("shipped proxy directive contains a socket target or service token: " + line)
        if "ProxyRequests Off" not in shipped or shipped.count("Require local") < 2:
            raise AssertionError("shipped proxy policy is missing ProxyRequests Off or Require local")
        cls.temp = tempfile.TemporaryDirectory(prefix="dc24h-apache-proxy-")
        cls.addClassCleanup(cls.temp.cleanup)
        cls.backend = Backend()
        cls.backend.start()
        cls.addClassCleanup(cls.backend.stop)
        if not cls.backend.ready.wait(3):
            raise AssertionError("loopback upstream did not start")
        cls.folder = Path(cls.temp.name)
        cls.folder.chmod(0o755)
        cls.public = cls.folder / "public"
        cls.public.mkdir()
        cls.index = "<!doctype html><title>dc24h.eu</title><h1>HTTPS fixture</h1>\n"
        (cls.public / "index.html").write_text(cls.index)
        (cls.public / "assets").mkdir()
        (cls.public / "assets" / "panel.js").write_text('export const project = "dc24h.eu";\n')
        for name in [".env", "admin.token"]:
            (cls.public / name).write_text("PRIVATE_FIXTURE_NOT_FOR_HTTP")
        (cls.folder / "outside.txt").write_text("PRIVATE_FIXTURE_NOT_FOR_HTTP")
        (cls.public / "link.txt").symlink_to(cls.folder / "outside.txt")
        cls.cert = cls.folder / "certificate.pem"
        cls.key = cls.folder / "private.key"
        subprocess.run(
            ["openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "1",
             "-subj", "/CN=dc24h.eu", "-addext", "subjectAltName=DNS:dc24h.eu,IP:127.0.0.1",
             "-addext", "extendedKeyUsage=serverAuth", "-keyout", str(cls.key), "-out", str(cls.cert)],
            check=True, capture_output=True, timeout=15)
        cls.key.chmod(0o600)
        with socket.socket() as first, socket.socket() as second:
            first.bind(("127.0.0.1", 0))
            second.bind(("127.0.0.1", 0))
            cls.http_port = first.getsockname()[1]
            cls.https_port = second.getsockname()[1]
        user = pwd.getpwnam("nobody") if os.geteuid() == 0 else pwd.getpwuid(os.geteuid())
        group = grp.getgrgid(user.pw_gid).gr_name
        modules = ["mpm_event", "authz_core", "authz_host", "headers", "dir", "mime", "alias",
                   "autoindex", "socache_shmcb", "ssl", "proxy", "proxy_http"]
        cls.prefix = 'ServerRoot "' + str(cls.folder) + '"\nServerName localhost\n'
        for module in modules:
            cls.prefix += 'LoadModule ' + module + '_module "' + str(Path(MODULES) / ("mod_" + module + ".so")) + '"\n'
        cls.prefix += (
            'PidFile "' + str(cls.folder / "apache.pid") + '"\nErrorLog "' + str(cls.folder / "error.log")
            + '"\nDefaultRuntimeDir "' + str(cls.folder) + '"\nUser ' + user.pw_name + "\nGroup " + group
            + "\nStartServers 1\nServerLimit 1\nThreadsPerChild 8\nMaxRequestWorkers 8\nTimeout 5\n"
            + 'SSLSessionCache "shmcb:' + str(cls.folder / "ssl-cache") + '(512000)"\n'
            + "Listen 127.0.0.1:" + str(cls.http_port) + "\nListen 127.0.0.1:" + str(cls.https_port) + "\n")
        mime = cls.folder / "mime.types"
        mime.write_text("text/html html\napplication/javascript js\napplication/json json\n")
        cls.prefix += 'TypesConfig "' + str(mime) + '"\n'
        cls.upstream = "http://127.0.0.1:" + str(cls.backend.port)
        cls.vhosts = (shipped.replace("<VirtualHost *:80>", "<VirtualHost 127.0.0.1:" + str(cls.http_port) + ">")
                      .replace("<VirtualHost *:443>", "<VirtualHost 127.0.0.1:" + str(cls.https_port) + ">")
                      .replace("https://dc24h.eu/", "https://dc24h.eu:" + str(cls.https_port) + "/")
                      .replace("/usr/local/share/dc24h/webadmin", str(cls.public))
                      .replace("/etc/dc24h/webadmin-tls/fullchain.pem", str(cls.cert))
                      .replace("/etc/dc24h/webadmin-tls/privkey.pem", str(cls.key))
                      .replace("http://127.0.0.1:4173", cls.upstream))
        cls.config = cls.folder / "httpd.conf"
        cls.config.write_text(cls.prefix + cls.vhosts)
        check = cls.check_config(cls.config)
        if check.returncode:
            raise AssertionError("Isolated Apache syntax check failed: " + check.stderr)
        cls.server = subprocess.Popen([APACHE, "-f", str(cls.config), "-DFOREGROUND"],
                                      stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, start_new_session=True)
        cls.addClassCleanup(cls.stop)
        deadline = time.monotonic() + 8
        while time.monotonic() < deadline:
            if cls.server.poll() is not None:
                raise AssertionError("Isolated Apache exited: " + (cls.folder / "error.log").read_text()[-6000:])
            try:
                with socket.create_connection(("127.0.0.1", cls.http_port), timeout=.2):
                    break
            except OSError:
                time.sleep(.05)
        else:
            raise AssertionError("Isolated Apache startup timed out")

    @classmethod
    def check_config(cls, path):
        return subprocess.run([APACHE, "-t", "-f", str(path)], capture_output=True, text=True, timeout=10)

    @classmethod
    def stop(cls):
        if cls.server.poll() is None:
            cls.server.terminate()
        try:
            cls.server.communicate(timeout=5)
        except subprocess.TimeoutExpired:
            os.killpg(cls.server.pid, signal.SIGKILL)
            cls.server.communicate(timeout=3)

    @classmethod
    def tearDownClass(cls):
        log = (cls.folder / "error.log").read_text(errors="replace")
        for secret in ("PROXYSECRETTOKEN", GRANT, "proxysecretcookie"):
            if secret in log:
                raise AssertionError("isolated Apache log contains a client secret")

    def tls(self, version=None):
        context = ssl.create_default_context(cafile=str(self.cert))
        if version is not None:
            context.minimum_version = context.maximum_version = version
        raw = socket.create_connection(("127.0.0.1", self.https_port), timeout=3)
        try:
            peer = context.wrap_socket(raw, server_hostname="dc24h.eu")
        except Exception:
            raw.close()
            raise
        self.addCleanup(peer.close)
        return peer

    def exchange(self, method, path, headers=None, body=None, plain=False):
        before = len(self.backend.snapshot())
        if plain:
            client = http.client.HTTPConnection("127.0.0.1", self.http_port, timeout=3)
        else:
            context = ssl.create_default_context(cafile=str(self.cert))
            client = http.client.HTTPSConnection("127.0.0.1", self.https_port, timeout=3, context=context)
        merged = {"Host": "dc24h.eu", **(headers or {})}
        client.request(method, path, body=body, headers=merged)
        result = client.getresponse()
        payload = result.read()
        status, response_headers = result.status, dict(result.getheaders())
        client.close()
        seen = self.backend.snapshot()[before:]
        return status, response_headers, payload, seen

    def assert_denied(self, method, path, headers=None, body=None):
        status, headers_out, payload, seen = self.exchange(method, path, headers, body)
        self.assertEqual(status, 403, payload)
        self.assertNotIn(MARKER, payload)
        self.assertEqual(seen, [])
        self.assertEqual(headers_out["Cache-Control"], "no-store")
        self.assertEqual(headers_out["Strict-Transport-Security"], "max-age=300")
        return payload

    def test_static_redirect_secrets_and_tls_versions(self):
        status, headers, body, seen = self.exchange("GET", "/")
        self.assertEqual(status, 200)
        self.assertEqual(body, self.index.encode())
        self.assertEqual(seen, [])
        self.assertEqual(headers["Strict-Transport-Security"], "max-age=300")
        self.assertIn("frame-ancestors 'none'", headers["Content-Security-Policy"])
        for version, expected in [(ssl.TLSVersion.TLSv1_2, "TLSv1.2"), (ssl.TLSVersion.TLSv1_3, "TLSv1.3")]:
            with self.subTest(version=expected):
                self.assertEqual(self.tls(version).version(), expected)
        for host in ["dc24h.eu", "untrusted.example"]:
            status, headers, body, seen = self.exchange(
                "GET", "/assets/panel.js?sample=1", {"Host": host}, plain=True)
            self.assertEqual(status, 301, body)
            self.assertEqual(headers["Location"], "https://dc24h.eu:" + str(self.https_port) + "/assets/panel.js?sample=1")
            self.assertIsNone(headers.get("Strict-Transport-Security"))
            self.assertEqual(seen, [])
        for path in ["/.env", "/admin.token", "/link.txt"]:
            with self.subTest(path=path):
                status, _, body, seen = self.exchange("GET", path)
                self.assertEqual(status, 403, body)
                self.assertNotIn(b"PRIVATE_FIXTURE_NOT_FOR_HTTP", body)
                self.assertEqual(seen, [])

    def test_allowlisted_requests_reach_only_the_loopback_adapter(self):
        form = {"Content-Type": "application/x-www-form-urlencoded", "X-DC24H-Admin": "1",
                "Origin": "https://dc24h.eu", "Sec-Fetch-Site": "same-origin"}
        reads = ["/admin-api/v1/status", "/admin-api/v1/health", "/admin-api/v1/users",
                 "/admin-api/v1/admin", "/admin-api/v1/management", "/admin-api/v1/accounts?after=0",
                 "/admin-api/v1/accounts/15", "/admin-api/v1/bans?state=active"]
        for path in reads:
            with self.subTest(path=path):
                status, headers, body, seen = self.exchange("GET", path, {"Sec-Fetch-Site": "none"})
                self.assertEqual(status, 200, body)
                self.assertIn(MARKER, body)
                self.assertEqual(headers["Cache-Control"], "no-store")
                self.assertEqual(seen[0]["path"], path)
                self.assertEqual(seen[0]["host"], self.upstream.removeprefix("http://"))
                self.assertIsNone(seen[0]["origin"])
        writes = [
            ("/admin-api/v1/admin/access", b"actor_id=1&proof=abc"),
            ("/admin-api/v1/audit", b"actor_id=1&proof=abc&after=0"),
            ("/admin-api/v1/accounts", b"name=ada"),
            ("/admin-api/v1/accounts/15/role", b"role=operator"),
            ("/admin-api/v1/bans", b"subject=1"),
            ("/admin-api/v1/bans/15/revoke", b"proof=abc"),
            ("/admin-api/v1/management/authorize", b"operation=configuration.read"),
        ]
        boundary = b"a=" + b"b" * 6998
        self.assertEqual(len(boundary), 7000)
        writes.append(("/admin-api/v1/audit", boundary))
        for path, payload in writes:
            with self.subTest(path=path, size=len(payload)):
                status, _, body, seen = self.exchange("POST", path, form, payload)
                self.assertEqual(status, 200, body)
                self.assertEqual(seen[0]["path"], path)
                self.assertEqual(seen[0]["body"], payload)
                self.assertEqual(seen[0]["origin"], self.upstream)
                self.assertEqual(seen[0]["admin"], "1")

    def test_client_secrets_are_stripped_and_a_valid_grant_is_forwarded(self):
        headers = {
            "Authorization": SECRET,
            "Cookie": "proxysecretcookie=1",
            "X-Forwarded-For": "203.0.113.50",
            "X-Forwarded-Host": "evil.example",
            "Forwarded": "for=203.0.113.50",
            "X-DC24H-Admin-Grant": GRANT,
            "X-DC24H-Session": SESSION,
            "X-DC24H-Csrf": CSRF,
            "Origin": "https://dc24h.eu",
            "Sec-Fetch-Site": "same-origin",
        }
        status, _, body, seen = self.exchange("GET", "/admin-api/v1/status", headers)
        self.assertEqual(status, 200, body)
        self.assertNotIn(b"PROXYSECRETTOKEN", body)
        self.assertNotIn(GRANT.encode(), body)
        item = seen[0]
        self.assertIsNone(item["authorization"])
        self.assertIsNone(item["cookie"])
        self.assertIsNone(item["forwarded"])
        self.assertEqual(item["grant"], GRANT)
        self.assertEqual(item["session"], SESSION)
        self.assertEqual(item["csrf"], CSRF)
        self.assertEqual(item["origin"], self.upstream)
        self.assertEqual(item["host"], self.upstream.removeprefix("http://"))
        status, _, body, seen = self.exchange(
            "POST", "/admin-api/v1/audit",
            {"Content-Type": "application/x-www-form-urlencoded", "X-DC24H-Admin": "1",
             "Origin": "https://dc24h.eu", "X-DC24H-Admin-Grant": "not-a-grant"},
            b"a=b")
        self.assertEqual(status, 200, body)
        self.assertIsNone(seen[0]["grant"])
        status, _, body, seen = self.exchange(
            "POST", "/admin-api/v1/audit",
            {"Content-Type": "application/x-www-form-urlencoded", "X-DC24H-Admin": "1",
             "Origin": "https://dc24h.eu", "X-DC24H-Session": "CD" * 16, "X-DC24H-Csrf": "not-csrf"},
            b"a=b")
        self.assertEqual(status, 200, body)
        self.assertIsNone(seen[0]["session"])
        self.assertIsNone(seen[0]["csrf"])

    def test_disallowed_api_shapes_never_reach_the_adapter(self):
        form = {"Content-Type": "application/x-www-form-urlencoded", "X-DC24H-Admin": "1",
                "Origin": "https://dc24h.eu"}
        denied = [
            ("GET", "/api/v1/health", None, None),
            ("GET", "/api/v1/status", None, None),
            ("GET", "/admin-api", None, None),
            ("GET", "/admin-api/v1/private", None, None),
            ("GET", "/admin-api/v1/status/extra", None, None),
            ("GET", "/admin-api/v1/accounts/0", None, None),
            ("GET", "/admin-api/v1/accounts/15/role", None, None),
            ("GET", "/admin-api/v1/admin/access", None, None),
            ("GET", "/admin-api/v1/bans/15/revoke", None, None),
            ("POST", "/admin-api/v1/status", form, b"x=y"),
            ("POST", "/admin-api/v1/accounts/15", form, b"x=y"),
            ("POST", "/admin-api/v1/management", form, b"operation=configuration.read"),
            ("GET", "/admin-api/v1/management/authorize", None, None),
            ("POST", "/admin-api/v1/management/authorize/extra", form, b"operation=configuration.read"),
            ("POST", "/admin-api/v1/management/write", form, b"operation=configuration.write"),
            ("GET", "/admin-api/v1/status", {"Origin": "https://evil.example"}, None),
            ("GET", "/admin-api/v1/status", {"Sec-Fetch-Site": "cross-site"}, None),
            ("GET", "/admin-api/v1/status", {"Host": "evil.example"}, None),
            ("GET", "/admin-api/v1/status", {"Content-Length": "1"}, b"x"),
            ("POST", "/admin-api/v1/audit", {"Content-Type": "application/x-www-form-urlencoded",
                                            "X-DC24H-Admin": "1"}, b"a=b"),
            ("POST", "/admin-api/v1/audit", {"Content-Type": "text/plain", "X-DC24H-Admin": "1",
                                            "Origin": "https://dc24h.eu"}, b"a=b"),
            ("POST", "/admin-api/v1/audit", {"Content-Type": "application/x-www-form-urlencoded",
                                            "X-DC24H-Admin": "2", "Origin": "https://dc24h.eu"}, b"a=b"),
        ]
        for method, path, headers, body in denied:
            with self.subTest(method=method, path=path, headers=headers):
                self.assert_denied(method, path, headers, body)
        status, _, body, seen = self.exchange("POST", "/admin-api/v1/audit", form, b"a" * 7001)
        self.assertIn(status, (403, 413), body)
        self.assertNotIn(MARKER, body)
        self.assertEqual(seen, [])
        status, headers, body, seen = self.exchange("GET", "/admin-api/v1/status?sample=1", plain=True)
        self.assertEqual(status, 301, body)
        self.assertTrue(headers["Location"].startswith("https://dc24h.eu:"))
        self.assertEqual(seen, [])

    def test_trace_chunked_and_forward_proxy_are_rejected(self):
        before = len(self.backend.snapshot())
        peer = self.tls()
        peer.sendall(b"TRACE /admin-api/v1/status HTTP/1.1\r\nHost: dc24h.eu\r\nConnection: close\r\n\r\n")
        raw = b""
        while chunk := peer.recv(400):
            raw += chunk
        self.assertTrue(b" 405 " in raw or b" 403 " in raw or b" 501 " in raw, raw[:160])
        self.assertNotIn(MARKER, raw)
        peer = self.tls()
        peer.sendall(b"GET http://127.0.0.1:9/secret HTTP/1.1\r\nHost: dc24h.eu\r\nConnection: close\r\n\r\n")
        raw = b""
        while chunk := peer.recv(400):
            raw += chunk
        self.assertNotIn(MARKER, raw)
        peer = self.tls()
        peer.sendall(
            b"POST /admin-api/v1/audit HTTP/1.1\r\nHost: dc24h.eu\r\nTransfer-Encoding: chunked\r\n"
            b"Content-Type: application/x-www-form-urlencoded\r\nX-DC24H-Admin: 1\r\n"
            b"Origin: https://dc24h.eu\r\nConnection: close\r\n\r\n3\r\na=b\r\n0\r\n\r\n")
        raw = b""
        peer.settimeout(3)
        try:
            while chunk := peer.recv(400):
                raw += chunk
        except socket.timeout:
            self.fail("chunked request hung: " + raw[:160].decode("latin1", "replace"))
        self.assertIn(b" 403 ", raw, raw[:160])
        self.assertNotIn(MARKER, raw)
        self.assertEqual(self.backend.snapshot()[before:], [])

    def test_legacy_tls_versions_are_rejected(self):
        for version in [ssl.TLSVersion.TLSv1, ssl.TLSVersion.TLSv1_1]:
            with self.subTest(version=version), warnings.catch_warnings():
                warnings.simplefilter("ignore", DeprecationWarning)
                context = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
                context.check_hostname = False
                context.verify_mode = ssl.CERT_NONE
                context.minimum_version = context.maximum_version = version
                context.set_ciphers("ALL:@SECLEVEL=0")
                with socket.create_connection(("127.0.0.1", self.https_port), timeout=3) as raw:
                    with self.assertRaises(ssl.SSLError) as failure:
                        context.wrap_socket(raw, server_hostname="dc24h.eu")
                    self.assertIn("ALERT_PROTOCOL_VERSION", failure.exception.reason)

    def test_mismatched_key_fails_startup(self):
        wrong_key = self.folder / "wrong.key"
        subprocess.run(["openssl", "genpkey", "-algorithm", "RSA", "-pkeyopt", "rsa_keygen_bits:2048",
                        "-out", str(wrong_key)], check=True, capture_output=True, timeout=15)
        wrong_key.chmod(0o600)
        with socket.socket() as first, socket.socket() as second:
            first.bind(("127.0.0.1", 0))
            second.bind(("127.0.0.1", 0))
            first_port, second_port = first.getsockname()[1], second.getsockname()[1]
        bad = self.folder / "mismatched.conf"
        text = self.prefix + self.vhosts.replace(str(self.key), str(wrong_key))
        for original, replacement in [(":" + str(self.http_port), ":" + str(first_port)),
                                      (":" + str(self.https_port), ":" + str(second_port)),
                                      ("apache.pid", "bad-apache.pid"), ("error.log", "bad-error.log"),
                                      ("ssl-cache", "bad-ssl-cache")]:
            text = text.replace(original, replacement)
        bad.write_text(text)
        result = subprocess.run([APACHE, "-f", str(bad), "-DFOREGROUND"], capture_output=True, text=True, timeout=5)
        self.assertNotEqual(result.returncode, 0)
        errors = (self.folder / "bad-error.log").read_text(errors="replace").lower()
        self.assertTrue("do not match" in errors or "mismatch" in errors, errors)


if __name__ == "__main__":
    unittest.main()
