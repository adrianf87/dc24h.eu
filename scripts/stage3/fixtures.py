"""Generated loopback TLS and hub fixtures. Test keys are never evidence."""

import configparser
from contextlib import contextmanager
from datetime import datetime, timedelta, timezone
import hashlib
import os
from pathlib import Path
import re
import socket
import ssl
import struct
import subprocess
import tempfile
from threading import Thread
import time

import public_stage3_external_probe as base
from public_stage3_qualification_external import (
    Ncdc, check_real_ncdc, check_two_client_protocol, fingerprint_url, guest,
    keyprint, ncdc_identity, require, see_identity, see_quit, suffix,
)

LOOPBACK = "127.0.0.1"
READY = re.compile(rb"ADCS listening on 127\.0\.0\.1:([0-9]+)")

VERIFY_CATEGORIES = {
    9: "certificate not yet valid",
    10: "certificate expired",
    20: "local issuer not found",
    30: "authority and subject key identifier mismatch",
    62: "DNS hostname mismatch",
}


def run(*command, input_data=None):
    return subprocess.run(command, input=input_data, capture_output=True, check=True, timeout=30).stdout


def openssl(*args, input_data=None):
    return run("openssl", *map(str, args), input_data=input_data)


def authority(root):
    root.mkdir()
    config = root / "openssl.cnf"
    config.write_text(f"""[ca]
default_ca = stage3
[stage3]
database = {root / 'index'}
serial = {root / 'serial'}
new_certs_dir = {root}
certificate = {root / 'ca.pem'}
private_key = {root / 'ca.key'}
default_md = sha256
policy = stage3_dn
[stage3_dn]
commonName = supplied
""")
    (root / "index").touch()
    (root / "index.attr").write_text("unique_subject = no\n")
    (root / "serial").write_text("1000\n")
    # Separate issuer DNs keep older OpenSSL from considering the trusted
    # CA as an issuer candidate for the leaf signed by the other CA.
    openssl("req", "-new", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "7",
            "-subj", f"/CN=Stage III synthetic {root.name} CA",
            "-addext", "basicConstraints=critical,CA:TRUE",
            "-addext", "keyUsage=critical,keyCertSign,cRLSign",
            "-out", root / "ca.pem", "-keyout", root / "ca.key")
    return config


def issue(root, issuer, label, *, san="localhost", start=-1, end=1):
    certificate = root / (label + ".pem")
    key = root / (label + ".key")
    request = root / (label + ".csr")
    extensions = root / (label + ".ext")
    extensions.write_text(f"""basicConstraints=critical,CA:FALSE
keyUsage=critical,digitalSignature,keyEncipherment
extendedKeyUsage=serverAuth
subjectAltName=DNS:{san}
""")
    openssl("req", "-new", "-newkey", "rsa:2048", "-nodes", "-subj", "/CN=localhost",
            "-out", request, "-keyout", key)
    issued = datetime.now(timezone.utc)
    dates = [(issued + timedelta(days=days)).strftime("%Y%m%d%H%M%SZ") for days in (start, end)]
    openssl("ca", "-config", issuer, "-batch", "-notext", "-in", request, "-out", certificate,
            "-extfile", extensions, "-startdate", dates[0], "-enddate", dates[1])
    return certificate, key


def loopback_context(ca):
    context = ssl.create_default_context(cafile=str(ca))
    context.hostname_checks_common_name = False
    context.minimum_version = ssl.TLSVersion.TLSv1_2
    return context


def write_config(path, pair, *, idle=25, port=0, policy=""):
    cert, key = pair
    path.write_text(f"""[network]
listen=none
adcs_listen={LOOPBACK}:{port}
idle_timeout={idle}
max_connections=8

[tls]
certificate={cert}
private_key={key}

[database]
enabled=off

[authentication]
enabled=off

[api]
enabled=off

{policy}
""")


@contextmanager
def hub(binary, config):
    with tempfile.TemporaryFile() as log:
        with subprocess.Popen([binary, "--config", str(config)], cwd=config.parent,
                              stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=log) as process:
            try:
                deadline = time.monotonic() + 6
                while time.monotonic() < deadline and process.poll() is None:
                    transcript = os.pread(log.fileno(), 8192, 0)
                    ready = READY.search(transcript)
                    if ready:
                        require(b"Network listening on" not in transcript and b"Local API listening" not in transcript,
                                "An undeclared plaintext or API listener started")
                        yield {"endpoint": (LOOPBACK, int(ready[1])), "hostname": "localhost"}, process
                        return
                    time.sleep(0.025)
                require(False, "ADCS-only loopback hub did not become ready")
            finally:
                if process.poll() is None:
                    process.terminate()
                    try:
                        process.wait(timeout=3)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait(timeout=3)


