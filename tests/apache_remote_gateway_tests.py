"""Disposable local HTTPS Apache integration for beta-0.2.5 proxy isolation."""
import grp
import http.client
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import platform
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
from concurrent.futures import ThreadPoolExecutor

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import stage4_endurance_gate as endurance_gate

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
        (cls.public / "private.key").write_text("PRIVATE_FIXTURE_NOT_FOR_HTTP")
        (cls.public / "api.token").write_text("PRIVATE_FIXTURE_NOT_FOR_HTTP")
        (cls.public / "secret.sql").write_text("PRIVATE_FIXTURE_NOT_FOR_HTTP")
        (cls.public / "assets").mkdir()
        (cls.public / "assets" / "local-test.js").write_text("LOCAL_PRIVATE_ADMIN_UI")
        (cls.public / "assets" / "remote-test.js").write_text("REMOTE_SAFE_ASSET")
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
        modules = ["mpm_event", "authz_core", "authz_host", "headers", "rewrite", "dir", "mime",
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
                   .replace("localhost:8443", "localhost:" + str(cls.port))
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

    @property
    def expected_origin(self):
        return "https://localhost:" + str(self.port)

    def test_allowlisted_routes_and_isolation(self):
        for name in ("challenge", "login", "session", "summary", "logout"):
            method = "POST" if name in ("login", "logout") else "GET"
            status, body, headers = self.fetch("/remote/v1/" + name, method,
                                                {"Origin": self.expected_origin,
                                                 "Sec-Fetch-Site": "none" if name == "challenge" else "same-origin",
                                                 "Cookie": "dc24h_remote=fixture"})
            self.assertEqual(status, 200, (name, status))
            self.assertEqual(body, MARKER)
            self.assertEqual(headers.get("Cache-Control"), "no-store")
            self.assertIn("nosniff", headers.get("X-Content-Type-Options", ""))
            self.assertEqual(headers.get("Cross-Origin-Opener-Policy"), "same-origin")
            self.assertEqual(headers.get("Cross-Origin-Resource-Policy"), "same-origin")
            self.assertIn("camera=()", headers.get("Permissions-Policy", ""))
        forwarded = {item[1] for item in self.backend.seen}
        self.assertEqual(forwarded, {"/remote/v1/" + route for route in
                                     ("challenge", "login", "session", "summary", "logout")})

    def test_browser_credentials_stripped(self):
        previous = len(self.backend.seen)
        status, _, _ = self.fetch("/remote/v1/summary", headers={
            "Authorization": "Bearer browser-secret",
            "Proxy-Authorization": "Basic browser-secret",
            "X-DC24H-Admin-Grant": "browser-grant",
            "X-DC24H-Session": "browser-session",
            "X-DC24H-Csrf": "browser-csrf",
            "X-Forwarded-For": "8.8.8.8",
            "X-Forwarded-Port": "8443",
            "X-Forwarded-Prefix": "/admin-api",
            "X-Forwarded-User": "owner",
            "X-Forwarded-Client-Cert": "fake-cert",
            "X-Forwarded-Uri": "/admin-api/v1/status",
            "X-Forwarded-Method": "POST",
            "Forwarded": "for=8.8.8.8",
            "X-Original-URL": "/api/v1/status",
            "X-Rewrite-URL": "/api/v1/status",
            "Cookie": "dc24h_remote=fixture",
        })
        self.assertEqual(status, 200)
        headers = {key.lower(): value for key, value in self.backend.seen[previous][2].items()}
        for key in ("authorization", "proxy-authorization", "x-dc24h-admin-grant", "x-dc24h-session",
                    "x-dc24h-csrf", "x-forwarded-for", "x-forwarded-port", "x-forwarded-prefix",
                    "x-forwarded-user", "x-forwarded-client-cert", "x-forwarded-uri", "x-forwarded-method",
                    "forwarded", "x-original-url", "x-rewrite-url"):
            self.assertNotIn(key, headers)
        self.assertEqual(headers.get("cookie"), "dc24h_remote=fixture")

    def test_private_paths_never_forward(self):
        for path in ("/api/v1/status", "/admin-api/v1/management", "/management",
                     "/remote/v1/admin", "/remote/v1/wildcard", "/remote/v1/challenge/",
                     "/remote/v1//challenge",
                     "/remote/v1/Challenge", "/remote/v1/%73ummary",
                     "/remote/v1/summary%2F", "/remote/v1/summary;ignored=1",
                     "/remote/v1/../api/v1/status", "/remote/v1/%2e%2e/api/v1/status",
                     "/.env", "/secret.ini", "/private.key", "/api.token",
                     "/secret.sql", "/index.html", "/assets/local-test.js"):
            before = len(self.backend.seen)
            status, body, _ = self.fetch(path)
            self.assertNotEqual(status, 200, path)
            self.assertNotIn(MARKER, body)
            self.assertNotIn(b"PRIVATE_FIXTURE_NOT_FOR_HTTP", body)
            self.assertEqual(len(self.backend.seen), before, path)

    def test_adversarial_paths_are_never_normalized_into_private_proxy(self):
        corpus = [
            "/remote/v1/%2573ummary", "/remote/v1/summary%2f", "/remote/v1/summary%5c",
            "/remote/v1/summary;ignored", "/remote/v1/summary%00", "/remote/v1/summary%0a",
            "/remote/v1/./summary", "/remote/v1/../v1/summary", "/remote/v1/%2e/summary",
            "/remote/v1/%2e%2e/v1/summary", "/remote/v1/passkey/device/revoke/",
            "/remote/v1/passkey/device/%72evoke", "/remote/v1/passkey/enroll/finish%2f",
            "/remote/v1/summary?admin=true", "/admin-api/v1/status%00",
        ]
        for path in corpus:
            with self.subTest(path=path):
                before = len(self.backend.seen)
                status, body, _ = self.fetch(path)
                self.assertNotEqual(status, 200, path)
                self.assertNotIn(MARKER, body)
                self.assertEqual(len(self.backend.seen), before, path)

    def test_bounded_https_api_and_portal_concurrency(self):
        # Exercises real loopback Apache TLS/proxy/HTML, with a synthetic API
        # upstream. It deliberately does NOT claim an authenticated Node or
        # C++ API request latency baseline.
        if os.environ.get("GITHUB_ACTIONS") != "true":
            self.skipTest("Stage IV HTTPS endurance is reserved for hosted Ubuntu CI")
        self.assertEqual(os.environ.get("RUNNER_ENVIRONMENT"), "github-hosted")
        self.assertEqual(os.environ.get("RUNNER_OS"), "Linux")
        import time

        def rss_kib():
            status = Path(f"/proc/{self.server.pid}/status").read_text(encoding="ascii")
            return int(next(line.split()[1] for line in status.splitlines() if line.startswith("VmRSS:")))

        reports = {}
        for profile, path, baseline_test_ms in (
            ("api_proxy_fixture", "/remote/v1/summary", 1956.9),
            ("webadmin_static_html", "/remote.html", 4141.0),
        ):
            before = rss_kib()
            started = time.monotonic()

            def once(_):
                begin = time.monotonic()
                status, body, _ = self.fetch(path, headers={"Cookie": "dc24h_remote=fixture"})
                return status, body, (time.monotonic() - begin) * 1000

            with ThreadPoolExecutor(max_workers=4) as pool:
                samples = list(pool.map(once, range(48)))
            elapsed = time.monotonic() - started
            self.assertTrue(all(status == 200 and body for status, body, _ in samples), profile)
            latencies = sorted(sample[2] for sample in samples)
            p99 = round(latencies[-1], 3)
            self.assertLess(p99, 4000, profile + " bounded HTTPS CI regression")
            reports[profile] = {
                "requests": 48, "errors": 0, "concurrency": 4,
                "p50_ms": round(latencies[23], 3), "p95_ms": round(latencies[45], 3),
                "p99_ms": p99, "successful_rps": round(48 / elapsed, 3),
                "apache_rss_before_kib": before, "apache_rss_after_kib": rss_kib(),
                "stage1_test_duration_p99_ms": baseline_test_ms,
                "stage1_request_level_baseline_available": False,
            }
        destination = os.environ.get("DC24H_VM_REPORT_DIR")
        if destination:
            path = Path(destination) / "stage4-https-endurance.json"
            with path.open("x", encoding="utf-8") as file:
                json.dump({"schema": 1, "evidence": "hosted-ubuntu-apache-https-fixture",
                           "source_sha": endurance_gate.required_source_sha(
                               os.environ.get("SOURCE_SHA")),
                           "os": "ubuntu-" + platform.freedesktop_os_release()["VERSION_ID"],
                           "stage1_run": 45283779224, "profiles": reports,
                           "production_capacity_claim": False,
                           "note": "Stage I API/browser figures are test durations, not HTTP request latency. No direct numerical regression inference; API backend here is a synthetic fixture."},
                          file, indent=2, sort_keys=True)
                file.write("\n")

    def test_host_method_query_and_origin_denied_before_proxy(self):
        cases = [
            ("/remote.html", "GET", {"Host": "evil.example"}, None),
            ("/remote/v1/summary", "GET", {"Host": "evil.example"}, None),
            ("/remote/v1/summary", "GET", {"Host": "127.0.0.1:" + str(self.port)}, None),
            ("/remote/v1/login", "GET", {}, None),
            ("/remote/v1/challenge", "POST", {"Origin": self.expected_origin}, b"{}"),
            ("/remote/v1/session", "HEAD", {}, None),
            ("/remote/v1/summary", "OPTIONS", {}, None),
            ("/remote/v1/logout", "PUT", {"Origin": self.expected_origin}, b"{}"),
            ("/remote/v1/summary?secret=1", "GET", {}, None),
            ("/remote/v1/login", "POST", {"Origin": "https://evil.example"}, b"{}"),
            ("/remote/v1/logout", "POST", {}, b"{}"),
            ("/remote/v1/challenge", "GET", {"Origin": "https://evil.example"}, None),
            ("/remote/v1/summary", "GET", {"Sec-Fetch-Site": "cross-site"}, None),
            ("/remote/v1/session", "GET", {"Sec-Fetch-Site": "same-site"}, None),
            ("/remote/v1/login", "POST", {"Origin": self.expected_origin,
                                              "Sec-Fetch-Site": "cross-site"}, b"{}"),
            ("/remote/v1/passkey/enroll/options", "POST", {}, b"{}"),
            ("/remote/v1/passkey/enroll/options", "POST", {"Origin": "https://evil.example"}, b"{}"),
            ("/remote/v1/passkey/device/revoke", "POST", {"Origin": "null"}, b"{}"),
            ("/remote/v1/passkey/login/finish", "GET", {}, None),
            ("/remote/v1/passkey/login/options?challenge=1", "POST", {"Origin": self.expected_origin}, b"{}"),
            ("/remote/v1/passkey/device/rename", "POST",
             {"Origin": self.expected_origin, "Sec-Fetch-Site": "cross-site"}, b"{}"),
        ]
        for path, method, headers, body in cases:
            with self.subTest(path=path, method=method, headers=headers):
                before = len(self.backend.seen)
                status, data, _ = self.fetch(path, method, headers, body)
                self.assertNotEqual(status, 200)
                self.assertNotIn(MARKER, data)
                self.assertEqual(len(self.backend.seen), before)

    def test_static_portal_only_and_oversized_request_denied(self):
        for path in ("/", "/remote.html", "/assets/remote-test.js"):
            status, _, _ = self.fetch(path)
            self.assertEqual(status, 200, path)
        status, data, _ = self.fetch("/remote/v1/login", "POST",
                                     {"Origin": self.expected_origin, "Content-Type": "application/json"},
                                     b"x" * 2048)
        self.assertEqual(status, 200)
        self.assertEqual(data, MARKER)
        before = len(self.backend.seen)
        status, body, _ = self.fetch("/remote/v1/login", "POST",
                                     {"Origin": self.expected_origin, "Content-Type": "application/json"},
                                     b"x" * 2049)
        self.assertNotEqual(status, 200)
        self.assertNotIn(MARKER, body)
        self.assertEqual(len(self.backend.seen), before)
        status, body, _ = self.fetch("/remote/v1/passkey/enroll/finish", "POST",
                                     {"Origin": self.expected_origin, "Content-Type": "application/json"},
                                     b"x" * 32769)
        self.assertNotEqual(status, 200)
        self.assertNotIn(MARKER, body)
        self.assertEqual(len(self.backend.seen), before)

    def test_certificate_hostname_and_trust_are_verified_by_client(self):
        trusted = ssl.create_default_context(cafile=str(self.cert))
        trusted.minimum_version = ssl.TLSVersion.TLSv1_2
        # The fixture certificate covers localhost, not unrelated domains.
        with socket.create_connection(("127.0.0.1", self.port), timeout=4) as raw:
            with self.assertRaises(ssl.SSLCertVerificationError):
                with trusted.wrap_socket(raw, server_hostname="evil.example"):
                    pass
        untrusted = ssl.create_default_context()
        untrusted.minimum_version = ssl.TLSVersion.TLSv1_2
        with socket.create_connection(("127.0.0.1", self.port), timeout=4) as raw:
            with self.assertRaises(ssl.SSLCertVerificationError):
                with untrusted.wrap_socket(raw, server_hostname="localhost"):
                    pass

    def test_chunked_post_denied_before_proxy(self):
        before = len(self.backend.seen)
        context = ssl.create_default_context(cafile=str(self.cert))
        conn = http.client.HTTPSConnection("localhost", self.port, context=context, timeout=4)
        try:
            conn.request("POST", "/remote/v1/login", body=[b"{}"],
                         headers={"Origin": self.expected_origin,
                                  "Content-Type": "application/json",
                                  "Transfer-Encoding": "chunked"}, encode_chunked=True)
            response = conn.getresponse()
            self.assertNotEqual(response.status, 200)
            self.assertNotIn(MARKER, response.read())
        finally:
            conn.close()
        self.assertEqual(len(self.backend.seen), before)

    def test_get_request_bodies_denied_before_proxy(self):
        # A GET body cannot be forwarded merely because ProxyPassMatch
        # accepted the fixed path. An explicit zero length remains harmless.
        status, data, _ = self.fetch("/remote/v1/summary", headers={"Content-Length": "0"})
        self.assertEqual(status, 200)
        self.assertEqual(data, MARKER)
        for name in ("challenge", "session", "summary"):
            before = len(self.backend.seen)
            status, body, _ = self.fetch("/remote/v1/" + name, body=b"x")
            self.assertNotEqual(status, 200, name)
            self.assertNotIn(MARKER, body)
            self.assertEqual(len(self.backend.seen), before, name)

        before = len(self.backend.seen)
        context = ssl.create_default_context(cafile=str(self.cert))
        conn = http.client.HTTPSConnection("localhost", self.port, context=context, timeout=4)
        try:
            conn.request("GET", "/remote/v1/summary", body=[b"x"],
                         headers={"Transfer-Encoding": "chunked"}, encode_chunked=True)
            response = conn.getresponse()
            self.assertNotEqual(response.status, 200)
            self.assertNotIn(MARKER, response.read())
        finally:
            conn.close()
        self.assertEqual(len(self.backend.seen), before)

    def test_plain_http_never_downgrades_to_the_remote_portal(self):
        before = len(self.backend.seen)
        reply = b""
        try:
            with socket.create_connection(("127.0.0.1", self.port), timeout=3) as conn:
                conn.settimeout(3)
                conn.sendall(("GET /remote.html HTTP/1.1\r\nHost: localhost:" +
                              str(self.port) + "\r\nConnection: close\r\n\r\n").encode())
                reply = conn.recv(4096)
        except (ConnectionError, OSError):
            pass  # TLS servers may immediately close a plaintext connection.
        self.assertNotIn(b"200 OK", reply)
        self.assertNotIn(b"Remote portal fixture", reply)
        self.assertEqual(len(self.backend.seen), before)

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

    def test_tls_12_and_13_negotiate_authenticated_aead(self):
        for version in (ssl.TLSVersion.TLSv1_2, ssl.TLSVersion.TLSv1_3):
            with self.subTest(version=version.name):
                context = ssl.create_default_context(cafile=str(self.cert))
                context.minimum_version = version
                context.maximum_version = version
                conn = http.client.HTTPSConnection("localhost", self.port, context=context, timeout=4)
                try:
                    conn.request("GET", "/remote.html")
                    cipher = conn.sock.cipher()[0]
                    self.assertEqual(conn.getresponse().status, 200)
                    self.assertTrue("GCM" in cipher or "CHACHA20" in cipher, cipher)
                finally:
                    conn.close()


if __name__ == "__main__":
    unittest.main()
