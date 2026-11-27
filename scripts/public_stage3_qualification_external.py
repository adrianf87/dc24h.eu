#!/usr/bin/env python3
"""External ADC interoperability checks with an independently connected guest."""

import base64
from contextlib import ExitStack, contextmanager, nullcontext
import fcntl
import hashlib
import os
import pty
import secrets
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
from stage3.evidence import Blocked, Mismatch

FORBIDDEN_TCP = (1511, 3000, 3306, 5173, 5432, 6379, 8000, 8080, 9090)
NCDC_TAB = b"stage3qual"


def case(suite, name):
    return suite.case(name) if suite else nullcontext()


def require(condition, message):
    if not condition:
        raise Mismatch(message)


def suffix():
    return secrets.token_hex(6).encode("ascii")


def command_fields(frame, command):
    parts = frame.split()
    if len(parts) > 1 and parts[0] == command:
        return parts
    return []


def attributed(frame, command, marker, expected):
    fields = frame.split()
    if fields and fields[0] == command and marker in fields[1:]:
        require(frame == expected, "Synthetic frame arrived with incorrect attribution or fields")
        return True
    return False


class FrameTimeout(TimeoutError):
    pass


class Reader:
    """Keep buffered frames while waiting for a bounded, specific observation."""

    def __init__(self, peer, sid=None, pending=b""):
        self.peer = peer
        self.sid = sid
        self.pending = pending
        self.message_frames = 0
        self.search_frames = 0

    def until(self, predicate, label, timeout=8):
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
                fields = frame.split()
                if frame.startswith((b"ISTA 1", b"ISTA 2")):
                    status = fields[1]
                    code = status.decode() if len(status) == 3 and status.isdigit() else "unknown"
                    raise Mismatch(f"ADC rejected {label} with ISTA {code}")
                if self.sid and fields[:2] == [b"IQUI", self.sid]:
                    raise Mismatch(f"Own ADC session was disconnected during {label}")
                continue
            remaining = end - time.monotonic()
            if remaining <= 0:
                break
            self.peer.settimeout(min(1, remaining))
            try:
                chunk = self.peer.recv(4096)
            except socket.timeout:
                continue
            if not chunk or len(self.pending) + len(chunk) > 65536:
                raise Mismatch("ADC session closed or exceeded the frame bound during " + label)
            self.pending += chunk
        raise FrameTimeout("Timed out waiting for " + label)

    def send(self, frame):
        self.peer.settimeout(6)
        self.peer.sendall(frame + b"\n")

    def close(self):
        self.peer.close()


@contextmanager
def guest(context, nick, **kwargs):
    peer, sid, pending = base.connect_guest(context, nick, **kwargs)
    try:
        yield Reader(peer, sid, pending)
    finally:
        peer.close()


def see_identity(reader, nick, cid=None, sid=None):
    def match(frame):
        fields = command_fields(frame, b"BINF")
        return (fields and (sid is None or fields[1] == sid) and
                base.guest_identity(frame, nick, cid))

    frame = reader.until(match, "guest identity visibility")
    sid = frame.split()[1]
    require(base.valid_sid(sid), "Guest had an invalid ADC SID")
    return sid


def see_quit(reader, sid):
    reader.until(lambda frame: frame.split()[:2] == [b"IQUI", sid], "guest disconnect")


