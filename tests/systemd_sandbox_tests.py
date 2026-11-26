#!/usr/bin/env python3
"""Check the example systemd unit and run the hub under those restrictions."""

import grp
import hashlib
import os
from pathlib import Path
import platform
import pwd
import re
import secrets
import shutil
import socket
import stat
import subprocess
import sys
import tempfile
import time
import unittest

REQUIRE_SYSTEMD_HOST = (os.environ.get("DC24H_REQUIRE_SYSTEMD_HOST") == "1" or
                        "--require-systemd-host" in sys.argv)
if "--require-systemd-host" in sys.argv:
    sys.argv.remove("--require-systemd-host")
BINARY, SERVICE, SYSUSERS = sys.argv[1:4]
del sys.argv[1:4]

SANDBOX_KEYS = (
    "UMask", "NoNewPrivileges", "PrivateTmp", "PrivateDevices", "ProtectSystem", "ProtectHome",
    "ProtectKernelTunables", "ProtectKernelModules", "ProtectKernelLogs", "ProtectControlGroups",
    "ProtectClock", "ProtectHostname", "ProtectProc", "ProcSubset", "RestrictAddressFamilies",
    "RestrictNamespaces", "RestrictRealtime", "RestrictSUIDSGID", "LockPersonality",
    "MemoryDenyWriteExecute", "SystemCallArchitectures", "CapabilityBoundingSet",
    "AmbientCapabilities", "SystemCallFilter", "SystemCallErrorNumber",
)
REQUIRED = {
    "Type": "simple",
    "User": "dc24h",
    "Group": "dc24h",
    "UMask": "0077",
    "WorkingDirectory": "/var/lib/dc24h",
    "ExecStart": "/usr/local/bin/dc24h-twilight --config /etc/dc24h/dc24h.ini",
    "Restart": "on-failure",
    "RestartSec": "3",
    "TimeoutStopSec": "20",
    "KillMode": "mixed",
    "KillSignal": "SIGTERM",
    "NoNewPrivileges": "yes",
    "PrivateTmp": "yes",
    "PrivateDevices": "yes",
    "ProtectSystem": "strict",
    "ProtectHome": "yes",
    "ProtectKernelTunables": "yes",
    "ProtectKernelModules": "yes",
    "ProtectKernelLogs": "yes",
    "ProtectControlGroups": "yes",
    "ProtectClock": "yes",
    "ProtectHostname": "yes",
    "ProtectProc": "invisible",
    "ProcSubset": "pid",
    "RestrictAddressFamilies": "AF_UNIX AF_INET AF_INET6",
    "RestrictNamespaces": "yes",
    "RestrictRealtime": "yes",
    "RestrictSUIDSGID": "yes",
    "LockPersonality": "yes",
    "MemoryDenyWriteExecute": "yes",
    "SystemCallArchitectures": "native",
    "CapabilityBoundingSet": "",
    "AmbientCapabilities": "",
    "SystemCallFilter": "@system-service",
    "SystemCallErrorNumber": "EPERM",
    "StateDirectory": "dc24h",
    "StateDirectoryMode": "0700",
    "RuntimeDirectory": "dc24h",
    "RuntimeDirectoryMode": "0700",
    "LogsDirectory": "dc24h",
    "LogsDirectoryMode": "0750",
    "ConfigurationDirectory": "dc24h",
    "ConfigurationDirectoryMode": "0755",
}
FORBIDDEN = (
    "User=root", "Group=root", "DynamicUser=", "Type=forking", "ExecStart=+", "sh -c", "bash ",
    "sudo ", "AF_PACKET", "AF_NETLINK", "AF_BLUETOOTH", "AmbientCapabilities=CAP",
    "CapabilityBoundingSet=CAP", "NoNewPrivileges=no", "PrivateTmp=no", "ProtectHome=no",
    "ProtectSystem=no", "ProtectSystem=false", "MemoryDenyWriteExecute=no",
)


