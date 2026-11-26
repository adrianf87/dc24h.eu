#!/usr/bin/env python3
"""Bounded external Stage III public-hub qualification, never a port scan.

Run only with OWNER approval from an independent GitHub-hosted runner.
The ncdc subprocess has an isolated temporary session, no shared files and
no upload/download shares. This script never asks users for credentials.
"""

import argparse
import base64
import hashlib
import os
import pty
import select
import shutil
import socket
import ssl
import subprocess
import tempfile
import time

import public_stage3_external_probe as base


FORBIDDEN_TCP = (1511, 3000, 3306, 5173, 5432, 6379, 8000, 8080, 9090)
NCDC_NICK = b"StageIIINcdcQ"
TERM = 20


class Reader:
    """A framed ADC stream, retaining extra frames across socket reads."""

    def __init__(self, peer):
        self.peer = peer
        self.pending = b""

    def until(self, predicate, label, timeout=12):
        end = time.monotonic() + timeout
        while time.monotonic() < end:
            if b"\n" in self.pending:
                frame, self.pending = self.pending.split(b"\n", 1)
                if predicate(frame):
                    return frame
                if frame.startswith((b"ISTA 1", b"ISTA 2")):
                    # Only the numeric status and fixed FC command are safe to
                    # publish: ADC text/extensions may contain private data.
                    fields = frame.split(b" ")
                    status = fields[1].decode("ascii") if len(fields) > 1 and len(fields[1]) == 3 and fields[1].isdigit() else "unknown"
                    command = next((part[2:].decode("ascii") for part in fields[2:]
                                    if len(part) == 6 and part.startswith(b"FC") and
                                    all(65 <= byte <= 90 for byte in part[2:])), "unknown")
                    raise RuntimeError(f"ADC rejected at {label}: ISTA status={status} FC={command}")
                if frame.startswith(b"IQUI "):
                    raise RuntimeError(f"ADC disconnected at {label}: IQUI")
                continue
            self.peer.settimeout(min(2, max(0.1, end - time.monotonic())))
            try:
                chunk = self.peer.recv(4096)
            except socket.timeout:
                continue
            if not chunk or len(self.pending) + len(chunk) > 65536:
                raise RuntimeError("Guest ADC session closed or exceeded frame bound: " + label)
            self.pending += chunk
        raise RuntimeError("Timed out waiting for: " + label)


def check_forbidden_ports():
    for port in FORBIDDEN_TCP:
        try:
            peer = socket.create_connection((base.IPV4, port), timeout=1.5)
        except (OSError, TimeoutError):
            continue
        else:
            peer.close()
            raise RuntimeError("Unexpected publicly accessible TCP port: " + str(port))


def check_two_client_protocol():
    context = ssl.create_default_context()
    context.minimum_version = ssl.TLSVersion.TLSv1_2
    first = second = None
    try:
        first, first_sid = base.connect_guest(context, b"StageIIIQOne", 105)
        second, second_sid = base.connect_guest(context, b"StageIIIQTwo", 106)
        if first_sid == second_sid:
            raise RuntimeError("Duplicate guest SID")
        a, b = Reader(first), Reader(second)
        # Test chat both ways, broadcast search, routed RES, and only CTM/RCM
        # signaling. No peer-to-peer transfer or download is initiated.
        first.sendall(b"BMSG " + first_sid + b" stage3-qualification-one\n")
        b.until(lambda line: line == b"BMSG " + first_sid + b" stage3-qualification-one",
                "first-to-second chat")
        second.sendall(b"BMSG " + second_sid + b" stage3-qualification-two\n")
        a.until(lambda line: line == b"BMSG " + second_sid + b" stage3-qualification-two",
                "second-to-first chat")
        private = b"DMSG " + first_sid + b" " + second_sid + b" stage3-private PM" + first_sid
        first.sendall(private + b"\n")
        b.until(lambda line: line == private, "first-to-second private message")

        token = b"StageIIIQual2026"
        first.sendall(b"BSCH " + first_sid + b" ANstage3qualification TO" + token + b"\n")
        b.until(lambda line: line.startswith(b"BSCH " + first_sid + b" ") and
                b"ANstage3qualification" in line and b"TO" + token in line,
                "broadcast file search")
        second.sendall(b"DRES " + second_sid + b" " + first_sid +
                       b" FN/stage3qualification.txt SI12 "
                       b"TRLWPNACQDBZRYXW3VHJVCJ64QBZNGHOHHHZWCLNQ TO" + token + b"\n")
        a.until(lambda line: line.startswith(b"DRES " + second_sid + b" " + first_sid + b" ") and
                b"TO" + token in line, "direct search result")

        # The login INF's I40.0.0.0 is normalized by the hub to the
        # *client's observed address*. Keep it: advertising the hub's IPv4
        # here is an invalid/forged client address and must be rejected.
        first.sendall(b"BINF " + first_sid + b" SUTCP4\n")
        b.until(lambda line: line.startswith(b"BINF " + first_sid + b" ") and
                b"SUTCP4" in line, "TCP-active signal")
        first.sendall(b"DCTM " + first_sid + b" " + second_sid +
                      b" ADCS/1.0 1515 StageIIIQCTM\n")
        b.until(lambda line: line == b"DCTM " + first_sid + b" " + second_sid +
                b" ADCS/1.0 1515 StageIIIQCTM", "CTM signaling")
        second.sendall(b"DRCM " + second_sid + b" " + first_sid +
                       b" ADC/1.0 StageIIIQRCM\n")
        a.until(lambda line: line == b"DRCM " + second_sid + b" " + first_sid +
                b" ADC/1.0 StageIIIQRCM", "RCM signaling")
    finally:
        if second:
            second.close()
        if first:
            first.close()