def check_two_client_protocol(suite, context, settings, *, permissions_fixture=None):
    first_nick, second_nick = b"StageIIIQOne" + suffix(), b"StageIIIQTwo" + suffix()
    first_pair, second_pair = base.pair(None), base.pair(None)
    with ExitStack() as first, ExitStack() as second:
        with case(suite, "guests.visibility"):
            a = first.enter_context(guest(context, first_nick, identity=first_pair, **settings))
            b = second.enter_context(guest(context, second_nick, identity=second_pair, **settings))
            require(a.sid != b.sid, "Two connected guests shared a SID")
            see_identity(a, second_nick, second_pair[1], b.sid)
            see_identity(b, first_nick, first_pair[1], a.sid)
        with case(suite, "chat.public"):
            for sender, receiver in ((a, b), (b, a)):
                marker = b"stage3-public-" + suffix()
                frame = b"BMSG " + sender.sid + b" " + marker
                sender.send(frame)
                receiver.until(lambda line: attributed(line, b"BMSG", marker, frame), "peer public chat")
        with case(suite, "chat.private"):
            for sender, receiver in ((a, b), (b, a)):
                marker = b"stage3-private-" + suffix()
                frame = b"DMSG " + sender.sid + b" " + receiver.sid + b" " + marker + b" PM" + sender.sid
                sender.send(frame)
                receiver.until(lambda line: attributed(line, b"DMSG", marker, frame), "peer private chat")
        with case(suite, "search.broadcast"):
            token, query = b"StageIII" + suffix(), b"StageIIIsearch" + suffix()
            frame = b"BSCH " + a.sid + b" AN" + query + b" TO" + token
            a.send(frame)
            b.until(lambda line: line == frame, "broadcast search")
        with case(suite, "search.direct"):
            token, query = b"StageIIIDirect" + suffix(), b"StageIIISpecific" + suffix()
            frame = b"DSCH " + a.sid + b" " + b.sid + b" AN" + query + b" TO" + token
            a.send(frame)
            b.until(lambda line: line == frame, "directed search")
        with case(suite, "search.result"):
            frame = (b"DRES " + b.sid + b" " + a.sid + b" FN/" + query + b".txt SI12 " +
                     b"TRLWPNACQDBZRYXW3VHJVCJ64QBZNGHOHHHZWCLNQ TO" + token)
            b.send(frame)
            a.until(lambda line: attributed(line, b"DRES", b"TO" + token, frame), "search-result routing")
        with case(suite, "transfer.ctm"):
            # The hub retains its own observed client address. Signal only.
            a.send(b"BINF " + a.sid + b" SUTCP4")
            b.until(lambda line: line.startswith(b"BINF " + a.sid + b" ") and
                    b"SUTCP4" in line.split(), "TCP-active metadata")
            frame = b"DCTM " + a.sid + b" " + b.sid + b" ADCS/1.0 1515 StageIII" + suffix()
            a.send(frame)
            b.until(lambda line: line == frame, "CTM signaling")
        with case(suite, "transfer.rcm"):
            frame = b"DRCM " + b.sid + b" " + a.sid + b" ADC/1.0 StageIII" + suffix()
            b.send(frame)
            a.until(lambda line: line == frame, "RCM signaling")
        with case(suite, "guests.disconnect"):
            second.close()
            see_quit(a, b.sid)
        with case(suite, "guests.reconnect"):
            with guest(context, second_nick, identity=second_pair, **settings) as second:
                require(second.sid not in (a.sid, b.sid), "Guest reconnected with a previous or active SID")
                see_identity(a, second_nick, second_pair[1], second.sid)
                see_identity(second, first_nick, first_pair[1], a.sid)
    if permissions_fixture:
        with case(suite, "guests.permissions"):
            permissions_fixture()


def keyprint(der):
    return base64.b32encode(hashlib.sha256(der).digest()).decode("ascii").rstrip("=")


def fingerprint_url(context, endpoint=(base.IPV4, base.ADCS_PORT), hostname=base.FQDN, *, url_host=None):
    with base.open_tls(context, endpoint, hostname) as peer:
        der = peer.getpeercert(binary_form=True)
    return f"adcs://{url_host or hostname}:{endpoint[1]}/?kp=SHA256/{keyprint(der)}"


def write_terminal(master, wire, timeout=2):
    deadline = time.monotonic() + timeout
    while wire:
        remaining = deadline - time.monotonic()
        if remaining <= 0 or not select.select([], [master], [], remaining)[1]:
            raise Mismatch("ncdc terminal input did not become writable")
        try:
            sent = os.write(master, wire)
        except BlockingIOError:
            continue
        if sent <= 0:
            raise Mismatch("ncdc terminal input write failed")
        wire = wire[sent:]


