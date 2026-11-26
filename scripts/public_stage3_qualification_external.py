#!/usr/bin/env python3
"""Bounded external Stage III public-hub qualification, never a port scan.

Run only with OWNER approval from an independent GitHub-hosted runner.
The ncdc subprocess has an isolated temporary session, no shared files and
no upload/download shares. This script never asks users for credentials.
"""

import argparse
import base64
import fcntl
import hashlib
import os
import pty
import select
import shutil
import socket
import ssl
import struct
import subprocess
import tempfile
import termios
import time

import public_stage3_external_probe as base


FORBIDDEN_TCP = (1511, 3000, 3306, 5173, 5432, 6379, 8000, 8080, 9090)
NCDC_NICK = b"StageIIINcdcQ"
NCDC_CHAT = b"stage3-ncdc-qualified"
NCDC_INBOUND = b"stage3-ncdc-inbound-ready"
NCDC_SEARCH = b"stage3ncdcqualification"
TERM = 20


class Reader:
    """A framed ADC stream, retaining extra frames across socket reads."""

    def __init__(self, peer):
        self.peer = peer
        self.pending = b""
        # Protocol *types* only: never store remote SIDs or message bodies.
        self.message_frames = 0
        self.search_frames = 0

    def until(self, predicate, label, timeout=12):
        end = time.monotonic() + timeout
        while time.monotonic() < end:
            if b"\n" in self.pending:
                frame, self.pending = self.pending.split(b"\n", 1)
                if frame.startswith(b"BMSG "):
                    self.message_frames += 1
                if frame.startswith((b"BSCH ", b"FSCH ")):
                    self.search_frames += 1
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
    # Ctrl+U clears any stale ncurses input before each command; CR submits it.
    # The command is never printed or included in failure diagnostics.
    wire = b"\x15" + command.encode("ascii") + b"\r"
    while wire:
        sent = os.write(master, wire)
        if sent <= 0:
            raise RuntimeError("ncdc terminal input write failed")
        wire = wire[sent:]


def select_ncdc_hub_tab(master):
    # In a new isolated profile the first tab is the main tab and the second
    # is the sole hub. ncdc documents Alt+<num> as an explicit tab selector.
    os.write(master, b"\x1b2")


class NcdcTerminal:
    """Recognize only synthetic markers; never export raw ncurses output."""

    def __init__(self):
        self.hub_tab_visible = False
        self.inbound_chat_visible = False
        self.activity = False
        self._tail = b""

    def observe(self, chunk):
        self.activity = True
        # Only a tiny rollover is kept for markers split across PTY reads.
        # All terminal content is ephemeral, never printed or uploaded.
        visible = self._tail + chunk
        self.hub_tab_visible |= b"stage3qual" in visible
        self.inbound_chat_visible |= NCDC_INBOUND in visible
        self._tail = visible[-(max(len(NCDC_INBOUND), len(b"stage3qual")) - 1):]

    def flags(self):
        return (f"hub_tab_visible={'yes' if self.hub_tab_visible else 'no'}; "
                f"guest_to_ncdc_displayed={'yes' if self.inbound_chat_visible else 'no'}; "
                f"tty_output_seen={'yes' if self.activity else 'no'}")


def drain_ncdc_terminal(master, terminal=None):
    """Consume bounded ncurses output; retain only marker booleans."""
    chunks = 0
    for _ in range(16):
        readable, _, _ = select.select([master], [], [], 0)
        if not readable:
            break
        try:
            chunk = os.read(master, 8192)
        except OSError:
            raise RuntimeError("ncdc terminal output closed") from None
        if not chunk:
            raise RuntimeError("ncdc terminal output ended")
        if terminal is not None:
            terminal.observe(chunk)
        chunks += 1
    return chunks


def await_ncdc_screen(terminal, master, process, timeout=10):
    """Prove that a message from a *different* guest reached ncdc's UI."""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise RuntimeError("ncdc exited before rendering guest chat")
        drain_ncdc_terminal(master, terminal)
        if terminal.inbound_chat_visible:
            return
        time.sleep(0.05)
    raise RuntimeError("ncdc guest chat not rendered; " + terminal.flags())


def await_ncdc_frame(reader, master, process, predicate, label, timeout, terminal=None):
    """Keep the PTY drained while waiting for independent ADC evidence."""
    deadline = time.monotonic() + timeout
    terminal_activity = False
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise RuntimeError(f"ncdc process exited during {label}")
        terminal_activity |= bool(drain_ncdc_terminal(master, terminal))
        try:
            return reader.until(predicate, label, timeout=min(0.35, max(0.1, deadline-time.monotonic())))
        except RuntimeError as error:
            if not str(error).startswith("Timed out waiting for:"):
                raise
    # The only exported PTY observation is a boolean, never ncurses text.
    seen = "yes" if terminal_activity else "no"
    flags = terminal.flags() if terminal else f"tty_output_seen={seen}"
    raise RuntimeError(f"Timed out waiting for: {label}; ncdc_alive=yes; "
                       f"pty_input_submitted=yes; {flags}; "
                       f"observer_BMSG_frames={reader.message_frames}; "
                       f"observer_SCH_frames={reader.search_frames}")


