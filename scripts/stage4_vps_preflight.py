#!/usr/bin/env python3
"""Read-only Stage IV VPS console report. Never sends mail or changes services."""

import json
import os
from pathlib import Path
import re
import shutil
import socket
import subprocess
import sys


HOST = "server-747585786.dc24h.eu"
IPV4 = "62.171.156.32"
CERT = Path("/etc/letsencrypt/live") / HOST / "fullchain.pem"


def command(*argv: str) -> str | None:
    try:
        result = subprocess.run(argv, capture_output=True, text=True,
                                timeout=5, check=False)
        if result.returncode != 0:
            return None
        return result.stdout.strip()
    except (OSError, subprocess.TimeoutExpired):
        return None


def listening_port(lines: str, port: int) -> list[str]:
    """Only inspect local listener addresses, never disclose process metadata."""
    hosts = []
    for line in lines.splitlines():
        columns = line.split()
        if len(columns) < 5 or columns[0] != "LISTEN":
            continue
        local = columns[3]
        if local.rsplit(":", 1)[-1] != str(port):
            continue
        hosts.append(local.rsplit(":", 1)[0].strip("[]"))
    return hosts


def evaluate_listeners(lines: str) -> dict[str, str]:
    hub = listening_port(lines, 1512)
    forbidden_adc = listening_port(lines, 1511)
    node = listening_port(lines, 4278)
    admin = listening_port(lines, 7278)
    return {
        "hub_adcs_1512": "PASS" if hub else "FAIL",
        "no_adc_1511": "PASS" if not forbidden_adc else "FAIL",
        "node_4278_loopback_only": "PASS" if all(ip in ("127.0.0.1", "::1") for ip in node) else "FAIL",
        "apache_7278": "PRESENT" if admin else "DISABLED",
    }


def main() -> int:
    if sys.argv[1:]:
        print("Usage: python3 scripts/stage4_vps_preflight.py", file=sys.stderr)
        return 2
    checks: dict[str, str] = {}
    try:
        addresses = {item[4][0] for item in socket.getaddrinfo(HOST, None,
                     family=socket.AF_INET, type=socket.SOCK_STREAM)}
        checks["dns_approved_ipv4"] = "PASS" if IPV4 in addresses else "FAIL"
    except OSError:
        checks["dns_approved_ipv4"] = "UNVERIFIED"

    if shutil.which("ss"):
        sockets = command("ss", "-lnt")
        checks.update(evaluate_listeners(sockets) if sockets is not None else {
            "listening_ports": "UNVERIFIED"})
    else:
        checks["listening_ports"] = "UNVERIFIED"

    node = command("node", "--version") if shutil.which("node") else None
    version = re.fullmatch(r"v(\d+)\.(\d+)\.(\d+)", node or "")
    checks["node_usable_runtime"] = "PASS" if version and (
        int(version.group(1)) > 22 or
        int(version.group(1)) == 22 and int(version.group(2)) >= 12) else "FAIL"
    checks["msmtp_installed"] = "PASS" if Path("/usr/bin/msmtp").is_file() else "FAIL"

    # The root shell's Node or msmtp does not qualify the dedicated service.
    webadmin = command("getent", "passwd", "dc24h-webadmin")
    checks["webadmin_dedicated_uid"] = (
        "PASS" if webadmin and webadmin.split(":")[0] == "dc24h-webadmin"
        and webadmin.split(":")[2] not in ("0", "") else "NOT_CONFIGURED"
    )
    service_node = Path("/opt/dc24h/node24/bin/node")
    if service_node.is_file():
        service_version = command(str(service_node), "--version")
        checks["webadmin_node24"] = "PASS" if re.fullmatch(r"v24\.\d+\.\d+", service_version or "") else "FAIL"
    else:
        checks["webadmin_node24"] = "NOT_CONFIGURED"
    checks["webadmin_bridge"] = (
        "PASS" if command("systemctl", "is-active", "dc24h-twilight-stage4-bridge.service") == "active"
        else "DISABLED"
    )

    if shutil.which("systemctl"):
        state = command("systemctl", "is-active", "dc24h-stage3-adcs.service")
        if state is None:
            state = command("systemctl", "is-active", "dc24h-twilight.service")
        checks["hub_service"] = "PASS" if state == "active" else "UNVERIFIED"
    if shutil.which("apache2ctl"):
        outcome = command("apache2ctl", "configtest")
        # Apache can print Syntax OK to stderr; do not treat that as evidence
        # of public :7278 reachability or activation.
        checks["apache_syntax"] = "PASS" if outcome is not None else "FAIL"
    else:
        checks["apache_syntax"] = "UNVERIFIED"

    if CERT.is_file() and shutil.which("openssl"):
        meta = command("openssl", "x509", "-in", str(CERT), "-noout", "-checkend", "604800")
        san = command("openssl", "x509", "-in", str(CERT), "-noout", "-ext", "subjectAltName")
        checks["letsencrypt_7_days"] = "PASS" if meta is not None else "FAIL"
        checks["letsencrypt_exact_hostname"] = "PASS" if san and f"DNS:{HOST}" in san else "FAIL"
    else:
        checks["letsencrypt_7_days"] = "UNVERIFIED"
        checks["letsencrypt_exact_hostname"] = "UNVERIFIED"

    for pending in ("real_smtp_delivery", "verified_mailbox_code", "real_backend_role_revocation",
                    "external_https_ingress", "systemd_restart_rollback", "independent_cookie_review"):
        checks[pending] = "NOT_TESTED"
    output = {"schema": 1, "stage": "IV", "scope": "read_only_vps_console", "checks": checks}
    print(json.dumps(output, indent=2, sort_keys=True))
    return 1 if "FAIL" in checks.values() else 0


if __name__ == "__main__":
    sys.exit(main())