def send_ncdc(master, command):
    write_terminal(master, b"\x15" + command.encode("ascii") + b"\r")


def select_ncdc_hub_tab(master):
    write_terminal(master, b"\x1b2")


class NcdcTerminal:
    """Observe configured synthetic markers in a rolling terminal buffer."""

    def __init__(self):
        self.activity = False
        self.expected = {}
        self._tail = b""

    def watch(self, marker):
        self.expected = {marker: False}
        self._tail = b""

    def observe(self, chunk):
        self.activity = True
        visible = self._tail + chunk
        for marker in self.expected:
            self.expected[marker] |= marker in visible
        length = max(map(len, self.expected), default=1)
        self._tail = visible[-(length - 1):] if length > 1 else b""


def drain_ncdc_terminal(master, terminal):
    for _ in range(16):
        if not select.select([master], [], [], 0)[0]:
            return
        try:
            chunk = os.read(master, 8192)
        except BlockingIOError:
            return
        except OSError:
            raise Mismatch("ncdc terminal output closed") from None
        if not chunk:
            raise Mismatch("ncdc terminal output ended")
        terminal.observe(chunk)


def await_ncdc_screen(terminal, master, process, marker, timeout=10):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise Mismatch("ncdc exited before the expected terminal update")
        drain_ncdc_terminal(master, terminal)
        if terminal.expected[marker]:
            return
        time.sleep(0.025)
    raise Mismatch("ncdc terminal update timed out; output activity=" + str(terminal.activity))


def await_ncdc_frame(reader, master, process, predicate, label, timeout, terminal):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise Mismatch("ncdc process exited during " + label)
        drain_ncdc_terminal(master, terminal)
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            break
        try:
            return reader.until(predicate, label, timeout=min(0.2, remaining))
        except FrameTimeout:
            continue
    raise Mismatch(f"ncdc observer timed out during {label}; output activity={terminal.activity}; "
                   f"observer message frames={reader.message_frames}; observer search frames={reader.search_frames}")


class Ncdc:
    def __init__(self, session):
        self.terminal = NcdcTerminal()
        self.process = None
        executable = shutil.which("ncdc")
        if not executable:
            raise Blocked("Packaged ncdc is unavailable")
        self.master, slave = pty.openpty()
        try:
            os.set_blocking(self.master, False)
            fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack("HHHH", 36, 140, 0, 0))
            self.process = subprocess.Popen(
                [executable, "-n", "-c", session], stdin=slave, stdout=slave, stderr=slave,
                close_fds=True, start_new_session=True,
                env={"HOME": session, "PATH": os.defpath, "TERM": "xterm", "LC_ALL": "C.UTF-8"},
            )
        except BaseException:
            os.close(self.master)
            raise
        finally:
            os.close(slave)

    def __enter__(self):
        return self

    def __exit__(self, *_):
        try:
            if self.process.poll() is None:
                self.process.terminate()
                try:
                    self.process.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    self.process.kill()
                    self.process.wait(timeout=3)
        finally:
            os.close(self.master)

    def watch(self, marker):
        drain_ncdc_terminal(self.master, self.terminal)
        self.terminal.watch(marker)

    def screen(self, marker):
        await_ncdc_screen(self.terminal, self.master, self.process, marker)

    def send(self, command):
        send_ncdc(self.master, command)

    def select_hub(self):
        select_ncdc_hub_tab(self.master)

    def frame(self, observer, predicate, label, timeout=12):
        return await_ncdc_frame(observer, self.master, self.process, predicate, label, timeout, self.terminal)

    def quit(self):
        self.send("/quit")
        deadline = time.monotonic() + 6
        while self.process.poll() is None:
            if time.monotonic() >= deadline:
                raise Mismatch("ncdc did not exit on /quit")
            if select.select([self.master], [], [], 0.05)[0]:
                # EOF can race with the final process status after a clean exit.
                try:
                    drain_ncdc_terminal(self.master, self.terminal)
                except Mismatch:
                    break
        require(self.process.wait(timeout=2) == 0, "ncdc exited with a nonzero status")


