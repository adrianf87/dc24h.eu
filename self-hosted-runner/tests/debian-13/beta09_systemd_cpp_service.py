"""GitHub-hosted-only, transient systemd service proof for installed old C++.

Never enable a repository or host service, never open a TCP listener, and never
make an operator's package installation. A systemd *system* transient unit runs
the exact previous-version maintenance executable as the unprivileged runner.
"""

import os
from pathlib import Path
import pwd
import shutil
import socket
import subprocess
import time

from api_support import enable_api, fetch_status


def command(args, *, success=True):
    result = subprocess.run(args, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                            stderr=subprocess.DEVNULL, check=False, timeout=25)
    if success and result.returncode:
        raise AssertionError("Isolated systemd service transition refused")
    return result


def require_sandbox_properties(unit, runner, invoke=command):
    """Attest *effective* protections, not merely systemd-run arguments."""
    for name, expected in (("User", runner), ("PrivateNetwork", "yes"),
                           ("NoNewPrivileges", "yes"), ("ProtectSystem", "strict"),
                           ("ProtectHome", "yes"), ("ProtectKernelTunables", "yes"),
                           ("ProtectControlGroups", "yes")):
        value = invoke(["systemctl", "show", "--property=" + name,
                        "--value", unit]).stdout.decode().strip()
        if value != expected:
            raise AssertionError("C++ systemd sandbox property not effective: " + name)
    families = invoke(["systemctl", "show", "--property=RestrictAddressFamilies",
                       "--value", unit]).stdout.decode().strip().split()
    if families != ["AF_UNIX"]:
        raise AssertionError("C++ systemd service permits non-Unix network sockets")


def rehearse(installed_admin, config, database_directory):
    if (os.environ.get("GITHUB_ACTIONS") != "true" or
            os.environ.get("RUNNER_ENVIRONMENT") != "self-hosted" or
            os.environ.get("RUNNER_OS") != "Linux" or
            not shutil.which("systemd-run") or not shutil.which("sudo")):
        raise AssertionError("Transient C++ service fixture requires Debian 13 self-hosted CI")
    installed_admin = Path(installed_admin).resolve(strict=True)
    config = Path(config).resolve(strict=True)
    directory = Path(database_directory).resolve(strict=True)
    if not str(installed_admin).startswith("/usr/lib/dc24h-twilight-ci-fixture/"):
        raise AssertionError("C++ transient unit requires an installed CI package binary")
    if not directory.is_dir() or not config.is_file():
        raise AssertionError("Fresh isolated database and private config required")

    api_socket, token = enable_api(config, directory)
    # A unique transient system service, not the inert dpkg service and not
    # a deployable unit file. systemd's PrivateNetwork prohibits IPv4/IPv6.
    unit = "dc24h-beta09-cpp-" + str(os.getpid()) + ".service"
    runner = pwd.getpwuid(os.geteuid()).pw_name
    started = False
    try:
        command(["sudo", "systemd-run", "--collect", "--unit=" + unit,
                 "--property=Type=exec", "--property=User=" + runner,
                 "--property=PrivateNetwork=yes", "--property=NoNewPrivileges=yes",
                 "--property=ProtectSystem=strict", "--property=ProtectHome=yes",
                 "--property=ProtectKernelTunables=yes", "--property=ProtectControlGroups=yes",
                 "--property=CapabilityBoundingSet=", "--property=RestrictAddressFamilies=AF_UNIX",
                 "--property=ReadWritePaths=" + str(directory),
                 str(installed_admin), "--config", str(config)])
        started = True
        for cycle in range(2):
            deadline = time.monotonic() + 20
            while time.monotonic() < deadline:
                if command(["systemctl", "is-active", "--quiet", unit], success=False).returncode != 0:
                    raise AssertionError("Old C++ systemd maintenance service exited")
                try:
                    status = fetch_status(api_socket, token)
                    if status["server"]["core_running"] is not False:
                        raise AssertionError("Account maintenance accidentally started the public serving hub")
                    break
                except (OSError, socket.timeout):
                    time.sleep(0.1)
            else:
                raise AssertionError("Old C++ systemd maintenance Unix API not ready")
            require_sandbox_properties(unit, runner)
            if cycle == 0:
                command(["sudo", "systemctl", "restart", unit])
        command(["sudo", "systemctl", "stop", unit])
        started = False
        if command(["systemctl", "is-active", "--quiet", unit], success=False).returncode == 0:
            raise AssertionError("Transient C++ service remained active after stop")
    finally:
        if started:
            command(["sudo", "systemctl", "stop", unit], success=False)
        command(["sudo", "systemctl", "reset-failed", unit], success=False)
    print("Installed historical C++ maintenance systemd start/restart/stop over Unix socket passed")
