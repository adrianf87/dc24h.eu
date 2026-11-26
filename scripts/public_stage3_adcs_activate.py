#!/usr/bin/env python3
"""OWNER-gated Stage III guest ADCS activation and exact-target rollback.

This script is intentionally never invoked by pull-request CI. It installs an
isolated service without modifying a legacy installation, firewall or SSH.
The separate GitHub-hosted external job decides whether it stays installed.
"""

import argparse
import grp
import json
import os
from pathlib import Path
import pwd
import re
import shutil
import stat
import subprocess
import sys

import public_stage3_adcs_preflight as readiness


UNIT = "dc24h-stage3-adcs.service"
UNIT_PATH = Path("/etc/systemd/system") / UNIT
PREFIX = Path("/opt/dc24h-stage3")
MANIFEST = PREFIX / "manifest.json"
CONFIG_DIR = Path("/etc/dc24h-stage3")
CONFIG = CONFIG_DIR / "dc24h.ini"
TLS_DIR = CONFIG_DIR / "tls"
CERT = TLS_DIR / "fullchain.pem"
KEY = TLS_DIR / "privkey.pem"
MARKER = CONFIG_DIR / ".owner-activation"
SERVICE_USER = "dc24h-stage3"
ACTIVATE_TOKEN = "RUN_STAGE3_PUBLIC_ADCS_ACTIVATION"
ROLLBACK_TOKEN = "RUN_STAGE3_PUBLIC_ADCS_ROLLBACK"


def run(*argv):
    subprocess.run(argv, check=True, timeout=45, capture_output=True, text=True)


def write_new(path, content, mode, owner=0, group=0):
    flags = os.O_WRONLY | os.O_CREAT | os.O_EXCL | getattr(os, "O_NOFOLLOW", 0)
    fd = os.open(path, flags, mode)
    with os.fdopen(fd, "w", encoding="utf-8") as handle:
        handle.write(content)
    os.chown(path, owner, group)
    os.chmod(path, mode)


def copy_new(source, destination, mode, owner=0, group=0):
    flags = os.O_WRONLY | os.O_CREAT | os.O_EXCL | getattr(os, "O_NOFOLLOW", 0)
    fd = os.open(destination, flags, mode)
    with open(source, "rb") as source_handle, os.fdopen(fd, "wb") as destination_handle:
        shutil.copyfileobj(source_handle, destination_handle)
    os.chown(destination, owner, group)
    os.chmod(destination, mode)


def guest_config():
    """Explicit TLS-only and no-account policy; never bind a wildcard address."""
    return f"""[general]
language = us

[network]
listen = none
adcs_listen = {readiness.EXPECTED_IPV4}:{readiness.EXPECTED_PORT}
max_connections = 256
idle_timeout = 120

[users]
max_sessions = 256
max_users = 256

[security]
max_connections_per_ip = 16
max_accepts_per_minute = 96
max_pending_logins = 64
max_pending_per_ip = 16
tls_handshake_timeout = 10
login_timeout = 30

[tls]
certificate = {CERT}
private_key = {KEY}

[database]
enabled = off

[authentication]
enabled = off

[api]
enabled = off

[logging]
level = info
connections = off
chat = off
file = stderr
"""


def service_unit(binary):
    return f"""[Unit]
Description=dc24h.eu-twilight Stage III public guest ADCS
After=network-online.target
Wants=network-online.target
AssertPathExists={MARKER}
StartLimitIntervalSec=60
StartLimitBurst=5

[Service]
Type=simple
User={SERVICE_USER}
Group={SERVICE_USER}
UMask=0077
ExecStart={binary} --config {CONFIG}
Restart=on-failure
RestartSec=3
TimeoutStopSec=20
NoNewPrivileges=yes
WorkingDirectory=/var/lib/dc24h-stage3
PrivateTmp=yes
PrivateDevices=yes
ProtectSystem=strict
ProtectHome=yes
ProtectKernelTunables=yes
ProtectKernelModules=yes
ProtectKernelLogs=yes
ProtectControlGroups=yes
ProtectProc=invisible
ProcSubset=pid
RestrictAddressFamilies=AF_UNIX AF_INET AF_INET6
RestrictNamespaces=yes
RestrictRealtime=yes
RestrictSUIDSGID=yes
LockPersonality=yes
MemoryDenyWriteExecute=yes
CapabilityBoundingSet=
AmbientCapabilities=
SystemCallArchitectures=native
SystemCallFilter=@system-service
SystemCallErrorNumber=EPERM
StateDirectory=dc24h-stage3
StateDirectoryMode=0700
RuntimeDirectory=dc24h-stage3
RuntimeDirectoryMode=0700

[Install]
WantedBy=multi-user.target
"""