def sections(text):
    found = {}
    section = None
    for raw in text.splitlines():
        line = raw.strip()
        if not line or line.startswith("#") or line.startswith(";"):
            continue
        if line.startswith("[") and line.endswith("]"):
            section = line[1:-1]
            found.setdefault(section, {})
            continue
        if section is None or "=" not in line:
            raise AssertionError("invalid unit line: " + line)
        key, value = line.split("=", 1)
        found[section][key.strip()] = value.strip()
    return found


def problems(text):
    errors = []
    if "\ufffd" in text or text.startswith("\ufeff"):
        errors.append("encoding")
    try:
        parsed = sections(text)
    except AssertionError as error:
        return [str(error)]
    service = parsed.get("Service", {})
    unit = parsed.get("Unit", {})
    for key, value in REQUIRED.items():
        if service.get(key) != value:
            errors.append(f"{key}={service.get(key)!r}")
    if unit.get("StartLimitIntervalSec") != "60" or unit.get("StartLimitBurst") != "5":
        errors.append("start limit")
    if (unit.get("Conflicts") != "dc24h.service" or
            unit.get("AssertPathExists") != "/etc/dc24h/twilight-unit-migration-approved"):
        errors.append("old/new systemd migration guard")
    if parsed.get("Install", {}).get("WantedBy") != "multi-user.target":
        errors.append("install target")
    for phrase in FORBIDDEN:
        if phrase in text:
            errors.append("forbidden " + phrase)
    families = set(service.get("RestrictAddressFamilies", "").split())
    if families != {"AF_UNIX", "AF_INET", "AF_INET6"}:
        errors.append("address families")
    return errors


def host_tool_prefix():
    if (os.environ.get("GITHUB_ACTIONS") == "true" and
            os.environ.get("RUNNER_ENVIRONMENT") == "github-hosted" and
            platform.freedesktop_os_release().get("ID") == "ubuntu" and shutil.which("sudo") and
            subprocess.run(["sudo", "-n", "id", "-u"], capture_output=True, timeout=10).stdout.strip() == b"0"):
        return ["sudo", "-n", "--"]
    if os.geteuid() == 0 and os.environ.get("GITHUB_ACTIONS") != "true":
        return []
    return None


def run(command, check=True, timeout=15):
    result = subprocess.run(command, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=timeout)
    if check and result.returncode != 0:
        raise AssertionError(f"{command} -> {result.returncode}\n{result.stdout}\n{result.stderr}")
    return result


class SystemdSandboxTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.host_tools = host_tool_prefix()
        if REQUIRE_SYSTEMD_HOST:
            if cls.host_tools is None or (os.geteuid() == 0 and os.environ.get("GITHUB_ACTIONS") == "true"):
                raise AssertionError("Required systemd-host test needs an unprivileged runner with sudo")
            manager = subprocess.run(
                ["systemctl", "show", "-p", "Version", "--value"],
                stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=15,
            )
            if manager.returncode != 0 or not manager.stdout.strip():
                raise AssertionError("Required systemd manager unavailable; container skip is not a pass")
        cls.text = Path(SERVICE).read_text(encoding="utf-8")
        cls.parsed = sections(cls.text)
        cls.account = pwd.getpwnam("nobody")
        cls.group = grp.getgrgid(cls.account.pw_gid).gr_name

    def test_shipped_directives(self):
        self.assertEqual(problems(self.text), [])
        weakened = self.text.replace("User=dc24h", "User=root", 1)
        self.assertTrue(problems(weakened))
        self.assertIn("old/new systemd migration guard",
                      problems(self.text.replace("Conflicts=dc24h.service", "", 1)))
        self.assertIn("old/new systemd migration guard",
                      problems(self.text.replace("AssertPathExists=/etc/dc24h/twilight-unit-migration-approved", "", 1)))
        self.assertFalse(any(line.startswith("dc24h:") for line in Path("/etc/passwd").read_text(encoding="utf-8").splitlines()))
        self.assertFalse(Path("/etc/systemd/system/dc24h.service").exists())
        self.assertFalse(Path("/etc/systemd/system/dc24h-twilight.service").exists())

    def test_verify_and_private_sysusers(self):
        with tempfile.TemporaryDirectory(prefix="dc24h-systemd-") as folder:
            root = Path(folder)
            unit = root / "dc24h-twilight.service"
            command = self.parsed["Service"]["ExecStart"].split(" ", 1)[1]
            unit.write_text(self.text.replace(
                "ExecStart=/usr/local/bin/dc24h-twilight ", f"ExecStart={BINARY} ", 1), encoding="utf-8")
            self.assertIn(command, unit.read_text(encoding="utf-8"))
            verified = run(["systemd-analyze", "verify", str(unit)], check=False)
            self.assertEqual(verified.returncode, 0, verified.stderr)
            self.assertNotIn("Failed", verified.stderr)
            private = root / "root"
            (private / "etc").mkdir(parents=True)
            shutil.copy("/etc/passwd", private / "etc" / "passwd")
            shutil.copy("/etc/group", private / "etc" / "group")
            before = hashlib.sha256(Path("/etc/passwd").read_bytes()).hexdigest()
            spec = Path(SYSUSERS).read_text(encoding="utf-8").strip()
            self.assertEqual(spec.splitlines()[-1], 'u dc24h - "dc24h hub" /var/lib/dc24h /usr/sbin/nologin')
            run(["systemd-sysusers", f"--root={private}", SYSUSERS])
            self.assertEqual(hashlib.sha256(Path("/etc/passwd").read_bytes()).hexdigest(), before)
            row = next(line.split(":") for line in (private / "etc" / "passwd").read_text(encoding="utf-8").splitlines()
                       if line.startswith("dc24h:"))
            self.assertNotEqual(row[2], "0")
            self.assertEqual(row[5], "/var/lib/dc24h")
            self.assertTrue(row[6].endswith("nologin"))
            self.assertFalse(any(line.startswith("dc24h:") for line in Path("/etc/passwd").read_text(encoding="utf-8").splitlines()))

    def host(self, command, **kwargs):
        return run([*self.host_tools, *command], **kwargs)

    def sandbox_properties(self, runtime):
        self.assertEqual(problems(self.text), [])
        service = self.parsed["Service"]
        properties = [f"-p{key}={service[key]}" for key in SANDBOX_KEYS]
        properties.extend([
            f"-pUser={self.account.pw_name}",
            f"-pGroup={self.group}",
            f"-pRuntimeDirectory={runtime}",
            f"-pRuntimeDirectoryMode={service['RuntimeDirectoryMode']}",
            "-pWorkingDirectory=/var/lib/" + runtime,
        ])
        return properties

    def test_hub_under_sandbox(self):
        if os.environ.get("DC24H_SANITIZER") in {"address", "thread"}:
            self.skipTest("MemoryDenyWriteExecute blocks this sanitizer runtime; the shipped unit is unchanged")
        if self.host_tools is None:
            self.skipTest("systemd sandbox requires an isolated Ubuntu runner with sudo")
        name = "dc24h-sandbox-" + secrets.token_hex(4)
        stage = Path("/var/lib") / name
        unit = name + ".service"
        token = secrets.token_hex(32)
        try:
            self.host(["install", "-d", "-m", "0755", str(stage)])
            self.host(["install", "-m", "0755", BINARY, str(stage / "dc24h")])
            token_path = stage / "admin.token"
            with socket.socket() as probe:
                probe.bind(("127.0.0.1", 0))
                port = probe.getsockname()[1]
            with tempfile.TemporaryDirectory(prefix="dc24h-sandbox-files-") as private:
                token_source = Path(private) / "admin.token"
                token_source.write_text(token + "\n", encoding="utf-8")
                token_source.chmod(0o600)
                self.host(["install", "-m", "0600", "-o", self.account.pw_name,
                           "-g", self.group, str(token_source), str(token_path)])
                config = Path(private) / "hub.ini"
                config.write_text(
                    "[network]\nlisten=127.0.0.1:%d\n[api]\nenabled=on\nsocket=/run/%s/admin.sock\ntoken_file=%s\n"
                    % (port, name, token_path), encoding="utf-8")
                self.host(["install", "-m", "0644", str(config), str(stage / "hub.ini")])
            self.host(["systemd-run", "--unit=" + name, "--collect", *self.sandbox_properties(name),
                       str(stage / "dc24h"), "--config", str(stage / "hub.ini")])
            deadline = time.monotonic() + 5
            log = ""
            while time.monotonic() < deadline:
                log = self.host(["journalctl", "-u", unit, "--no-pager", "-o", "cat"], check=False).stdout
                if "Network listening on" in log and "Local API listening" in log:
                    break
                time.sleep(0.05)
            else:
                self.fail(log)
            self.assertNotIn(token, log)
            match = re.search(r"Network listening on 127\.0\.0\.1:(\d+)", log)
            self.assertIsNotNone(match)
            with socket.create_connection(("127.0.0.1", int(match.group(1))), 2):
                pass
            socket_path = Path("/run") / name / "admin.sock"
            parent = socket_path.parent.stat()
            info = self.host(["stat", "--format=%F:%a:%u", str(socket_path)]).stdout.strip()
            self.assertEqual(stat.S_IMODE(parent.st_mode), 0o700)
            self.assertEqual(parent.st_uid, self.account.pw_uid)
            self.assertEqual(info, f"socket:600:{self.account.pw_uid}")
            pid = run(["systemctl", "show", "-p", "MainPID", "--value", unit]).stdout.strip()
            status = Path("/proc", pid, "status").read_text(encoding="utf-8")
            self.assertIn("NoNewPrivs:\t1", status)
            self.assertIn("CapEff:\t0000000000000000", status)
            self.assertIn("CapBnd:\t0000000000000000", status)
            self.assertEqual(run(["ps", "-o", "user=", "-p", pid]).stdout.strip(), self.account.pw_name)
            self.host(["systemctl", "kill", "-s", "SIGTERM", unit])
            deadline = time.monotonic() + 5
            while time.monotonic() < deadline:
                if run(["systemctl", "is-active", unit], check=False).stdout.strip() == "inactive":
                    break
                time.sleep(0.05)
            else:
                self.fail("service did not stop")
            shown = run(["systemctl", "show", "-p", "ExecMainStatus", "-p", "Result", unit]).stdout
            self.assertIn("ExecMainStatus=0", shown)
            self.assertIn("Result=success", shown)
        finally:
            self.host(["systemctl", "stop", unit], check=False)
            self.host(["rm", "-rf", "--", str(stage), str(Path("/run") / name)])

    def test_sandbox_rejects_host_writes(self):
        if self.host_tools is None:
            self.skipTest("systemd sandbox requires an isolated Ubuntu runner with sudo")
        name = "dc24h-write-" + secrets.token_hex(4)
        state = Path("/var/lib") / name
        runtime = Path("/run") / name
        probes = [Path("/etc") / name, Path("/usr") / name, Path("/root") / name]
        try:
            properties = [item for item in self.sandbox_properties(name) if not item.startswith("-pWorkingDirectory")]
            properties.append(f"-pStateDirectory={name}")
            properties.append("-pStateDirectoryMode=" + self.parsed["Service"]["StateDirectoryMode"])
            for path in probes:
                result = self.host(["systemd-run", "--quiet", "--wait", "--collect", "--pipe", *properties,
                              "/usr/bin/touch", str(path)], check=False)
                self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertFalse(path.exists())
            allowed = self.host(["systemd-run", "--quiet", "--wait", "--collect", "--pipe", *properties,
                           "/usr/bin/touch", str(state / "ok")], check=False)
            self.assertEqual(allowed.returncode, 0, allowed.stderr)
            created = (state / "ok").stat()
            self.assertEqual(created.st_uid, self.account.pw_uid)
            self.assertEqual(stat.S_IMODE(created.st_mode), 0o600)
            self.assertEqual(stat.S_IMODE(state.stat().st_mode), 0o700)
            runtime_write = self.host(["systemd-run", "--quiet", "--wait", "--collect", "--pipe", *properties,
                                 "/usr/bin/touch", str(runtime / "ok")], check=False)
            self.assertEqual(runtime_write.returncode, 0, runtime_write.stderr)
        finally:
            self.host(["rm", "-rf", "--", *map(str, (state, runtime, *probes))])


if __name__ == "__main__":
    unittest.main()