def check_prelogin_message_denied():
    context = ssl.create_default_context()
    with socket.create_connection((base.IPV4, 1512), timeout=6) as plain:
        with context.wrap_socket(plain, server_hostname=base.FQDN) as peer:
            peer.settimeout(6)
            pending = b""
            peer.sendall(b"HSUP ADBASE ADTIGR\n")
            first, pending = base.recv_line(peer, pending)
            second, pending = base.recv_line(peer, pending)
            if first != b"ISUP ADBASE ADTIGR" or not second.startswith(b"ISID "):
                raise RuntimeError("Pre-login security test could not handshake")
            sid = second[5:]
            peer.sendall(b"BMSG " + sid + b" forbidden-premature-chat\n")
            denied = Reader(peer)
            denied.pending = pending
            # This unauthenticated protocol action must never be accepted.
            denied.until(lambda frame: frame.startswith(b"ISTA 244 "),
                         "pre-login denial", timeout=6)


def fingerprint_url(context):
    with socket.create_connection((base.IPV4, base.ADCS_PORT), timeout=6) as plain:
        with context.wrap_socket(plain, server_hostname=base.FQDN) as peer:
            der = peer.getpeercert(binary_form=True)
    keyprint = base64.b32encode(hashlib.sha256(der).digest()).decode("ascii").rstrip("=")
    return "adcs://" + base.FQDN + ":1512/?kp=SHA256/" + keyprint


def send_ncdc(master, command):
    os.write(master, command.encode("ascii") + b"\r")


def check_real_ncdc():
    if not shutil.which("ncdc"):
        raise RuntimeError("ncdc executable missing (install ncdc on external runner)")
    context = ssl.create_default_context()
    observer = None
    master = None
    process = None
    with tempfile.TemporaryDirectory(prefix="stage3-ncdc-") as session:
        try:
            observer, _ = base.connect_guest(context, b"StageIIIQNcdcObserver", 107)
            peer = Reader(observer)
            url = fingerprint_url(context)
            master, slave = pty.openpty()
            try:
                env = dict(os.environ, TERM="xterm-256color", LANG="C.UTF-8")
                process = subprocess.Popen(["ncdc", "-n", "-c", session],
                                           stdin=slave, stdout=slave, stderr=slave,
                                           close_fds=True, env=env, start_new_session=True)
            finally:
                os.close(slave)
            # ncurses initialization is asynchronous; consume terminal output.
            time.sleep(2)
            if process.poll() is not None:
                raise RuntimeError("ncdc exited before it connected")
            send_ncdc(master, "/set nick " + NCDC_NICK.decode("ascii"))
            time.sleep(0.5)
            send_ncdc(master, "/open stage3qual " + url)
            end = time.monotonic() + 20
            connected = False
            while time.monotonic() < end:
                if process.poll() is not None:
                    raise RuntimeError("ncdc process exited unexpectedly")
                ready, _, _ = select.select([master], [], [], 0.2)
                if ready:
                    try:
                        os.read(master, 8192)  # Never report terminal contents.
                    except OSError:
                        pass
                try:
                    frame = peer.until(lambda line: line.startswith(b"BINF ") and
                                       b"NI" + NCDC_NICK in line,
                                       "real ncdc guest login", timeout=1)
                except RuntimeError as error:
                    if str(error).startswith("Timed out"):
                        continue
                    raise
                if frame:
                    connected = True
                    break
            if not connected:
                raise RuntimeError("Independent ncdc did not reach guest login")
            send_ncdc(master, "/say stage3-ncdc-qualified")
            peer.until(lambda line: line.startswith(b"BMSG ") and
                       b"stage3-ncdc-qualified" in line, "ncdc public chat", timeout=10)
            send_ncdc(master, "/search stage3ncdcqualification")
            peer.until(lambda line: line.startswith(b"BSCH ") and
                       b"stage3ncdcqualification" in line,
                       "ncdc search broadcast", timeout=10)
        finally:
            if process is not None:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=5)
            if master is not None:
                os.close(master)
            if observer is not None:
                observer.close()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--smoke-only", action="store_true",
                        help="post-restart CA/SAN, guest login and forbidden-port checks")
    args = parser.parse_args()
    def step(name, check):
        try:
            return check()
        except (OSError, RuntimeError, ssl.SSLError, subprocess.SubprocessError) as error:
            raise RuntimeError(f"stage={name}: {error}") from None

    step("dns", base.assert_dns)
    step("public-ports", check_forbidden_ports)
    if args.smoke_only:
        context = ssl.create_default_context()
        peer, _ = step("post-restart-guest-login", lambda: base.connect_guest(context, b"StageIIIPostRestart", 109))
        peer.close()
        print("STAGE3_EXTERNAL_POST_RESTART_TLS_GUEST_AND_PORTS_PASS")
        return
    step("pre-login-denial", check_prelogin_message_denied)
    step("two-client-protocol", check_two_client_protocol)
    step("real-ncdc", check_real_ncdc)
    for seed in range(110, 113):
        context = ssl.create_default_context()
        peer, _ = step("guest-reconnect", lambda: base.connect_guest(context, b"StageIIIReconnect", seed))
        peer.close()
    print("STAGE3_EXTERNAL_NCDC_CHAT_SEARCH_RES_CTM_RCM_RECONNECT_PORTS_PASS")


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, ssl.SSLError, subprocess.SubprocessError) as exc:
        # Avoid leaking network responses, local ncurses or certificate material.
        print("STAGE3_EXTERNAL_FAIL: " + type(exc).__name__ + ": " + str(exc)[:160])
        raise SystemExit(2)