def require_identity(args):
    expected = ACTIVATE_TOKEN if args.action == "activate" else ROLLBACK_TOKEN
    if not re.fullmatch(r"[0-9a-f]{40}", args.expected_sha):
        raise RuntimeError("Expected commit must be a full lowercase SHA-1")
    if (args.confirmation != expected or args.actor != readiness.EXPECTED_OWNER
            or args.ref != "refs/heads/main" or args.runner != readiness.EXPECTED_RUNNER
            or args.runner_user != "github-runner" or os.geteuid() != 0):
        raise RuntimeError("OWNER, protected main, runner, root or exact action confirmation mismatch")
    if not readiness.debian_13() or not readiness.matching_source_sha(args.expected_sha):
        raise RuntimeError("Debian 13 or exact checked-out SHA verification failed")


def assert_new_install_targets():
    for path in (UNIT_PATH, PREFIX, CONFIG_DIR):
        if path.exists() or path.is_symlink():
            raise RuntimeError("Refusing to overwrite an existing Stage III target")


def get_service_user():
    try:
        user = pwd.getpwnam(SERVICE_USER)
        if user.pw_uid >= 1000 or user.pw_shell != "/usr/sbin/nologin":
            raise RuntimeError("Existing Stage III service account has an unsafe profile")
        try:
            service_group = grp.getgrnam(SERVICE_USER)
        except KeyError as exc:
            raise RuntimeError("Existing Stage III service account has no matching group") from exc
        if service_group.gr_gid != user.pw_gid:
            raise RuntimeError("Existing Stage III service account has an unexpected group")
        return user, False
    except KeyError:
        run("useradd", "--system", "--user-group", "--no-create-home", "--home-dir", "/var/lib/dc24h-stage3",
            "--shell", "/usr/sbin/nologin", SERVICE_USER)
        return pwd.getpwnam(SERVICE_USER), True


def activate(args):
    report = readiness.collect(args.expected_sha, args.actor, args.ref, args.runner, args.runner_user)
    if report["result"] != "LOCAL_PREREQUISITES_CLEAR":
        raise RuntimeError("Pre-deployment blockers: " + ", ".join(report["blockers"]))
    if not args.provider_1512_confirmed:
        raise RuntimeError("OWNER must attest provider TCP 1512 allow rule and SSH access")
    if not args.tls_renewal_plan_confirmed:
        raise RuntimeError("OWNER must identify the certificate renewal/restart operator")
    if not args.binary.is_file() or args.binary.is_symlink():
        raise RuntimeError("A previously built ordinary binary is required")
    assert_new_install_targets()
    pair = readiness.certificate_pair()
    if not readiness.certificate_valid(pair):
        raise RuntimeError("Trusted source TLS chain and private key are not valid")
    release = PREFIX / args.expected_sha[:12]
    installed = release / "bin" / "dc24h-twilight"
    created_user = False
    try:
        user, created_user = get_service_user()
        PREFIX.mkdir(mode=0o755)
        release.mkdir(mode=0o755)
        installed.parent.mkdir(mode=0o755)
        CONFIG_DIR.mkdir(mode=0o750)
        TLS_DIR.mkdir(mode=0o750)
        os.chown(CONFIG_DIR, 0, user.pw_gid)
        os.chown(TLS_DIR, 0, user.pw_gid)
        copy_new(args.binary, installed, 0o755)
        copy_new(pair[0], CERT, 0o640, 0, user.pw_gid)
        copy_new(pair[1], KEY, 0o640, 0, user.pw_gid)
        write_new(CONFIG, guest_config(), 0o640, 0, user.pw_gid)
        run("runuser", "-u", SERVICE_USER, "--", str(installed), "--config", str(CONFIG), "--check-config")
        write_new(MARKER, "OWNER Stage III public guest ADCS activation\n", 0o640, 0, user.pw_gid)
        write_new(UNIT_PATH, service_unit(installed), 0o644)
        write_new(MANIFEST, json.dumps({
            "schema": 1, "source_sha": args.expected_sha, "unit": UNIT,
            "fqdn": readiness.EXPECTED_FQDN, "ipv4": readiness.EXPECTED_IPV4,
            "port": readiness.EXPECTED_PORT, "created_user": created_user,
        }, sort_keys=True) + "\n", 0o600)
        run("systemctl", "daemon-reload")
        run("systemctl", "enable", "--now", UNIT)
        run("systemctl", "is-active", "--quiet", UNIT)
    except Exception:
        # No legacy service or global firewall is touched; remove only our
        # exact, originally absent paths on failure. Never delete /etc/dc24h.
        cleanup(release, created_user)
        raise
    print("STAGE3_DEPLOYED_PENDING_EXTERNAL_ACCEPTANCE")