def local_credentials(root):
    issuer = authority(root / "trusted")
    pair = issue(root, issuer, "original")
    return pair, loopback_context(issuer.parent / "ca.pem"), issuer


def pair_hash(pair):
    certificate, key = pair
    leaf_public = openssl("x509", "-in", certificate, "-pubkey", "-noout")
    public_from_cert = openssl("pkey", "-pubin", "-outform", "DER", input_data=leaf_public)
    public_from_key = openssl("pkey", "-in", key, "-pubout", "-outform", "DER")
    require(hashlib.sha256(public_from_cert).digest() == hashlib.sha256(public_from_key).digest(),
            "Certificate and key did not match")


def check_pair_recovery(binary, config, original, replacement):
    from shutil import copyfile
    root = config.parent
    backup = (root / "backup.pem", root / "backup.key")
    installed = (root / "installed.pem", root / "installed.key")
    for source, saved, deployed in zip(original, backup, installed):
        copyfile(source, saved)
        copyfile(source, deployed)
        saved.chmod(0o600)
        deployed.chmod(0o600)
    def config_accepts(pair, expected):
        write_config(config, pair)
        result = subprocess.run([binary, "--config", str(config), "--check-config"],
                                capture_output=True, timeout=10)
        require((result.returncode == 0) == expected, "C++ configuration accepted the wrong TLS pair state")

    pair_hash(installed)
    config_accepts(installed, True)
    config_accepts((installed[0], replacement[1]), False)
    pair_hash(replacement)
    for source, deployed in zip(replacement, installed):
        copyfile(source, deployed)
    config_accepts(installed, True)
    for saved, deployed in zip(backup, installed):
        copyfile(saved, deployed)
    require(tuple(path.read_bytes() for path in installed) == tuple(path.read_bytes() for path in original),
            "Restored pair differed from the backup")
    pair_hash(installed)
    config_accepts(installed, True)


def ncdc_open(client, nick, url):
    client.terminal.watch(b"Welcome")
    client.screen(b"Welcome")
    client.send("/set nick " + nick.decode())
    client.send("/open stage3pin " + url)


def assert_pin_rejected(url):
    with tempfile.TemporaryDirectory(prefix="stage3-ncdc-pin-") as profile, Ncdc(profile) as client:
        warning = b"TLS certificate of this hub has changed!"
        # Start watching after the command is sent without discarding its reply.
        client.terminal.watch(b"Welcome")
        client.screen(b"Welcome")
        client.watch(warning)
        client.send("/open stage3pin " + url)
        client.screen(warning)
        client.quit()


def assert_pin_accepted(context, settings, url):
    nick = b"StageIIIPin" + suffix()
    with tempfile.TemporaryDirectory(prefix="stage3-ncdc-pin-") as profile, Ncdc(profile) as client:
        with guest(context, b"StageIIIPinWitness" + suffix(), **settings) as observer:
            ncdc_open(client, nick, url)
            login = client.frame(observer, lambda line: ncdc_identity(line, nick), "pinned ncdc login")
            client.quit()
            see_quit(observer, login.split()[1])


@contextmanager
def certificate_server(pair):
    server = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    server.load_cert_chain(*pair)
    with socket.socket() as listener:
        listener.bind((LOOPBACK, 0))
        listener.listen(1)
        listener.settimeout(4)
        errors = []

        def handshake():
            try:
                raw, _ = listener.accept()
                with raw:
                    raw.settimeout(4)
                    try:
                        with server.wrap_socket(raw, server_side=True):
                            return
                    except ssl.SSLError:
                        # Client validation can reject the certificate in the handshake.
                        return
            except OSError as error:
                errors.append(error)

        thread = Thread(target=handshake, daemon=True)
        thread.start()
        try:
            yield {"endpoint": listener.getsockname(), "hostname": "localhost"}
        finally:
            thread.join(timeout=9)
            require(not thread.is_alive() and not errors, "Loopback TLS fixture did not complete")


def verify_certificate_rejection(context, settings, expected):
    try:
        with base.open_tls(context, **settings):
            pass
    except ssl.SSLCertVerificationError as error:
        code = error.verify_code
        category = VERIFY_CATEGORIES.get(code, "other certificate verification error")
        observed = f"OpenSSL verification code {code} ({category})"
        require(code == expected, f"{observed}. Expected {expected} ({VERIFY_CATEGORIES[expected]})")
        return observed
    require(False, "TLS accepted a deliberately invalid certificate")


