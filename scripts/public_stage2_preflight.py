#!/usr/bin/env python3
"""Read-only Debian 13 installation inventory; never authorizes an activation.

Prints a deliberately bounded JSON summary. No secret contents, process command
lines, raw addresses, database queries or configuration values are collected.
"""

import argparse
import json
import os
from pathlib import Path
import re
import shutil
import stat
import subprocess
import sys


SERVICES = ("dc24h.service", "dc24h-twilight.service")
HUB_PROCESSES = ("dc24h", "dc24h-twilight")
STABLE_PATHS = (
    "/etc/dc24h", "/var/lib/dc24h", "/run/dc24h", "/var/log/dc24h",
    "/opt/dc24h/webadmin", "/usr/local/share/dc24h/webadmin",
    "/usr/local/bin/dc24h", "/usr/local/bin/dc24h-twilight",
)
MARKER = "/etc/dc24h/dc24h-unit-migration-approved"
REQUIRED_TOOLS = ("git", "cmake", "c++", "systemctl", "systemd-run", "dpkg", "dpkg-deb")
OPTIONAL_TOOLS = ("node", "npm", "ncdc", "openssl", "ip", "ss")


def command(args):
    """Read-only subprocess, with a hard timeout and no shell invocation."""
    try:
        proc = subprocess.run(args, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                              text=True, check=False, timeout=5)
    except (OSError, subprocess.TimeoutExpired):
        return None
    return proc.stdout.strip() if proc.returncode == 0 else None


def path_metadata(name):
    try:
        item = os.lstat(name)  # Deliberately never follow links to private data.
    except FileNotFoundError:
        return {"type": "missing"}
    except (OSError, ValueError):
        # EACCES must not be confused with a missing file (fail closed).
        return {"type": "unavailable"}
    kind = ("symlink" if stat.S_ISLNK(item.st_mode) else
            "directory" if stat.S_ISDIR(item.st_mode) else
            "file" if stat.S_ISREG(item.st_mode) else "other")
    return {"type": kind, "mode": format(stat.S_IMODE(item.st_mode), "04o"),
            "uid": item.st_uid, "gid": item.st_gid}


def systemd_unit(name):
    output = command(("systemctl", "show", "--no-pager",
                      "--property=LoadState,ActiveState,UnitFileState", name))
    if output is None:
        return {"load": "unknown", "active": "unknown", "enabled": "unknown"}
    rows = dict(line.split("=", 1) for line in output.splitlines() if "=" in line)
    return {"load": rows.get("LoadState", "unknown"),
            "active": rows.get("ActiveState", "unknown"),
            "enabled": rows.get("UnitFileState", "unknown")}


def process_counts():
    counts = {name: 0 for name in HUB_PROCESSES}
    try:
        pids = os.listdir("/proc")
    except OSError:
        return {name: None for name in HUB_PROCESSES}
    for pid in pids:
        if not pid.isdecimal():
            continue
        try:
            name = (Path("/proc") / pid / "comm").read_text(encoding="utf-8").strip()
        except (OSError, UnicodeError):
            continue
        if name in counts:
            counts[name] += 1
    return counts


def network_summary():
    """Record only address-family counts; never write host IPs to a report."""
    raw = command(("ip", "-j", "address", "show"))
    if raw is None:
        return {"available": False}
    try:
        interfaces = json.loads(raw)
        if not isinstance(interfaces, list):
            return {"available": False}
        return {
            "available": True,
            "interfaces": len(interfaces),
            "nonloopback_ipv4": sum(
                1 for nic in interfaces if nic.get("ifname") != "lo"
                for address in nic.get("addr_info", []) if address.get("family") == "inet"),
            "nonloopback_ipv6": sum(
                1 for nic in interfaces if nic.get("ifname") != "lo"
                for address in nic.get("addr_info", []) if address.get("family") == "inet6"),
            "public_ipv4_identity_verified": False,  # Requires separate network/NAT evidence.
        }
    except (ValueError, TypeError, AttributeError):
        return {"available": False}


def template_contract(source):
    try:
        unit = (source / "config/dc24h.service.example").read_text(encoding="utf-8")
        webadmin = (source / "webadmin/src/lib/status.ts").read_text(encoding="utf-8")
        cmake = (source / "CMakeLists.txt").read_text(encoding="utf-8")
    except (OSError, UnicodeError):
        return False
    required = (
        "Conflicts=dc24h-twilight.service", "After=dc24h-twilight.service network-online.target", "AssertPathExists=" + MARKER,
        "User=dc24h", "Group=dc24h", "StateDirectory=dc24h",
        "ExecStart=/usr/local/bin/dc24h --config /etc/dc24h/dc24h.ini",
    )
    return (all(item in unit.splitlines() for item in required)
            and 'server.product_id === PRODUCT_ID && server.name === PRODUCT_NAME' in webadmin
            and "add_executable(dc24h src/main.cpp)" in cmake
            and "OUTPUT_NAME dc24h-twilight" not in cmake)