def cleanup(release, created_user):
    try:
        run("systemctl", "disable", "--now", UNIT)
    except (OSError, subprocess.CalledProcessError, subprocess.TimeoutExpired):
        pass
    unit_active = readiness.command("systemctl", "is-active", "--quiet", UNIT)
    if unit_active is None or unit_active.returncode == 0:
        raise RuntimeError("Stage III rollback blocked: systemd unit may still be active")
    if not readiness.port_available(readiness.EXPECTED_PORT):
        raise RuntimeError("Stage III rollback blocked: TCP 1512 is still listening")
    for file in (UNIT_PATH, MANIFEST, MARKER, CONFIG, CERT, KEY, release / "bin" / "dc24h-twilight"):
        try:
            file.unlink(missing_ok=True)
        except OSError:
            pass
    try:
        run("systemctl", "daemon-reload")
    except (OSError, subprocess.CalledProcessError, subprocess.TimeoutExpired):
        pass
    for directory in (TLS_DIR, CONFIG_DIR, release / "bin", release, PREFIX):
        try:
            directory.rmdir()  # Never recursively remove unknown host files.
        except OSError:
            pass
    state_dir = Path("/var/lib/dc24h-stage3")
    try:
        state_dir.rmdir()  # Preserve any unexpected persistent state.
    except OSError:
        pass
    if created_user and not state_dir.exists():
        try:
            run("userdel", SERVICE_USER)
        except (OSError, subprocess.CalledProcessError, subprocess.TimeoutExpired):
            pass


def rollback(args):
    if not MANIFEST.is_file() or MANIFEST.is_symlink():
        if not any(path.exists() for path in (UNIT_PATH, PREFIX, CONFIG_DIR)):
            print("STAGE3_ALREADY_NOT_DEPLOYED")
            return
        raise RuntimeError("Partial Stage III files exist without a trusted owned manifest")
    data = json.loads(MANIFEST.read_text(encoding="utf-8"))
    deployed_sha = args.deployed_sha or args.expected_sha
    if not re.fullmatch(r"[0-9a-f]{40}", deployed_sha):
        raise RuntimeError("Rollback needs a full deployed commit SHA")
    if (data.get("schema") != 1 or data.get("source_sha") != deployed_sha
            or data.get("unit") != UNIT or data.get("fqdn") != readiness.EXPECTED_FQDN
            or data.get("ipv4") != readiness.EXPECTED_IPV4 or data.get("port") != readiness.EXPECTED_PORT):
        raise RuntimeError("Refusing rollback: exact deployment identity differs")
    cleanup(PREFIX / deployed_sha[:12], bool(data["created_user"]))
    if any(path.exists() for path in (MANIFEST, UNIT_PATH, CONFIG, CERT, KEY)):
        raise RuntimeError("Rollback incomplete; operator review required")
    print("STAGE3_ROLLBACK_COMPLETED")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--action", required=True, choices=("activate", "rollback"))
    parser.add_argument("--confirmation", required=True)
    parser.add_argument("--expected-sha", required=True)
    parser.add_argument("--actor", required=True)
    parser.add_argument("--ref", required=True)
    parser.add_argument("--runner", required=True)
    parser.add_argument("--runner-user", required=True)
    parser.add_argument("--binary", type=Path)
    parser.add_argument("--deployed-sha")
    parser.add_argument("--provider-1512-confirmed", action="store_true")
    parser.add_argument("--tls-renewal-plan-confirmed", action="store_true")
    args = parser.parse_args(argv)
    try:
        require_identity(args)
        if args.action == "activate":
            if args.binary is None:
                raise RuntimeError("Activation needs an exact-head built binary")
            activate(args)
        else:
            rollback(args)
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError,
            subprocess.TimeoutExpired) as exc:
        print("Stage III host action refused or failed: " + type(exc).__name__, file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