def tls_fixtures(suite, binary, root):
    original, context, issuer = local_credentials(root)
    config = root / "hub.ini"
    write_config(config, original)
    with suite.case("tls.control"):
        with hub(binary, config) as (settings, _):
            with base.open_tls(context, **settings):
                pass

    untrusted = authority(root / "other")
    cases = (("tls.untrusted", untrusted, "localhost", -1, 1, 20),
             ("tls.san", issuer, "wrong.invalid", -1, 1, 62),
             ("tls.expired", issuer, "localhost", -3, -2, 10),
             ("tls.future", issuer, "localhost", 2, 3, 9))
    for identifier, ca, san, start, end, expected in cases:
        with suite.case(identifier) as outcome:
            pair = issue(root, ca, identifier, san=san, start=start, end=end)
            with certificate_server(pair) as settings:
                outcome.observed = verify_certificate_rejection(context, settings, expected)

    replacement = issue(root, issuer, "replacement")
    write_config(config, original)
    with hub(binary, config) as (settings, _):
        with suite.case("keyp.correct"):
            url = fingerprint_url(context, **settings, url_host=LOOPBACK)
            assert_pin_accepted(context, settings, url)
        with suite.case("keyp.incorrect"):
            incorrect = url.split("SHA256/")[0] + "SHA256/" + keyprint(b"unrelated synthetic certificate")
            assert_pin_rejected(incorrect)

    write_config(config, replacement, port=settings["endpoint"][1])
    with hub(binary, config) as (rotated, _):
        with suite.case("keyp.stale"):
            require(rotated == settings, "Rotation did not reuse the same isolated endpoint")
            assert_pin_rejected(url)
        with suite.case("keyp.rotation"):
            new_url = fingerprint_url(context, **rotated, url_host=LOOPBACK)
            require(new_url != url, "Replacement certificate reused the previous pin")
            assert_pin_accepted(context, rotated, new_url)
    with suite.case("tls.pair.rollback"):
        check_pair_recovery(binary, config, original, replacement)


def two_clients(suite, binary, root):
    pair, context, _ = local_credentials(root)
    config = root / "hub.ini"
    write_config(config, pair)
    def permissions_fixture():
        denied = root / "denied.ini"
        write_config(denied, pair, policy="[chat]\npublic_enabled=off\nprivate_enabled=off\n"
                                        "[search]\nguest_queries=off\n")
        with hub(binary, denied) as (settings, _):
            with guest(context, b"StageIIIPermissionA", **settings) as a:
                with guest(context, b"StageIIIPermissionB", **settings) as b:
                    see_identity(a, b"StageIIIPermissionB", sid=b.sid)
                    see_identity(b, b"StageIIIPermissionA", sid=a.sid)
                    for command, parameters in (
                        (b"BMSG", b" stage3-denied"),
                        (b"DMSG", b" " + b.sid + b" stage3-denied PM" + a.sid),
                        (b"DSCH", b" " + b.sid + b" ANStageIIIDenied"),
                    ):
                        frame = command + b" " + a.sid + parameters
                        a.send(frame)
                        a.until(lambda line: line.startswith(b"ISTA 125 ") and
                                b"FC" + command in line.split(), "guest permission confirmation")
                        # A subsequent allowed update fences the receiver queue.
                        fence = b"BINF " + a.sid + b" DEAllowed" + suffix()
                        a.send(fence)
                        received = b.until(lambda line: line == frame or line == fence, "permission relay fence")
                        require(received == fence, "A denied guest command reached its target")
    with hub(binary, config) as (settings, _):
        check_two_client_protocol(suite, context, settings, permissions_fixture=permissions_fixture)