def is_ncdc_chat_frame(frame, sender_sid):
    """A chat is qualified only if the independently observed sender matches ncdc."""
    return frame == b"BMSG " + sender_sid + b" " + NCDC_CHAT


def is_ncdc_search_frame(frame):
    # ncdc may send broadcast SCH or feature-routed SCH depending on mode.
    return frame.startswith((b"BSCH ", b"FSCH ")) and NCDC_SEARCH in frame


def check_real_ncdc():
    if not shutil.which("ncdc"):
        raise RuntimeError("ncdc executable missing (install ncdc on external runner)")
    context = ssl.create_default_context()
    observer = None
    master = None
    process = None
    terminal = NcdcTerminal()
    with tempfile.TemporaryDirectory(prefix="stage3-ncdc-") as session:
        try:
            observer, observer_sid = base.connect_guest(context, b"StageIIIQNcdcObserver", 107)
            peer = Reader(observer)
            # Feature-routed (FSCH) searches reach only compatible recipients.
            # The observer advertises TCP4 for signaling, not an actual transfer.
            observer.sendall(b"BINF " + observer_sid + b" SUTCP4\n")
            peer.until(lambda line: line.startswith(b"BINF " + observer_sid + b" ") and
                       b"SUTCP4" in line, "ncdc observer search capability")
            url = fingerprint_url(context)
            master, slave = pty.openpty()
            try:
                fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack("HHHH", 30, 100, 0, 0))
                env = dict(os.environ, TERM="xterm-256color", LANG="C.UTF-8")
                process = subprocess.Popen(["ncdc", "-n", "-c", session],
                                           stdin=slave, stdout=slave, stderr=slave,
                                           close_fds=True, env=env, start_new_session=True)
            finally:
                os.close(slave)
            # ncurses initialization is asynchronous; drain the bounded PTY.
            time.sleep(2)
            if process.poll() is not None:
                raise RuntimeError("ncdc exited before it connected")
            drain_ncdc_terminal(master, terminal)
            send_ncdc(master, "/set nick " + NCDC_NICK.decode("ascii"))
            time.sleep(0.5)
            drain_ncdc_terminal(master, terminal)
            send_ncdc(master, "/open stage3qual " + url)
            login = await_ncdc_frame(peer, master, process,
                                     lambda line: line.startswith(b"BINF ") and
                                     b"NI" + NCDC_NICK in line,
                                     "real ncdc guest login", timeout=20, terminal=terminal)
            ncdc_sid = login.split(b" ", 2)[1]
            if len(ncdc_sid) != 4:
                raise RuntimeError("ncdc login returned invalid SID length")
            print("STAGE3_NCDC_INDEPENDENT_LOGIN_PASS", flush=True)
            # BINF establishes the independent peer's presence, but does not
            # prove that ncdc has processed the hub tab's user list/UI yet.
            # First select the sole new hub tab, then send a synthetic chat
            # from the observer and require it to appear on ncdc's terminal.
            select_ncdc_hub_tab(master)
            time.sleep(0.35)
            drain_ncdc_terminal(master, terminal)
            observer.sendall(b"BMSG " + observer_sid + b" " + NCDC_INBOUND + b"\n")
            await_ncdc_screen(terminal, master, process, timeout=12)
            if not terminal.hub_tab_visible:
                raise RuntimeError("ncdc hub tab not rendered; " + terminal.flags())
            print("STAGE3_NCDC_TAB_AND_GUEST_TO_CLIENT_CHAT_PASS", flush=True)
            # A successful PTY write is only an input observation. The
            # independent guest must then receive a BMSG from ncdc's SID.
            send_ncdc(master, "/say " + NCDC_CHAT.decode("ascii"))
            await_ncdc_frame(peer, master, process,
                             lambda line: is_ncdc_chat_frame(line, ncdc_sid),
                             "ncdc public chat", timeout=15, terminal=terminal)
            print("STAGE3_NCDC_CLIENT_TO_GUEST_CHAT_PASS", flush=True)
            send_ncdc(master, "/search " + NCDC_SEARCH.decode("ascii"))
            await_ncdc_frame(peer, master, process, is_ncdc_search_frame,
                             "ncdc search broadcast or feature search", timeout=15,
                             terminal=terminal)
            print("STAGE3_NCDC_SEARCH_PASS", flush=True)
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
        print("STAGE3_EXTERNAL_FAIL: " + type(exc).__name__ + ": " + str(exc)[:260])
        raise SystemExit(2)
