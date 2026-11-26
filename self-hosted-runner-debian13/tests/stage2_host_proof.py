#!/usr/bin/env python3
"""OWNER-only Debian 13 host rehearsal with sudo and disposable test-owned state.

This runs a real CMake system-prefix install and real systemd units on the
OWNER-designated *test* VPS. Nothing writes to the existing dc24h service,
database, /etc/dc24h, /var/lib/dc24h, or production listeners. Even though the
runner has unrestricted passwordless sudo, this script owns only a fresh /opt
directory and two uniquely named /run/systemd/system units.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import pwd
import re
import secrets
import shutil
import signal
import stat
import subprocess
import sys
import tempfile
import time


CONFIRMATION = "RUN_STAGE2_SUDO_HOST_TESTS"
RUNNER = "server-747585786"
OWNER = "adrianf87"
RUNNER_USER = "github-runner"
PREFIX_PARENT = Path("/opt")
UNIT_PARENT = Path("/run/systemd/system")


class ProofError(RuntimeError):
    """Stop without exposing raw privileged host output in CI logs."""


def require(condition, reason):
    if not condition:
        raise ProofError(reason)


def command(*argv, timeout=40, allow_failure=False):
    try:
        result = subprocess.run(argv, stdin=subprocess.DEVNULL,
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                text=True, timeout=timeout, check=False)
    except (OSError, subprocess.TimeoutExpired) as exc:
        raise ProofError("Required test command unavailable or timed out: " + argv[0]) from exc
    if result.returncode and not allow_failure:
        raise ProofError("Host rehearsal command failed: " + argv[0] +
                         " (exit " + str(result.returncode) + ")")
    return result


def inside(path, parent):
    """Require a strict descendant, not the parent itself or an alias."""
    candidate = path.resolve(strict=True)
    base = parent.resolve(strict=True)
    return candidate != base and base in candidate.parents


def validate_context(args):
    require(os.geteuid() == 0 and os.environ.get("SUDO_USER") == RUNNER_USER,
            "Run only via sudo -n from github-runner")
    require(args.confirmation == CONFIRMATION and args.runner == RUNNER and
            args.actor == OWNER and args.ref == "refs/heads/main",
            "OWNER/manual/main/runner guard failed")
    require(re.fullmatch(r"[a-f0-9]{40}", args.expected_sha) is not None,
            "Expected exact source SHA")
    require(Path("/proc/1/comm").read_text(encoding="utf-8").strip() == "systemd",
            "Real systemd PID 1 is required")
    release = Path("/etc/os-release").read_text(encoding="utf-8")
    require(re.search(r"(?m)^ID=debian$", release) is not None and
            re.search(r'(?m)^VERSION_ID="?13"?$', release) is not None,
            "Debian 13 required")
    require(PREFIX_PARENT.is_dir() and not PREFIX_PARENT.is_symlink() and
            UNIT_PARENT.is_dir() and not UNIT_PARENT.is_symlink(),
            "Dedicated /opt and runtime systemd directories are required")
    require(args.runner_temp.is_dir() and args.build_dir.is_dir() and
            inside(args.build_dir, args.runner_temp),
            "Build directory must be beneath this runner's temp directory")
    require(args.source_dir.is_dir() and
            command("git", "-C", str(args.source_dir), "rev-parse", "HEAD").stdout.strip()
            == args.expected_sha, "Built source must match protected main")
    require((args.build_dir / "dc24h-twilight").is_file() and
            (args.build_dir / "cmake_install.cmake").is_file(),
            "Exact-SHA C++ build is missing")
    require(args.report.parent.resolve(strict=True) ==
            args.runner_temp.resolve(strict=True) and not args.report.exists() and
            not args.report.is_symlink(), "Report must be a fresh runner-temp file")
    require(command("systemctl", "show", "-p", "Version", "--value").stdout.strip(),
            "Systemd system manager unavailable")
    # Recheck immediately before root mutations: the build may have taken a
    # long time after the initial read-only preflight.
    for unit in ("dc24h.service", "dc24h-twilight.service"):
        state = command("systemctl", "show", "-p", "ActiveState", "--value", unit).stdout.strip()
        require(state == "inactive", "An existing hub unit became active or unknown")
    for entry in Path("/proc").iterdir():
        if not entry.name.isdecimal():
            continue
        try:
            name = (entry / "comm").read_text(encoding="utf-8").strip()
        except FileNotFoundError:
            continue
        except (OSError, UnicodeError) as exc:
            raise ProofError("Cannot verify absence of existing hub processes") from exc
        require(name not in {"dc24h", "dc24h-twilight"},
                "Existing hub process found before root test")


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def snapshot(folder):
    """Synthetic fixture fingerprint: content and POSIX mode, not real data."""
    return {p.name: {"sha256": digest(p), "mode": stat.S_IMODE(p.stat().st_mode)}
            for p in sorted(folder.iterdir()) if p.is_file()}


def wait_for_service(unit, installed_binary, prior_pid=None, timeout=18):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        state = command("systemctl", "show", "-p", "ActiveState", "--value", unit).stdout.strip()
        pid = command("systemctl", "show", "-p", "MainPID", "--value", unit).stdout.strip()
        if state == "active" and pid.isdecimal() and int(pid) > 0 and pid != prior_pid:
            executable = Path("/proc") / pid / "exe"
            try:
                if executable.resolve(strict=True) == installed_binary.resolve(strict=True):
                    return pid
            except OSError:
                pass
        time.sleep(0.15)
    raise ProofError("Test-only hub service failed to become active")


def assert_unit_protection(unit):
    for field, expected in (("User", RUNNER_USER), ("PrivateNetwork", "yes"),
                            ("NoNewPrivileges", "yes"), ("ProtectSystem", "strict"),
                            ("ProtectHome", "yes")):
        value = command("systemctl", "show", "-p", field, "--value", unit).stdout.strip()
        require(value == expected, "Effective systemd protection missing: " + field)


def write_unit(path, body):
    # Refuse to replace even an inactive or symlinked existing unit.
    with path.open("x", encoding="utf-8") as handle:
        handle.write(body)
    os.chmod(path, 0o644)


def host_proof(args):
    validate_context(args)
    test_root = None
    unit_files = []
    hub_unit = None
    conflict_unit = None
    manager_changed = False
    report = {"schema": 1, "source_sha": args.expected_sha,
              "scope": "DISPOSABLE_VPS_TEST_OWNED_HOST_PATHS",
              "sudo_root": True, "system_prefix_install": False,
              "webadmin_assets": False, "systemd_start_restart": False,
              "fault_restart": False, "unit_conflict": False,
              "journal_observed": False, "fixture_backup_restore": False,
              "installed_binary_rollback": False, "cleanup": False,
              "historical_version_rollback": False, "database_restore": False,
              "production_go": False, "migration_go": False,
              "public_listener_go": False}
    original_error = None
    try:
        test_root = Path(tempfile.mkdtemp(prefix="dc24h-stage2-ci-", dir=PREFIX_PARENT))
        require(test_root.parent == PREFIX_PARENT and not test_root.is_symlink(),
                "Refuse non-unique test installation path")
        os.chmod(test_root, 0o755)
        installed = test_root / "installed"
        command("cmake", "--install", str(args.build_dir), "--prefix", str(installed), timeout=100)
        binary = installed / "bin/dc24h-twilight"
        webadmin = installed / "share/dc24h/webadmin/index.html"
        require(binary.is_file() and webadmin.is_file() and
                digest(binary) == digest(args.build_dir / "dc24h-twilight"),
                "System-prefix installation did not preserve C++/WebAdmin")
        report["system_prefix_install"] = True
        report["webadmin_assets"] = True

        # All backup/restore/rollback inputs are generated within our /opt root.
        fixture = test_root / "fixture"
        fixture.mkdir(mode=0o755)
        (fixture / "users.fixture").write_bytes(b"stage2-test-user\n")
        (fixture / "checkpoint.fixture").write_bytes(b"schema=v1\n")
        for path in fixture.iterdir():
            path.chmod(0o600)
        baseline = snapshot(fixture)
        backup = test_root / "backup"
        shutil.copytree(fixture, backup, copy_function=shutil.copy2)
        require(snapshot(backup) == baseline, "Backup fixture mismatch")

        config = fixture / "hub.ini"
        config.write_text("[network]\nlisten=127.0.0.1:0\n", encoding="utf-8")
        config.chmod(0o644)
        user = pwd.getpwnam(RUNNER_USER)
        os.chown(fixture, user.pw_uid, user.pw_gid)

        token = secrets.token_hex(6)
        hub_unit = "dc24h-stage2-" + token + ".service"
        conflict_unit = "dc24h-stage2-" + token + "-conflict.service"
        hub_path = UNIT_PARENT / hub_unit
        conflict_path = UNIT_PARENT / conflict_unit
        for name in (hub_unit, conflict_unit):
            require(command("systemctl", "show", "-p", "LoadState", "--value", name)
                    .stdout.strip() == "not-found", "Test service name is already in use")

        write_unit(hub_path, "[Unit]\nDescription=Stage II test-only Direct Connect hub\n"
                   # Bound orphaned test services even if a runner is killed
                   # before finally can stop the unit: 4 starts per hour.
                   "StartLimitIntervalSec=3600\nStartLimitBurst=4\n"
                   "[Service]\nType=exec\nUser=" + RUNNER_USER + "\n"
                   "WorkingDirectory=" + str(fixture) + "\n"
                   "ExecStart=" + str(binary) + " --config " + str(config) + "\n"
                   "Restart=on-failure\nRestartSec=1\nRuntimeMaxSec=120\n"
                   "NoNewPrivileges=yes\nPrivateNetwork=yes\nPrivateTmp=yes\n"
                   "ProtectHome=yes\nProtectSystem=strict\n"
                   "ProtectKernelTunables=yes\nProtectControlGroups=yes\n"
                   "CapabilityBoundingSet=\nAmbientCapabilities=\n"
                   "RestrictAddressFamilies=AF_UNIX AF_INET AF_INET6\n"
                   "ReadWritePaths=" + str(fixture) + "\n")
        unit_files.append(hub_path)
        write_unit(conflict_path, "[Unit]\nDescription=Stage II test-only service conflict\n"
                   "Conflicts=" + hub_unit + "\nAfter=" + hub_unit + "\n"
                   "[Service]\nType=exec\nExecStart=/usr/bin/sleep 60\n")
        unit_files.append(conflict_path)
        command("systemctl", "daemon-reload")
        manager_changed = True

        command("systemctl", "start", hub_unit)
        first_pid = wait_for_service(hub_unit, binary)
        assert_unit_protection(hub_unit)
        command("systemctl", "restart", hub_unit)
        restart_pid = wait_for_service(hub_unit, binary, first_pid)
        report["systemd_start_restart"] = True

        # Real SIGKILL: observe bounded automatic recovery, not a simulation.
        command("systemctl", "kill", "--signal=SIGKILL", hub_unit)
        wait_for_service(hub_unit, binary, restart_pid)
        report["fault_restart"] = True
        journal = command("journalctl", "--unit", hub_unit, "--no-pager", "-n", "80", "-o", "cat")
        require("Network listening on 127.0.0.1:" in journal.stdout,
                "No bounded startup evidence in test-only service journal")
        report["journal_observed"] = True

        # The conflicting unit is ours; never stop or modify dc24h.service.
        command("systemctl", "start", conflict_unit)
        require(command("systemctl", "is-active", "--quiet", hub_unit,
                        allow_failure=True).returncode != 0,
                "Test-only systemd Conflicts failed to stop test hub")
        require(command("systemctl", "is-active", "--quiet", conflict_unit).returncode == 0,
                "Test-only conflicting unit did not start")
        report["unit_conflict"] = True
        command("systemctl", "stop", conflict_unit)

        (fixture / "checkpoint.fixture").write_bytes(b"BROKEN-INJECTED-STATE\n")
        require(snapshot(fixture)["checkpoint.fixture"] != baseline["checkpoint.fixture"],
                "Test-only fixture failure injection not observed")
        for item in ("users.fixture", "checkpoint.fixture"):
            shutil.copy2(backup / item, fixture / item)
        require(all(snapshot(fixture)[item] == original for item, original in baseline.items()),
                "Restore of test-owned fixture content/modes failed")
        report["fixture_backup_restore"] = True

        historical = test_root / "binary-backup"
        shutil.copy2(binary, historical)
        original_hash = digest(binary)
        binary.write_bytes(b"INTENTIONALLY-BROKEN-TEST-CANDIDATE\n")
        require(digest(binary) != original_hash, "Test-owned upgrade failure not observed")
        shutil.copy2(historical, binary)
        require(digest(binary) == original_hash, "Test-owned binary rollback hash mismatch")
        command(str(binary), "--version")
        report["installed_binary_rollback"] = True
    except (ProofError, OSError, ValueError, subprocess.SubprocessError) as exc:
        original_error = exc
    finally:
        # Stop only unique test services and delete only files created by this
        # invocation, including on failed assertions or interrupted builds.
        cleanup_ok = True
        for name in (conflict_unit, hub_unit):
            if name:
                try:
                    command("systemctl", "stop", name, timeout=20, allow_failure=True)
                    # A failed stop cannot be disguised as successful cleanup.
                    active = command("systemctl", "is-active", "--quiet", name,
                                     allow_failure=True)
                    if active.returncode == 0:
                        cleanup_ok = False
                except ProofError:
                    cleanup_ok = False
        for path in unit_files:
            try:
                path.unlink()
            except OSError:
                cleanup_ok = False
        if manager_changed or unit_files:
            try:
                command("systemctl", "daemon-reload", timeout=20)
                for name in (conflict_unit, hub_unit):
                    if name:
                        command("systemctl", "reset-failed", name, allow_failure=True)
            except ProofError:
                cleanup_ok = False
        if test_root is not None and cleanup_ok:
            try:
                require(test_root.parent == PREFIX_PARENT and
                        test_root.name.startswith("dc24h-stage2-ci-") and
                        not test_root.is_symlink(), "Unsafe test directory cleanup target")
                shutil.rmtree(test_root)
            except (ProofError, OSError):
                cleanup_ok = False
        report["cleanup"] = cleanup_ok
        # Created as root but readable by the unprivileged artifact uploader.
        with args.report.open("x", encoding="utf-8") as handle:
            json.dump(report, handle, indent=2, sort_keys=True)
            handle.write("\n")
        args.report.chmod(0o644)
    if not report["cleanup"]:
        raise ProofError("Test-unit cleanup failed; OWNER must inspect test-only remnants")
    if original_error:
        raise ProofError("Stage II privileged host qualification failed; inspect bounded evidence") from original_error
    require(all(report[k] for k in (
        "system_prefix_install", "webadmin_assets", "systemd_start_restart",
        "fault_restart", "unit_conflict", "journal_observed",
        "fixture_backup_restore", "installed_binary_rollback")),
        "Privileged host evidence incomplete")
    return report


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--confirmation", required=True)
    parser.add_argument("--runner", required=True)
    parser.add_argument("--actor", required=True)
    parser.add_argument("--ref", required=True)
    parser.add_argument("--expected-sha", required=True)
    parser.add_argument("--source-dir", required=True, type=Path)
    parser.add_argument("--build-dir", required=True, type=Path)
    parser.add_argument("--runner-temp", required=True, type=Path)
    parser.add_argument("--report", required=True, type=Path)
    args = parser.parse_args(argv)
    # No automatic fallback to an alternate host, target directory, or user.
    def interrupted(signum, frame):
        raise ProofError("Host rehearsal interrupted; perform test-only cleanup")

    signal.signal(signal.SIGTERM, interrupted)
    signal.signal(signal.SIGINT, interrupted)
    try:
        host_proof(args)
    except (ProofError, OSError) as exc:
        print("FAIL: " + str(exc), file=sys.stderr)
        return 1
    print("PASS: Debian 13 test-only root install, systemd, recovery and rollback")
    return 0


if __name__ == "__main__":
    sys.exit(main())
