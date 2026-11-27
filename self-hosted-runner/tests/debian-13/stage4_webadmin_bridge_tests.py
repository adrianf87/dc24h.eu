"""Real AF_UNIX regression; CI must never pass by skipping socket failures."""

import errno
import http.client
from http.server import BaseHTTPRequestHandler
import os
from pathlib import Path
import socket
import socketserver
import tempfile
import threading
import unittest
from unittest import mock

from scripts.stage4_webadmin_bridge import (BridgeServer, HubConnection,
                                            checked_hub_credentials)


class Upstream(BaseHTTPRequestHandler):
    seen = []

    def log_message(self, *_args):
        pass

    def do_POST(self):
        self.respond()

    def do_GET(self):
        self.respond()

    def respond(self):
        Upstream.seen.append((self.command, self.path, dict(self.headers)))
        content = b'{"schema":2,"safe":true}'
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(content)))
        self.end_headers()
        self.wfile.write(content)


class LocalServer(socketserver.ThreadingMixIn, socketserver.UnixStreamServer):
    daemon_threads = True


class BridgeTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="dc24h-stage4-bridge-")
        self.root = Path(self.temp.name)
        self.private = self.root / "hub"
        self.private.mkdir(mode=0o700)
        self.socket = self.private / "api.sock"
        self.token_file = self.private / "api.token"
        self.token_file.write_text("d" * 64 + "\n", encoding="ascii")
        self.token_file.chmod(0o600)
        try:
            self.hub = LocalServer(str(self.socket), Upstream)
        except PermissionError as error:
            # Some local sandboxes prohibit AF_UNIX. CI must fail, not report
            # success with every socket/peer regression silently skipped.
            self.temp.cleanup()
            if error.errno == errno.EPERM and os.environ.get("GITHUB_ACTIONS") != "true":
                self.skipTest("Local sandbox prohibits AF_UNIX; Debian 13 CI must test it")
            raise
        self.socket.chmod(0o600)
        self.hub_worker = threading.Thread(target=self.hub.serve_forever, daemon=True)
        self.hub_worker.start()
        self.bridge_socket = self.root / "bridge.sock"
        self.bridge = BridgeServer(self.bridge_socket, os.geteuid(), self.socket,
                                   checked_hub_credentials(self.socket, self.token_file, os.geteuid()))
        self.worker = threading.Thread(target=self.bridge.serve_forever, daemon=True)
        self.worker.start()
        Upstream.seen = []

    def tearDown(self):
        self.bridge.shutdown()
        self.bridge.server_close()
        self.hub.shutdown()
        self.hub.server_close()
        self.temp.cleanup()

    def call(self, method, path, headers=None, body=None):
        conn = HubConnection(str(self.bridge_socket), timeout=2)
        try:
            conn.request(method, path, body=body, headers=headers or {})
            res = conn.getresponse()
            return res.status, res.read()
        finally:
            conn.close()

    def test_allowlist_real_peer_and_hidden_hub_bearer(self):
        self.assertEqual(self.bridge_socket.stat().st_mode & 0o777, 0o660)
        status, result = self.call("POST", "/api/v1/admin/access",
                                   {"Content-Type": "application/x-www-form-urlencoded",
                                    "Authorization": "Bearer attacker"}, b"proof=synthetic")
        self.assertEqual(status, 200)
        self.assertIn(b'"safe":true', result)
        self.assertEqual(Upstream.seen[-1][0:2], ("POST", "/api/v1/admin/access"))
        self.assertEqual(Upstream.seen[-1][2]["Authorization"], "Bearer " + "d" * 64)
        self.assertEqual(self.call("GET", "/api/v1/management")[0], 200)
        self.assertEqual(self.call("GET", "/api/v1/status")[0], 200)
        self.assertEqual(self.call("GET", "/api/v1/accounts")[0], 404)
        self.assertEqual(self.call("POST", "/api/v1/management")[0], 404)
        self.assertEqual(self.call("GET", "/api/v1/admin/access")[0], 404)
        self.assertEqual(len(Upstream.seen), 3)

    def test_smuggling_and_oversized_password_request_denied(self):
        self.assertEqual(self.call("POST", "/api/v1/admin/access",
                                   {"Content-Type": "application/x-www-form-urlencoded",
                                    "Transfer-Encoding": "chunked"}, b"x=1")[0], 400)
        # The bridge rejects an oversized Content-Length before consuming the
        # body. Depending on socket scheduling, the client may receive HTTP 400
        # or BrokenPipe/connection reset while still transmitting invalid data.
        try:
            status, _ = self.call("POST", "/api/v1/admin/access",
                                  {"Content-Type": "application/x-www-form-urlencoded",
                                   "Content-Length": "16385"}, b"x=1")
        except (BrokenPipeError, ConnectionResetError, http.client.RemoteDisconnected):
            pass
        else:
            self.assertEqual(status, 400)
        self.assertEqual(self.call("GET", "/api/v1/status", {"Content-Length": "2"})[0], 400)
        self.assertEqual(Upstream.seen, [])

    def test_peer_uid_mismatch_is_refused_before_http(self):
        self.bridge.expected_uid = os.geteuid() + 1
        with self.assertRaises((OSError, http.client.HTTPException)):
            self.call("GET", "/api/v1/status")
        self.assertEqual(Upstream.seen, [])

    def test_unsafe_hub_token_is_rejected(self):
        self.token_file.chmod(0o644)
        with self.assertRaises(ValueError):
            checked_hub_credentials(self.socket, self.token_file, os.geteuid())
        self.token_file.chmod(0o600)
        self.token_file.unlink()
        self.token_file.symlink_to(self.socket)
        with self.assertRaises((OSError, ValueError)):
            checked_hub_credentials(self.socket, self.token_file, os.geteuid())


class BridgeRunnerPolicyTests(unittest.TestCase):
    """A denied AF_UNIX socket cannot be turned into a green GitHub CI job."""

    def assert_denied_socket(self, ci, expected_error):
        env = {"GITHUB_ACTIONS": "true" if ci else "false",
               "RUNNER_ENVIRONMENT": "self-hosted", "RUNNER_OS": "Linux"}
        with mock.patch.dict(os.environ, env), mock.patch.object(
                LocalServer, "__init__", side_effect=PermissionError(errno.EPERM, "AF_UNIX denied")):
            case = BridgeTests("test_allowlist_real_peer_and_hidden_hub_bearer")
            with self.assertRaises(expected_error):
                case.setUp()

    def test_denied_af_unix_is_fatal_on_debian13_ci(self):
        self.assert_denied_socket(True, PermissionError)

    def test_denied_af_unix_can_skip_on_local_sandbox(self):
        self.assert_denied_socket(False, unittest.SkipTest)


if __name__ == "__main__":
    unittest.main()
