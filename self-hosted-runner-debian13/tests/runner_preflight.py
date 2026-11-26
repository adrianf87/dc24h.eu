#!/usr/bin/env python3
"""Read-only supplemental runner preflight; no sudo and no private-state access."""

import argparse
import json
import os
from pathlib import Path
import platform
import pwd
import re
import shutil
import subprocess
import sys


EXPECTED_RUNNER = "server-747585786"
EXPECTED_USER = "github-runner"
EXPECTED_ACTOR = "adrianf87"
EXPECTED_REF = "refs/heads/main"
HUB_UNITS = ("dc24h.service", "dc24h-twilight.service")
HUB_PROCESS_NAMES = ("dc24h", "dc24h-twilight")
TOOL_NAMES = ("python3", "cmake", "c++", "node", "npm", "ncdc", "systemctl")


def read_os_release():
    result = {}
    try:
        for line in Path("/etc/os-release").read_text(encoding="utf-8").splitlines():
            if line.startswith(("ID=", "VERSION_ID=")):
                key, value = line.split("=", 1)
                result[key] = value.strip('"')
    except OSError:
        pass
    return result


def active_state(unit):
    try:
        proc = subprocess.run(
            ("systemctl", "show", "--property=ActiveState", "--value", unit),
            stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
            text=True, timeout=5, check=False)
    except (OSError, subprocess.TimeoutExpired):
        return "unknown"
    state = proc.stdout.strip()
    return state if proc.returncode == 0 and state else "unknown"


def process_counts():
    counts = {name: 0 for name in HUB_PROCESS_NAMES}
    try:
        processes = os.listdir("/proc")
    except OSError:
        return None
    for pid in processes:
        if not pid.isdecimal():
            continue
        try:
            name = (Path("/proc") / pid / "comm").read_text(encoding="utf-8").strip()
        except FileNotFoundError:
            continue  # The process exited between the directory listing and read.
        except (OSError, UnicodeError):
            return None  # Incomplete visibility cannot be a passing inventory.
        if name in counts:
            counts[name] += 1
    return counts


def evaluate(report):
    blockers = []
    if report["host"]["os_id"] != "debian" or report["host"]["os_major"] != "13":
        blockers.append("DEBIAN_13_REQUIRED")
    if report["host"]["pid1"] != "systemd":
        blockers.append("SYSTEMD_PID1_REQUIRED")
    if report["runner"]["name"] != EXPECTED_RUNNER:
        blockers.append("WRONG_RUNNER_NAME")
    if report["runner"]["unix_user"] != EXPECTED_USER or report["runner"]["euid_is_root"]:
        blockers.append("UNEXPECTED_RUNNER_PRIVILEGES")
    if report["runner"]["actor"] != EXPECTED_ACTOR or report["runner"]["ref"] != EXPECTED_REF:
        blockers.append("UNAUTHORIZED_DISPATCH_CONTEXT")
    for unit, state in report["services"].items():
        if state != "inactive":
            blockers.append("SERVICE_NOT_INACTIVE_OR_UNKNOWN:" + unit)
    if report["processes"] is None:
        blockers.append("PROCESS_VISIBILITY_UNKNOWN")
    else:
        for name, count in report["processes"].items():
            if count:
                blockers.append("HUB_PROCESS_RUNNING:" + name)
    if not report["tools"]["python3"] or not report["tools"]["systemctl"]:
        blockers.append("BASELINE_TOOL_MISSING")
    report["findings"] = blockers
    report["result"] = "BLOCKED" if blockers else "INVENTORY_CLEAR"
    report["production_go"] = False
    report["migration_go"] = False
    report["public_listener_go"] = False
    return report


def inventory():
    release = read_os_release()
    try:
        pid1 = Path("/proc/1/comm").read_text(encoding="utf-8").strip()
    except OSError:
        pid1 = "unknown"
    report = {
        "schema": 1,
        "scope": "READ_ONLY_SUPPLEMENTAL_NO_SUDO",
        "host": {
            "os_id": release.get("ID", "unknown"),
            "os_major": release.get("VERSION_ID", "unknown").split(".")[0],
            "architecture": platform.machine(),
            "logical_cpus": os.cpu_count(),
            "pid1": pid1,
        },
        "runner": {
            "name": os.environ.get("RUNNER_NAME", "unknown"),
            "unix_user": pwd.getpwuid(os.geteuid()).pw_name,
            "euid_is_root": os.geteuid() == 0,
            "actor": os.environ.get("GITHUB_ACTOR", "unknown"),
            "ref": os.environ.get("GITHUB_REF", "unknown"),
        },
        "tools": {name: shutil.which(name) is not None for name in TOOL_NAMES},
        "services": {unit: active_state(unit) for unit in HUB_UNITS},
        "processes": process_counts(),
    }
    return evaluate(report)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--expected-sha", required=True)
    args = parser.parse_args(argv)
    if not re.fullmatch(r"[a-f0-9]{40}", args.expected_sha):
        parser.error("--expected-sha must be a lowercase 40-character commit SHA")
    # The exact source SHA must already have been checked by the workflow.
    report = inventory()
    report["source_sha"] = args.expected_sha
    print(json.dumps(report, indent=2, sort_keys=True))
    return 0 if report["result"] == "INVENTORY_CLEAR" else 2


if __name__ == "__main__":
    sys.exit(main())