def stability(suite, binary, root):
    pair, context, _ = local_credentials(root)
    config = root / "hub.ini"
    write_config(config, pair)
    with suite.case("stability.config"):
        run(binary, "--config", str(config), "--check-config")
        parsed = configparser.ConfigParser()
        parsed.read(config)
        require(parsed["network"]["listen"] == "none" and parsed["network"]["adcs_listen"] == "127.0.0.1:0"
                and all(not parsed.getboolean(section, "enabled") for section in ("api", "database", "authentication")),
                "Configuration permitted a Stage III-excluded facility")
    with hub(binary, config) as (settings, process):
        with suite.case("stability.cycles") as result:
            with guest(context, b"StageIIILoopObserver", **settings) as observer:
                baseline = len(os.listdir(f"/proc/{process.pid}/fd"))
                used = {observer.sid}
                identity = base.pair(None)
                for _ in range(20):
                    with guest(context, b"StageIIICycle", identity=identity, **settings) as peer:
                        require(peer.sid not in used, "Hub assigned a previous or active SID")
                        used.add(peer.sid)
                        see_identity(observer, b"StageIIICycle", identity[1], peer.sid)
                    see_quit(observer, peer.sid)
                deadline = time.monotonic() + 2
                while len(os.listdir(f"/proc/{process.pid}/fd")) > baseline and time.monotonic() < deadline:
                    time.sleep(0.02)
                require(len(os.listdir(f"/proc/{process.pid}/fd")) <= baseline,
                        "Hub descriptors accumulated across guest cycles")
            result.observed = "20 join/quit cycles with unique SIDs, one reused CID, and no descriptor growth"
        with suite.case("stability.refusal") as result:
            with socket.socket() as bound:
                # Holding a loopback bind without listening guarantees controlled refusal.
                bound.bind((LOOPBACK, 0))
                identity = base.pair(None)
                seen = set()
                for _ in range(10):
                    try:
                        with socket.create_connection(bound.getsockname(), timeout=1):
                            require(False, "Intentional refusal fixture accepted TCP")
                    except ConnectionRefusedError:
                        pass
                    with guest(context, b"StageIIIRefused", identity=identity, **settings) as peer:
                        require(peer.sid not in seen, "Repeated return reused a SID")
                        seen.add(peer.sid)
            result.observed = "10 intentional TCP refusals followed by verified same-CID ADCS logins"
        with suite.case("stability.interruption"):
            identity = base.pair(None)
            with guest(context, b"StageIIIAbortWitness", **settings) as observer:
                with guest(context, b"StageIIIAbort", identity=identity, **settings) as peer:
                    see_identity(observer, b"StageIIIAbort", identity[1], peer.sid)
                    peer.peer.sendall(b"BMSG " + peer.sid + b" staged-partial")
                    peer.peer.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack("ii", 1, 0))
                observer.until(lambda line: line.split()[:2] == [b"IQUI", peer.sid] or
                               line.startswith(b"BMSG " + peer.sid + b" "), "interrupted message")
                require(observer.message_frames == 0, "An incomplete message was delivered")
                with guest(context, b"StageIIIAbort", identity=identity, **settings) as returned:
                    require(returned.sid != peer.sid, "Interrupted guest reused its SID")
                    see_identity(observer, b"StageIIIAbort", identity[1], returned.sid)
        with suite.case("stability.observation") as result:
            with guest(context, b"StageIIIMulti1", **settings) as a:
                with guest(context, b"StageIIIMulti2", **settings) as b:
                    with guest(context, b"StageIIIMulti3", **settings) as c:
                        require(len({a.sid, b.sid, c.sid}) == 3, "Concurrent guests shared a SID")
                        for _ in range(4):
                            for sender, receiver in ((a, b), (b, c), (c, a)):
                                frame = b"BMSG " + sender.sid + b" stage3-live-" + suffix()
                                sender.send(frame)
                                receiver.until(lambda line: line == frame, "concurrent guest chat")
                            time.sleep(1)
            result.observed = "Three simultaneous guests exchanged attributed chat over four seconds"
        with suite.case("stability.ncdc"):
            check_real_ncdc(None, context, settings, url_host=LOOPBACK)

    write_config(config, pair, idle=2)
    with suite.case("stability.idle") as result:
        with hub(binary, config) as (settings, _):
            identity = base.pair(None)
            with guest(context, b"StageIIIIdle", identity=identity, **settings) as peer:
                start = time.monotonic()
                peer.until(lambda line: line.startswith(b"ISTA 230 "), "server idle status", timeout=5)
                peer.until(lambda line: line.split()[:2] == [b"IQUI", peer.sid], "server idle quit", timeout=3)
                peer.peer.settimeout(3)
                require(peer.peer.recv(32) == b"", "Unexpected data after idle disconnect")
                require(1 <= time.monotonic() - start <= 5, "Idle deadline was outside the expected fixture bound")
            with guest(context, b"StageIIIIdle", identity=identity, **settings) as returned:
                require(returned.sid != peer.sid, "Idle-disconnected guest reused its SID")
        result.observed = "Server emitted idle status and quit, closed TLS, and admitted the same CID under a new SID"
