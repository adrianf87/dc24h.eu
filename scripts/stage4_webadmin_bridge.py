#!/usr/bin/env python3
"""Narrow Stage IV Unix bridge. Never exposes the serving hub token to Node.

This process runs as the *hub* UID but with the WebAdmin primary group. The
kernel peer UID check admits only the separate WebAdmin service account.
The existing C++ API retains its same-UID, mode-0600 security boundary.
"""

import http.client
import os
from pathlib import Path
import pwd
import re
import socket
import socketserver
import stat
import struct
import threading
import time
from http.server import BaseHTTPRequestHandler


BRIDGE_SOCKET = Path("/run/dc24h-stage4-bridge/bridge.sock")
AUTH_SOCKET = Path("/run/dc24h-stage4-auth/api.sock")
AUTH_TOKEN = Path("/var/lib/dc24h-stage4-auth/api.token")
ROUTES = {("POST", "/api/v1/admin/access"),
          ("GET", "/api/v1/management"),
          ("GET", "/api/v1/status"),
          ("GET", "/api/v1/stage4/identity-summary")}
GRANT_HEADERS = ("X-DC24H-Admin-Grant", "X-DC24H-Session", "X-DC24H-Csrf")


def checked_hub_credentials(socket_path: Path, token_path: Path, uid: int) -> str:
    """Reject symlinks, wrong owners, accessible secrets and non-regular tokens."""
    parent = os.lstat(socket_path.parent)
    endpoint = os.lstat(socket_path)
    if (not stat.S_ISDIR(parent.st_mode) or parent.st_uid != uid or
            parent.st_mode & 0o077 or not stat.S_ISSOCK(endpoint.st_mode) or
            endpoint.st_uid != uid or endpoint.st_mode & 0o077):
        raise ValueError("Unsafe private C++ socket")
    fd = os.open(token_path, os.O_RDONLY | os.O_NOFOLLOW | os.O_CLOEXEC | os.O_NONBLOCK)
    try:
        info = os.fstat(fd)
        if (not stat.S_ISREG(info.st_mode) or info.st_uid != uid or
                info.st_nlink != 1 or info.st_mode & 0o077 or
                not 64 <= info.st_size <= 66):
            raise ValueError("Unsafe private C++ token")
        token = os.read(fd, 67).removesuffix(b"\n")
        if not re.fullmatch(rb"[0-9a-fA-F]{64}", token):
            raise ValueError("Invalid private C++ token")
        return token.decode("ascii")
    finally:
        os.close(fd)


class HubConnection(http.client.HTTPConnection):
    def connect(self):
        self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.sock.settimeout(4)
        self.sock.connect(self.host)


class BridgeHandler(BaseHTTPRequestHandler):
    server_version = ""
    sys_version = ""
    protocol_version = "HTTP/1.0"

    def setup(self):
        super().setup()
        self.connection.settimeout(5)

    def log_message(self, *_args):
        # Passwords, grants, mailbox and hub-token material never reach logs.
        pass

    def reply(self, code: int, body: bytes = b'{"error":"unavailable"}'):
        self.send_response_only(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        self.forward()

    def do_POST(self):
        self.forward()

    def forward(self):
        if (self.command, self.path) not in ROUTES:
            self.reply(404, b'{"error":"not_found"}')
            return
        if self.headers.get("Transfer-Encoding") or len(self.headers.get_all("Content-Length", [])) > 1:
            self.reply(400)
            return
        length = self.headers.get("Content-Length")
        if self.command == "POST":
            if (not length or not re.fullmatch(r"[1-9][0-9]{0,4}", length) or
                    int(length) > 16384 or
                    self.headers.get("Content-Type") != "application/x-www-form-urlencoded"):
                self.reply(400)
                return
        elif length not in (None, "0"):
            self.reply(400)
            return
        headers = {"Authorization": "Bearer " + self.server.hub_token,
                   "Accept": "application/json", "Connection": "close",
                   "Host": "localhost"}
        for name in GRANT_HEADERS:
            values = self.headers.get_all(name, [])
            if len(values) > 1 or (values and not re.fullmatch(r"[a-fA-F0-9]{32,128}", values[0])):
                self.reply(400)
                return
            if values:
                headers[name] = values[0]
        body = self.rfile.read(int(length)) if self.command == "POST" else None
        if body is not None:
            headers["Content-Type"] = "application/x-www-form-urlencoded"
            headers["Content-Length"] = str(len(body))
        upstream = HubConnection(str(self.server.hub_socket), timeout=4)
        try:
            upstream.request(self.command, self.path, body=body, headers=headers)
            response = upstream.getresponse()
            result = response.read(65537)
            if (len(result) > 65536 or not response.getheader("Content-Type", "").lower().startswith("application/json") or
                    response.status not in (200, 400, 401, 403, 404, 405, 409, 429, 503)):
                self.reply(502)
            else:
                self.reply(response.status, result)
        except (OSError, http.client.HTTPException):
            self.reply(502)
        finally:
            upstream.close()


class BridgeServer(socketserver.ThreadingMixIn, socketserver.UnixStreamServer):
    daemon_threads = True
    block_on_close = True
    request_queue_size = 8

    def __init__(self, path: Path, expected_uid: int, hub_socket: Path, hub_token: str):
        self.expected_uid = expected_uid
        self.hub_socket = hub_socket
        self.hub_token = hub_token
        self.slots = threading.BoundedSemaphore(8)
        super().__init__(str(path), BridgeHandler)
        os.chmod(path, 0o660)

    def verify_request(self, request, _client_address):
        try:
            credentials = request.getsockopt(socket.SOL_SOCKET, socket.SO_PEERCRED, struct.calcsize("3i"))
            _pid, uid, _gid = struct.unpack("3i", credentials)
            return self.expected_uid != 0 and uid == self.expected_uid and self.slots.acquire(blocking=False)
        except OSError:
            return False

    def process_request_thread(self, request, client_address):
        try:
            super().process_request_thread(request, client_address)
        finally:
            self.slots.release()


def main():
    hub_uid = pwd.getpwnam("dc24h-stage3").pw_uid
    webadmin = pwd.getpwnam("dc24h-webadmin")
    if hub_uid == 0 or webadmin.pw_uid == 0 or hub_uid == webadmin.pw_uid or \
            os.geteuid() != hub_uid or os.getegid() != webadmin.pw_gid:
        raise SystemExit("Stage IV bridge: service identity refused")
    parent = os.lstat(BRIDGE_SOCKET.parent)
    if (not stat.S_ISDIR(parent.st_mode) or parent.st_uid != hub_uid or
            parent.st_gid != webadmin.pw_gid or (parent.st_mode & 0o777) != 0o750):
        raise SystemExit("Stage IV bridge: unsafe runtime directory")
    for attempt in range(40):
        try:
            token = checked_hub_credentials(AUTH_SOCKET, AUTH_TOKEN, hub_uid)
            break
        except FileNotFoundError:
            if attempt == 39:
                raise SystemExit("Stage IV bridge: C++ auth socket is not ready") from None
            time.sleep(0.25)
    if os.path.lexists(BRIDGE_SOCKET):
        raise SystemExit("Stage IV bridge: socket already exists")
    server = BridgeServer(BRIDGE_SOCKET, webadmin.pw_uid, AUTH_SOCKET, token)
    try:
        server.serve_forever(poll_interval=0.2)
    finally:
        server.server_close()
        BRIDGE_SOCKET.unlink(missing_ok=True)


if __name__ == "__main__":
    main()
