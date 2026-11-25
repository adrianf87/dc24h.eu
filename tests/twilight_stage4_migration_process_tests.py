"""Actual old-name -> Twilight-name package transition on disposable hosted Ubuntu.

These are *two different*, mutually conflicting, non-deployable CI fixture
packages. No real service, private state, credentials or production path is
touched. Every package transition is explicit; no --force, Replaces or triggers.
"""

import hashlib
import json
import os
from pathlib import Path
import platform
import pwd
import re
import shutil
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import beta07_ci_deb as deb
import beta06_previous_version_rollback as previous


def command(args, *, expected=True, timeout=30):
    result = subprocess.run(args, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, check=False, timeout=timeout)
    if expected and result.returncode:
        raise AssertionError("Hosted Twilight transition failed; inspect CI runner privately")
    return result


def checksum(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def require_host_and_source(current, historical, checkout):
    release = platform.freedesktop_os_release()
    if (os.environ.get("CI") != "true" or os.environ.get("GITHUB_ACTIONS") != "true" or
            os.environ.get("RUNNER_ENVIRONMENT") != "github-hosted" or
            os.environ.get("RUNNER_OS") != "Linux" or release.get("ID") != "ubuntu" or
            release.get("VERSION_ID") not in ("22.04", "24.04", "26.04") or
            not shutil.which("sudo") or not shutil.which("systemctl") or
            not Path("/run/systemd/system").is_dir()):
        raise AssertionError("Migration is limited to disposable GitHub-hosted Ubuntu/systemd")
    sha = os.environ.get("SOURCE_SHA", "")
    if not re.fullmatch(r"[0-9a-f]{40}", sha):
        raise AssertionError("Exact tested head required")
    if command(["git", "rev-parse", "HEAD"]).stdout.decode().strip() != sha:
        raise AssertionError("Current source differs from the tested head")
    if command(["git", "-C", str(checkout), "rev-parse", "HEAD"]).stdout.decode().strip() != previous.PREVIOUS_SHA:
        raise AssertionError("Unexpected historical source")
    if previous.execute(historical, "--version") != previous.PREVIOUS_VERSION:
        raise AssertionError("Historical executable identity mismatch")
    if previous.execute(current, "--version") != "dc24h.eu-twilight beta-0.9.20":
        raise AssertionError("Current executable identity mismatch")
    if command(["dpkg", "--print-architecture"]).stdout.strip() != b"amd64":
        raise AssertionError("Only pinned amd64 fixtures allowed")
    for name in (deb.LEGACY_PACKAGE, deb.PACKAGE):
        if command(["dpkg-query", "-W", "-f=${Status}", name], expected=False).returncode == 0:
            raise AssertionError("Existing package-name fixture; refuse host interference")
    for name in (deb.LEGACY_UNIT, deb.UNIT):
        if command(["systemctl", "is-active", "--quiet", name], expected=False).returncode == 0:
            raise AssertionError("Existing fixture unit is active")
    if Path("/__dc24h_ci_fixture_never_start__").exists():
        raise AssertionError("Inert package-unit guard unexpectedly present")
    return sha, "ubuntu-" + release["VERSION_ID"]


def status(name):
    result = command(["dpkg-query", "-W", "-f=${Status}|${Version}", name], expected=False)
    return result.stdout.decode().strip() if result.returncode == 0 else "absent"


def verify_installed(name, version, binary_path, source_binary, admin_path, source_admin, unit_name):
    if status(name) != "install ok installed|" + version:
        raise AssertionError("Wrong installed named fixture/version")
    if checksum("/" + binary_path) != checksum(source_binary):
        raise AssertionError("Installed hub bytes differ from expected source")
    if checksum("/" + admin_path) != checksum(source_admin):
        raise AssertionError("Installed account-maintenance bytes differ from expected source")
    if command(["systemctl", "is-active", "--quiet", unit_name], expected=False).returncode == 0:
        raise AssertionError("Inert CI service unexpectedly active")
    enablement = command(["systemctl", "is-enabled", unit_name], expected=False).stdout.decode().strip()
    if enablement not in ("static", "disabled", "not-found"):
        raise AssertionError("Inert CI service unexpectedly enabled or linked")
    owned = command(["dpkg-query", "-L", name]).stdout.decode()
    if not all("/" + path in owned.splitlines() for path in (binary_path, admin_path)):
        raise AssertionError("dpkg ownership does not match expected payload")


def prove_systemd_conflict(directory):
    """Use unique inert test units, not actual dc24h or Twilight service names."""
    suffix = str(os.getpid())
    old_name = f"dc24h-stage4-old-ci-{suffix}.service"
    new_name = f"dc24h-stage4-twilight-ci-{suffix}.service"
    marker = Path("/run") / f"dc24h-stage4-ci-{suffix}.approved"
    targets = [Path("/run/systemd/system") / name for name in (old_name, new_name)]
    if any(path.exists() or path.is_symlink() for path in (*targets, marker)):
        raise AssertionError("Unique systemd CI fixture already exists")
    runner = pwd.getpwuid(os.geteuid()).pw_name
    base = ("[Unit]\nDescription=CI-only no-network name transition\n"
            "[Service]\nType=exec\nExecStart=/usr/bin/sleep 120\n"
            f"User={runner}\nPrivateNetwork=yes\nNoNewPrivileges=yes\n"
            "ProtectSystem=strict\nProtectHome=yes\n")
    source_old = directory / "old-ci.service"
    source_new = directory / "new-ci.service"
    source_old.write_text(base, encoding="ascii")
    source_new.write_text(base.replace("[Unit]\n", "[Unit]\nConflicts=" + old_name +
                                       "\nAssertPathExists=" + str(marker) + "\n"), encoding="ascii")
    installed = []
    try:
        for source, target in zip((source_old, source_new), targets):
            command(["sudo", "install", "-m", "0644", str(source), str(target)])
            installed.append(target)
        command(["sudo", "systemctl", "daemon-reload"])
        if command(["sudo", "systemctl", "start", new_name], expected=False).returncode == 0:
            raise AssertionError("New systemd CI unit started without operator marker")
        if command(["systemctl", "is-active", "--quiet", new_name], expected=False).returncode == 0:
            raise AssertionError("New systemd CI unit active despite absent marker")
        command(["sudo", "systemctl", "start", old_name])
        if command(["systemctl", "is-active", "--quiet", old_name], expected=False).returncode:
            raise AssertionError("Synthetic old service failed to start")
        marker_source = directory / "owner-marker"
        marker_source.write_text("ci-only\n", encoding="ascii")
        command(["sudo", "install", "-m", "0600", str(marker_source), str(marker)])
        command(["sudo", "systemctl", "start", new_name])
        # systemctl's start command waits for job completion, not necessarily
        # for unrelated stop jobs; poll both states before attestation.
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline:
            new_active = command(["systemctl", "is-active", "--quiet", new_name], expected=False).returncode == 0
            old_active = command(["systemctl", "is-active", "--quiet", old_name], expected=False).returncode == 0
            if new_active and not old_active:
                break
            time.sleep(0.1)
        else:
            raise AssertionError("Conflicts did not stop synthetic old service")
        command(["sudo", "systemctl", "stop", new_name])
        command(["sudo", "systemctl", "start", old_name])
        if command(["systemctl", "is-active", "--quiet", old_name], expected=False).returncode:
            raise AssertionError("Synthetic old service did not restart during rollback")
        if command(["systemctl", "is-active", "--quiet", new_name], expected=False).returncode == 0:
            raise AssertionError("Two synthetic services active after rollback")
        return True
    finally:
        for name in (new_name, old_name):
            command(["sudo", "systemctl", "stop", name], expected=False)
            command(["sudo", "systemctl", "reset-failed", name], expected=False)
        if marker.exists():
            command(["sudo", "rm", "--", str(marker)])
        for target in installed:
            command(["sudo", "rm", "--", str(target)])
        if installed:
            command(["sudo", "systemctl", "daemon-reload"])


def run(current, historical, checkout, current_admin, historical_admin):
    sha, os_image = require_host_and_source(current, historical, checkout)
    with tempfile.TemporaryDirectory(prefix="twilight-stage4-name-ci-") as directory:
        root = Path(directory)
        old = deb.build(historical, root / "old", deb.VERSIONS["previous"],
                        previous.PREVIOUS_SHA, account_admin=historical_admin, legacy=True)
        new = deb.build(current, root / "new", deb.VERSIONS["current"], sha,
                        account_admin=current_admin)
        failing = deb.build(current, root / "failure", deb.FAILED_UPGRADE_VERSION, sha,
                            account_admin=current_admin, fail_preinst=True)
        touched = False
        try:
            touched = True
            command(["sudo", "dpkg", "--install", str(old)])
            verify_installed(deb.LEGACY_PACKAGE, deb.VERSIONS["previous"], deb.LEGACY_BIN,
                             historical, deb.LEGACY_ADMIN_BIN, historical_admin, deb.LEGACY_UNIT)
            if command(["sudo", "dpkg", "--install", str(new)], expected=False).returncode == 0:
                raise AssertionError("dpkg silently replaced installed legacy package")
            verify_installed(deb.LEGACY_PACKAGE, deb.VERSIONS["previous"], deb.LEGACY_BIN,
                             historical, deb.LEGACY_ADMIN_BIN, historical_admin, deb.LEGACY_UNIT)
            if Path("/" + deb.BIN).exists():
                raise AssertionError("Denied transition unpacked Twilight bytes")
            command(["sudo", "dpkg", "--purge", deb.LEGACY_PACKAGE])
            if command(["sudo", "dpkg", "--install", str(failing)], expected=False).returncode == 0:
                raise AssertionError("Injected new-name preinst failure unexpectedly succeeded")
            if Path("/" + deb.BIN).exists():
                raise AssertionError("Failed install unpacked Twilight hub")
            # A failed preinst may leave a dpkg entry; clear only this unique
            # test package explicitly before restoring the earlier name.
            command(["sudo", "dpkg", "--purge", deb.PACKAGE])
            command(["sudo", "dpkg", "--install", str(old)])
            verify_installed(deb.LEGACY_PACKAGE, deb.VERSIONS["previous"], deb.LEGACY_BIN,
                             historical, deb.LEGACY_ADMIN_BIN, historical_admin, deb.LEGACY_UNIT)
            command(["sudo", "dpkg", "--purge", deb.LEGACY_PACKAGE])
            command(["sudo", "dpkg", "--install", str(new)])
            verify_installed(deb.PACKAGE, deb.VERSIONS["current"], deb.BIN,
                             current, deb.ADMIN_BIN, current_admin, deb.UNIT)
            if Path("/" + deb.LEGACY_BIN).exists():
                raise AssertionError("Old executable survived explicit package migration")
            if command(["sudo", "dpkg", "--install", str(old)], expected=False).returncode == 0:
                raise AssertionError("dpkg silently downgraded to legacy package name")
            verify_installed(deb.PACKAGE, deb.VERSIONS["current"], deb.BIN,
                             current, deb.ADMIN_BIN, current_admin, deb.UNIT)
            command(["sudo", "dpkg", "--purge", deb.PACKAGE])
            command(["sudo", "dpkg", "--install", str(old)])
            verify_installed(deb.LEGACY_PACKAGE, deb.VERSIONS["previous"], deb.LEGACY_BIN,
                             historical, deb.LEGACY_ADMIN_BIN, historical_admin, deb.LEGACY_UNIT)
            systemd_proof = prove_systemd_conflict(root)
            if not systemd_proof:
                raise AssertionError("systemd-name rollback proof missing")
            report_dir = os.environ.get("DC24H_VM_REPORT_DIR")
            if not report_dir:
                raise AssertionError("Protected CI report destination is required")
            report = {
                "schema": 1, "source_sha": sha, "previous_sha": previous.PREVIOUS_SHA,
                "os_image": os_image, "legacy_package": deb.LEGACY_PACKAGE,
                "twilight_package": deb.PACKAGE,
                "legacy_hub_sha256": checksum(historical),
                "twilight_hub_sha256": checksum(current),
                "actual_two_distinct_package_names": True,
                "clean_old_name_install": True,
                "new_name_refused_without_explicit_old_purge": True,
                "failed_new_install_refused_and_old_name_restored": True,
                "explicit_old_to_new_migration": True,
                "legacy_install_refused_while_twilight_installed": True,
                "actual_new_to_old_name_rollback": True,
                "binary_and_account_admin_hashes_verified": True,
                "inert_package_units_not_enabled_or_active": True,
                "systemd_owner_marker_required": True,
                "systemd_conflict_stopped_old_without_parallel_activation": True,
                "synthetic_systemd_rollback_restored_old": True,
                "production_deployment_or_migration_claim": False,
            }
            with (Path(report_dir) / "twilight-stage4-name-migration.json").open("x", encoding="utf8") as handle:
                json.dump(report, handle, sort_keys=True); handle.write("\n")
        finally:
            if touched:
                for name in (deb.PACKAGE, deb.LEGACY_PACKAGE):
                    command(["sudo", "dpkg", "--purge", name], expected=False)
    print("GitHub-hosted CI-only two-package transition and synthetic systemd rollback passed")


if __name__ == "__main__":
    if len(sys.argv) != 6:
        raise SystemExit("Usage: twilight_stage4_migration_process_tests.py "
                         "CURRENT OLD OLD-CHECKOUT CURRENT-ADMIN OLD-ADMIN")
    run(*(Path(value).resolve(strict=True) for value in sys.argv[1:]))
