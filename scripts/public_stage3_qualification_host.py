#!/usr/bin/env python3
"""OWNER-only active-service restart and same-pair manual TLS copy rehearsal.

Strictly scoped to the known Stage III unit and copied TLS files. Never runs
certbot, edits Let's Encrypt archives, changes a firewall, or stops SSH.
Actual renewal/rotation to a *different* pair remains an OWNER procedure.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import pwd
import re
import shutil
import socket
import ssl
import subprocess
import tempfile
import time

import public_stage3_adcs_activate as activate
import public_stage3_adcs_preflight as preflight
import public_stage3_external_probe as external


UNIT = "dc24h-stage3-adcs.service"
FQDN = external.FQDN
SRC = Path("/etc/letsencrypt/live") / FQDN
CERT_SOURCE = SRC / "fullchain.pem"
KEY_SOURCE = SRC / "privkey.pem"
CERT = activate.CERT
KEY = activate.KEY
TRIES = 3
CONFIRM = "RUN_STAGE3_PUBLIC_QUALIFICATION"
# SSH and existing Apache/ACME endpoints are left untouched. All other
# externally bound TCP services require a new documented OWNER decision.
APPROVED_TCP = frozenset({22, 80, 443, 1512})


def run(*args, timeout=30):
    return subprocess.run(args, check=True, timeout=timeout, capture_output=True, text=True)


def public_listener_ports(ss_output):
    ports = set()
    for line in ss_output.splitlines():
        cols = line.split()
        if len(cols) < 5:
            raise RuntimeError("Unexpected ss listener format")
        endpoint = cols[3]
        address, separator, port = endpoint.rpartition(":")
        if not separator or not port.isdecimal():
            raise RuntimeError("Cannot classify listening TCP socket")
        # Loopback listeners are not directly exposed; the separate external
        # probe still checks denied public routes on the selected IPv4.
        if address in ("127.0.0.1", "[::1]", "::1", "localhost"):
            continue
        if address in ("0.0.0.0", "*", "[::]", "::", external.IPV4):
            ports.add(int(port))
        else:
            # Unexpected non-loopback interface may be port-forwarded by a
            # provider; conservatively include its TCP port.
            ports.add(int(port))
    return ports


def check_host_exposure():
    output = run("ss", "-H", "-ltn").stdout
    exposed = public_listener_ports(output)
    if 1512 not in exposed:
        raise RuntimeError("Expected ADCS 1512 not listening on the service host")
    forbidden = exposed - APPROVED_TCP
    if forbidden:
        raise RuntimeError("Unexpected non-loopback listener TCP port(s): " +
                           ",".join(map(str, sorted(forbidden))))
    print("STAGE3_HOST_LISTENER_ALLOWLIST_PASS", flush=True)


def guard(args):
    if (os.geteuid() != 0 or args.confirmation != CONFIRM or
            args.actor != "adrianf87" or args.ref != "refs/heads/main" or
            args.runner != "server-747585786" or args.runner_user != "github-runner" or
            not re.fullmatch(r"[0-9a-f]{40}", args.expected_sha) or
            not preflight.debian_13() or not preflight.matching_source_sha(args.expected_sha)):
        raise RuntimeError("OWNER/main/exact checked-out SHA/root/runner guard failed")
    if not activate.MANIFEST.is_file() or activate.MANIFEST.is_symlink():
        raise RuntimeError("Trusted Stage III manifest missing")
    data = json.loads(activate.MANIFEST.read_text())
    if (data.get("schema") != 1 or not re.fullmatch(r"[0-9a-f]{40}", data.get("source_sha", ""))
            or data.get("unit") != UNIT or data.get("fqdn") != FQDN or
            data.get("ipv4") != external.IPV4 or data.get("port") != 1512):
        raise RuntimeError("Stage III deployment manifest identity mismatch")
    for path in (CERT, KEY, activate.UNIT_PATH, activate.CONFIG, activate.MARKER):
        if not path.is_file() or path.is_symlink():
            raise RuntimeError("Stage III owned file missing or symlinked: " + str(path))
    if pwd.getpwnam(activate.SERVICE_USER).pw_uid == 0:
        raise RuntimeError("Refusing root-owned service account")
    if "active" != run("systemctl", "is-active", UNIT).stdout.strip():
        raise RuntimeError("Stage III service must already be active")
    unit_contents = activate.UNIT_PATH.read_text()
    if "User=dc24h-stage3\n" not in unit_contents or "ExecStart=/opt/dc24h-stage3/" not in unit_contents:
        raise RuntimeError("Unknown service unit; refusing to restart")


def probe_until_ready(timeout=20):
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        try:
            context = ssl.create_default_context()
            with socket.create_connection((external.IPV4, 1512), timeout=2) as plain:
                with context.wrap_socket(plain, server_hostname=FQDN) as peer:
                    peer.settimeout(2)
                    peer.sendall(b"HSUP ADBASE ADTIGR\n")
                    line, _ = external.recv_line(peer, b"")
                    if line == b"ISUP ADBASE ADTIGR":
                        return
        except (OSError, ssl.SSLError, RuntimeError):
            time.sleep(0.4)
    raise RuntimeError("Stage III CA/SAN TLS/ADC handshake did not recover")


def restart_and_probe(label):
    start = time.monotonic()
    run("systemctl", "restart", UNIT, timeout=30)
    probe_until_ready()
    run("systemctl", "is-active", "--quiet", UNIT)
    elapsed = round(time.monotonic() - start, 2)
    print(label + "_PASS duration_seconds=" + str(elapsed), flush=True)
    return elapsed


def source_pair_is_same():
    # Let's Encrypt live files are expected symlinks into the archive.
    for path in (CERT_SOURCE, KEY_SOURCE):
        resolved = path.resolve(strict=True)
        if not resolved.is_file() or not resolved.is_relative_to(Path("/etc/letsencrypt/archive").resolve()):
            raise RuntimeError("Unexpected Let's Encrypt live path")
    for a, b in ((CERT_SOURCE, CERT), (KEY_SOURCE, KEY)):
        if not hashlib.sha256(a.read_bytes()).digest() == hashlib.sha256(b.read_bytes()).digest():
            raise RuntimeError("Source TLS pair differs from deployed pair; manual rehearsal cannot rotate credentials")
    # Verify source certificate and key match before ANY file mutation.
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(str(CERT_SOURCE), str(KEY_SOURCE))


def guarded_copy_pair():
    source_pair_is_same()
    tls_dir = activate.TLS_DIR
    group = KEY.stat().st_gid
    backup = Path(tempfile.mkdtemp(prefix=".stage3-manual-tls-", dir=tls_dir))
    os.chmod(backup, 0o700)
    installed = False
    try:
        # Fully staged pair plus preserved originals; never copy into certbot's archive.
        for name, source, live in (("cert", CERT_SOURCE, CERT), ("key", KEY_SOURCE, KEY)):
            for target, origin in ((backup / (name + ".old"), live),
                                   (backup / (name + ".new"), source)):
                with target.open("xb") as output, origin.open("rb") as input_file:
                    shutil.copyfileobj(input_file, output)
                os.chmod(target, 0o640)
                os.chown(target, 0, group)
        os.replace(backup / "cert.new", CERT)
        os.replace(backup / "key.new", KEY)
        installed = True
        restart_and_probe("STAGE3_SAME_PAIR_TLS_COPY_RESTART")
    except Exception:
        # Both backups are present before the first replace. Restore the old
        # pair and restart only our named unit; propagate failures to OWNER.
        if (backup / "cert.old").exists() and (backup / "key.old").exists():
            os.replace(backup / "cert.old", CERT)
            os.replace(backup / "key.old", KEY)
            try:
                restart_and_probe("STAGE3_TLS_RESTORED_AFTER_FAILED_REHEARSAL")
            except Exception as rollback_error:
                raise RuntimeError("URGENT: TLS rehearsal AND scoped restore failed") from rollback_error
        raise
    finally:
        # Avoid deleting unknown files; all entries were created by this run.
        for name in ("cert.old", "key.old", "cert.new", "key.new"):
            (backup / name).unlink(missing_ok=True)
        backup.rmdir()
    if not installed:
        raise RuntimeError("TLS rehearsal did not install the copied pair")


def main():
    parser = argparse.ArgumentParser()
    for name in ("confirmation", "actor", "ref", "runner", "runner-user", "expected-sha"):
        parser.add_argument("--" + name, required=True)
    args = parser.parse_args()
    guard(args)
    check_host_exposure()
    probe_until_ready()
    print("STAGE3_HOST_IDENTITY_AND_INITIAL_TLS_PASS", flush=True)
    for cycle in range(1, TRIES + 1):
        restart_and_probe("STAGE3_SYSTEMD_RESTART_" + str(cycle))
    guarded_copy_pair()
    print("STAGE3_HOST_RESTARTS_AND_MANUAL_SAME_PAIR_REHEARSAL_PASS", flush=True)


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError, ssl.SSLError) as exc:
        print("STAGE3_HOST_FAIL: " + type(exc).__name__ + ": " + str(exc)[:160], flush=True)
        raise SystemExit(2)
