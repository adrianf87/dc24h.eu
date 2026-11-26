#!/usr/bin/env python3
"""Read-only, fail-closed Debian 13 public ADCS readiness inventory.

No service, firewall, DNS, key, account or listener is created or modified.
The bounded JSON output deliberately omits raw host inventory and private data.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import stat
import subprocess
import sys


EXPECTED_RUNNER = "server-747585786"
EXPECTED_OWNER = "adrianf87"
EXPECTED_FQDN = "server-747585786.dc24h.eu"
EXPECTED_IPV4 = "62.171.156.32"
EXPECTED_PORT = 1512


def command(*args, timeout=12):
    try:
        return subprocess.run(
            args, capture_output=True, text=True, check=False, timeout=timeout
        )
    except (OSError, subprocess.TimeoutExpired):
        return None


def matching_source_sha(expected):
    # Git runs as the unprivileged checkout owner: root must not globally trust
    # arbitrary worktrees via safe.directory=*. The workflow checks the SHA too.
    proc = command("runuser", "-u", "github-runner", "--", "git", "rev-parse", "HEAD")
    return proc is not None and proc.returncode == 0 and proc.stdout.strip() == expected


def debian_13():
    try:
        pairs = dict(
            line.split("=", 1)
            for line in Path("/etc/os-release").read_text(encoding="utf-8").splitlines()
            if "=" in line
        )
    except OSError:
        return False
    return pairs.get("ID", "").strip('"') == "debian" and pairs.get(
        "VERSION_ID", ""
    ).strip('"').split(".")[0] == "13"


def dns_contains_target(result, target):
    if result is None or result.returncode != 0:
        return False
    return target in {line.split()[0] for line in result.stdout.splitlines() if line.split()}


def direct_ipv4_present(result, target):
    if result is None or result.returncode != 0:
        return False
    return re.search(r"\binet\s+" + re.escape(target) + r"/\d+\b", result.stdout) is not None


def listening_tcp_port(proc_content, port):
    """Recognize LISTEN sockets in /proc/net/tcp{,6} without disclosing peers."""
    for line in proc_content.splitlines()[1:]:
        fields = line.split()
        if len(fields) < 4 or fields[3] != "0A":
            continue
        try:
            local_port = int(fields[1].rsplit(":", 1)[1], 16)
        except (ValueError, IndexError):
            continue
        if local_port == port:
            return True
    return False


def port_available(port):
    try:
        return not any(
            listening_tcp_port(Path(path).read_text(encoding="ascii"), port)
            for path in ("/proc/net/tcp", "/proc/net/tcp6")
        )
    except OSError:
        return False


def inactive_service(unit):
    proc = command("systemctl", "show", "-p", "ActiveState", "--value", unit)
    return proc is not None and proc.returncode == 0 and proc.stdout.strip() == "inactive"


def certificate_pair():
    candidates = (
        (Path("/etc/letsencrypt/live") / EXPECTED_FQDN / "fullchain.pem",
         Path("/etc/letsencrypt/live") / EXPECTED_FQDN / "privkey.pem"),
        (Path("/etc/dc24h/tls/fullchain.pem"), Path("/etc/dc24h/tls/privkey.pem")),
    )
    for cert, key in candidates:
        try:
            if cert.is_file() and key.is_file():
                return cert, key
        except OSError:
            pass
    return None


def certificate_valid(pair):
    if pair is None:
        return False
    cert, key = pair
    try:
        key_mode = stat.S_IMODE(key.stat().st_mode)
        if key_mode & 0o077:
            return False
    except OSError:
        return False

    # Verify a trusted public chain, DNS hostname, server EKU, and validity.
    verify = command(
        "openssl", "verify", "-purpose", "sslserver",
        "-verify_hostname", EXPECTED_FQDN,
        "-CAfile", "/etc/ssl/certs/ca-certificates.crt",
        "-untrusted", str(cert), str(cert),
    )
    expiry = command("openssl", "x509", "-in", str(cert), "-noout", "-checkend", "604800")
    cert_pub = command("openssl", "x509", "-in", str(cert), "-pubkey", "-noout")
    key_pub = command("openssl", "pkey", "-in", str(key), "-pubout")
    return (
        all(p is not None and p.returncode == 0 for p in (verify, expiry, cert_pub, key_pub))
        and hashlib.sha256(cert_pub.stdout.encode()).digest()
        == hashlib.sha256(key_pub.stdout.encode()).digest()
    )


def collect(expected_sha, actor, ref, runner, user):
    checks = {
        "owner_and_ref": actor == EXPECTED_OWNER and ref == "refs/heads/main",
        "runner_and_user": runner == EXPECTED_RUNNER and user == "github-runner" and os.geteuid() == 0,
        "exact_source_sha": matching_source_sha(expected_sha),
        "debian_13": debian_13(),
        "dns_a_matches_target": dns_contains_target(
            command("getent", "ahostsv4", EXPECTED_FQDN), EXPECTED_IPV4
        ),
        "target_ipv4_on_interface": direct_ipv4_present(
            command("ip", "-4", "-o", "address", "show"), EXPECTED_IPV4
        ),
        "legacy_service_inactive": inactive_service("dc24h.service"),
        "twilight_service_inactive": inactive_service("dc24h-twilight.service"),
        "tcp_1512_available": port_available(EXPECTED_PORT),
        "public_tls_chain_san_key_valid": certificate_valid(certificate_pair()),
    }
    blockers = sorted(name for name, ok in checks.items() if not ok)
    return {
        "scope": "READ_ONLY_PRE_DEPLOYMENT",
        "source_sha": expected_sha,
        "target_fqdn": EXPECTED_FQDN,
        "target_ipv4": EXPECTED_IPV4,
        "adcs_port": EXPECTED_PORT,
        "result": "LOCAL_PREREQUISITES_CLEAR" if not blockers else "BLOCKED",
        "checks": checks,
        "blockers": blockers,
        "provider_firewall_and_external_reachability_verified": False,
        "public_listener_activated": False,
        "production_go": False,
        "webadmin_public_go": False,
        "private_state_migration_go": False,
    }


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--expected-sha", required=True)
    parser.add_argument("--actor", required=True)
    parser.add_argument("--ref", required=True)
    parser.add_argument("--runner", required=True)
    parser.add_argument("--runner-user", required=True)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args(argv)
    report = collect(args.expected_sha, args.actor, args.ref, args.runner, args.runner_user)
    with args.report.open("x", encoding="utf-8") as handle:
        json.dump(report, handle, indent=2, sort_keys=True)
        handle.write("\n")
    args.report.chmod(0o644)
    print("Stage III preflight: " + report["result"])
    if report["blockers"]:
        print("Read-only blockers: " + ", ".join(report["blockers"]))
        return 2
    print("Local checks passed; external/provider and OWNER activation gates remain separate")
    return 0


if __name__ == "__main__":
    sys.exit(main())