def is_ncdc_search_frame(frame, sender_sid, marker):
    fields = frame.split()
    return (len(fields) > 2 and fields[0] in (b"BSCH", b"FSCH") and fields[1] == sender_sid
            and b"AN" + marker in fields[2:])


def ncdc_identity(frame, nick):
    fields = base.inf_fields(frame)
    return (base.guest_identity(frame, nick) and base.valid_sid(frame.split()[1]) and
            b"ncdc" in b" ".join(fields.get(field, b"").lower() for field in (b"AP", b"VE")) and
            len(fields.get(b"ID", b"")) == 39)


def check_real_ncdc(suite, context, settings, *, url_host=None):
    nick = b"StageIIINcdc" + suffix()
    with ExitStack() as cleanup:
        with case(suite, "ncdc.login"):
            url = fingerprint_url(context, **settings, url_host=url_host)
            profile = cleanup.enter_context(tempfile.TemporaryDirectory(prefix="stage3-ncdc-"))
            observer = cleanup.enter_context(guest(context, b"StageIIIObserver" + suffix(), **settings))
            client = cleanup.enter_context(Ncdc(profile))
            # The observer supports feature-routed searches but never connects to a peer.
            observer.send(b"BINF " + observer.sid + b" SUTCP4")
            observer.until(lambda line: line == b"BINF " + observer.sid + b" SUTCP4", "observer capabilities")
            client.terminal.watch(b"Welcome")
            client.screen(b"Welcome")
            client.send("/set nick " + nick.decode())
            client.send("/open " + NCDC_TAB.decode() + " " + url)
            login = client.frame(observer, lambda line: ncdc_identity(line, nick), "ncdc guest login")
            sid = login.split()[1]
            cid = base.inf_fields(login)[b"ID"]
            require(sid != observer.sid, "ncdc shared its observer's SID")
        with case(suite, "ncdc.inbound"):
            client.select_hub()
            marker = b"stage3-ncdc-in-" + suffix()
            client.watch(marker)
            observer.send(b"BMSG " + observer.sid + b" " + marker)
            client.screen(marker)
        with case(suite, "ncdc.outbound"):
            marker = b"stage3-ncdc-out-" + suffix()
            client.send(marker.decode())
            sent = b"BMSG " + sid + b" " + marker
            client.frame(observer, lambda line: attributed(line, b"BMSG", marker, sent), "ncdc public chat")
        with case(suite, "ncdc.search"):
            marker = b"stage3ncdc" + suffix()
            client.send("/search " + marker.decode())
            client.frame(observer, lambda line: is_ncdc_search_frame(line, sid, marker), "ncdc search")
        with case(suite, "ncdc.reconnect"):
            client.select_hub()
            client.send("/disconnect")
            client.frame(observer, lambda line: line.split()[:2] == [b"IQUI", sid], "ncdc disconnect")
            client.send("/connect")
            login = client.frame(observer, lambda line: ncdc_identity(line, nick), "ncdc reconnect")
            new_sid = login.split()[1]
            require(new_sid not in (sid, observer.sid) and base.inf_fields(login)[b"ID"] == cid,
                    "ncdc did not retain its identity under a new SID")
            marker = b"stage3-ncdc-return-" + suffix()
            client.watch(marker)
            observer.send(b"BMSG " + observer.sid + b" " + marker)
            client.screen(marker)
            client.send(marker.decode() + "reply")
            sent = b"BMSG " + new_sid + b" " + marker + b"reply"
            client.frame(observer, lambda line: attributed(line, b"BMSG", marker + b"reply", sent),
                         "ncdc reconnect chat")
        with case(suite, "ncdc.cleanup"):
            client.quit()
            see_quit(observer, new_sid)
            cleanup.close()
            require(not os.path.exists(profile), "Temporary ncdc profile remained after cleanup")


def public_context():
    context = ssl.create_default_context()
    context.minimum_version = ssl.TLSVersion.TLSv1_2
    context.hostname_checks_common_name = False
    return context


if __name__ == "__main__":
    from stage3.run import main
    raise SystemExit(main())