def assess(report):
    """Fail-closed classification, pure and independently unit-testable."""
    problems = []

    def block(code):
        problems.append(code)

    if not report["source"]["matches_expected"]:
        block("SOURCE_SHA_MISMATCH")
    if report["host"]["os_id"] != "debian" or report["host"]["os_major"] != "13":
        block("DEBIAN_13_REQUIRED")
    if not report["host"]["systemd_pid1"] or not report["host"]["systemd_manager"]:
        block("SYSTEMD_MANAGER_UNAVAILABLE")
    if not report["host"].get("elevated_inventory", False):
        block("INVENTORY_REQUIRES_ROOT")
    if not report["guards"]["template_contract_valid"]:
        block("UNIT_OR_STATUS_CONTRACT_MISMATCH")
    if report["guards"]["operator_marker_exists"]:
        block("EXISTING_MARKER_REQUIRES_OWNER_REVIEW")
    if not report["guards"].get("operator_marker_status_known", False):
        block("MARKER_STATUS_UNKNOWN")
    for name, unit in report["services"].items():
        if name == "dc24h.service" and unit["load"] != "not-found":
            block("TARGET_UNIT_NAME_IN_USE:" + name)
        if unit["active"] == "active":
            block("LIVE_HUB_SERVICE_PRESENT:" + name)
        elif unit["active"] == "unknown":
            block("SERVICE_STATUS_UNKNOWN:" + name)
        elif unit["active"] != "inactive":
            block("SERVICE_NOT_INACTIVE:" + name)
    for name, number in report["processes"].items():
        if number is None or number > 0:
            block("HUB_PROCESS_PRESENT_OR_UNVERIFIED:" + name)
    for name, metadata in report["paths"].items():
        if metadata["type"] == "symlink":
            block("SYMLINK_REQUIRES_REVIEW:" + name)
        elif metadata["type"] == "unavailable":
            block("PATH_METADATA_UNAVAILABLE:" + name)
    if any(not report["tools"][tool] for tool in REQUIRED_TOOLS):
        block("REQUIRED_TOOL_MISSING")

    report["findings"] = problems
    report["preflight"] = "BLOCKED" if problems else "INVENTORY_CLEAR"
    # Even a clear inventory never grants permission to write/migrate/activate.
    report["production_go"] = False
    report["private_state_migration_go"] = False
    report["public_listener_go"] = False
    return report


def inventory(source, expected):
    os_release = {}
    try:
        for line in Path("/etc/os-release").read_text(encoding="utf-8").splitlines():
            if line.startswith(("ID=", "VERSION_ID=")):
                key, value = line.split("=", 1)
                os_release[key] = value.strip('"')
    except OSError:
        pass
    sha = command(("git", "-C", str(source), "rev-parse", "HEAD"))
    pid1 = ""
    try:
        pid1 = Path("/proc/1/comm").read_text(encoding="utf-8").strip()
    except OSError:
        pass
    marker_meta = path_metadata(MARKER)
    report = {
        "schema": 1, "stage": "public-stage-ii-debian13", "scope": "READ_ONLY_INVENTORY",
        "source": {"expected_sha": expected, "actual_sha": sha,
                   "matches_expected": sha == expected},
        "host": {"os_id": os_release.get("ID", "unknown"),
                 "os_major": os_release.get("VERSION_ID", "unknown").split(".")[0],
                 "kernel": os.uname().release, "architecture": os.uname().machine,
                 "cpus": os.cpu_count(), "systemd_pid1": pid1 == "systemd",
                 "elevated_inventory": os.geteuid() == 0,
                 "systemd_manager": command(("systemctl", "show", "-p", "Version", "--value")) is not None},
        "network": network_summary(),
        "tools": {tool: shutil.which(tool) is not None
                  for tool in REQUIRED_TOOLS + OPTIONAL_TOOLS},
        "services": {name: systemd_unit(name) for name in SERVICES},
        "processes": process_counts(),
        "paths": {name: path_metadata(name) for name in STABLE_PATHS},
        "guards": {"operator_marker_exists": marker_meta["type"] != "missing",
                   "operator_marker_status_known": marker_meta["type"] != "unavailable",
                   "template_contract_valid": template_contract(source)},
    }
    return assess(report)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-dir", required=True, type=Path)
    parser.add_argument("--expected-sha", required=True)
    args = parser.parse_args(argv)
    if not re.fullmatch(r"[a-f0-9]{40}", args.expected_sha):
        parser.error("--expected-sha must be exactly 40 lowercase hex characters")
    result = inventory(args.source_dir.resolve(), args.expected_sha)
    print(json.dumps(result, indent=2, sort_keys=True))
    return 2 if result["preflight"] == "BLOCKED" else 0


if __name__ == "__main__":
    sys.exit(main())
