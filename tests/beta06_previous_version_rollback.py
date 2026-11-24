"""True prior-version C++ binary against fresh disposable SQL/private recovery.

Requires a separate, immutable beta-0.5.16 source checkout and built binary.
Never operates dpkg, installed services, production databases or live state.
"""

import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tempfile
import time

import stage1_matched_recovery_tests as matched
import stage4_endurance_gate as provenance


PREVIOUS_SHA = "5cd7b571fd974cbb7d59d8e4478185ff819e248a"
PREVIOUS_VERSION = "dc24h.eu beta-0.5.16"


def execute(binary, *args):
    result = subprocess.run([str(binary), *args], capture_output=True, timeout=30)
    if result.returncode:
        raise AssertionError("Disposable version/config/database validation refused")
    return result.stdout.decode("utf-8").strip()


def fingerprint(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def run(current_binary, previous_binary, previous_checkout):
    if not (os.environ.get("GITHUB_ACTIONS") == "true" and
            os.environ.get("RUNNER_ENVIRONMENT") == "github-hosted" and
            os.environ.get("RUNNER_OS") == "Linux"):
        raise AssertionError("Historical rollback requires hosted Ubuntu CI")
    os_version = platform.freedesktop_os_release().get("VERSION_ID")
    if os_version not in ("22.04", "24.04", "26.04"):
        raise AssertionError("Unexpected Ubuntu image")
    sha = provenance.required_source_sha(os.environ.get("SOURCE_SHA"))
    actual_previous_sha = subprocess.check_output(
        ["git", "-C", str(previous_checkout), "rev-parse", "HEAD"], timeout=5).decode().strip()
    if actual_previous_sha != PREVIOUS_SHA:
        raise AssertionError("Historical checkout provenance is not pinned")
    if execute(previous_binary, "--version") != PREVIOUS_VERSION:
        raise AssertionError("Historical binary is not beta-0.5.16")
    current_version = execute(current_binary, "--version")
    if current_version != "dc24h.eu beta-0.7.20":
        raise AssertionError("A same-version package copy cannot prove historical rollback")

    with tempfile.TemporaryDirectory(prefix="dc24h-beta06-prior-version-") as directory:
        base = Path(directory)
        first = matched.IsolatedDatabase(base / "source")
        prior = matched.IsolatedDatabase(base / "rollback")
        try:
            first.initialize(); first.start()
            current_config = first.setup_schema()
            execute(current_binary, "--config", str(current_config), "--migrate-database")
            first.sql("INSERT INTO dc24h.dc24h_accounts "
                      "(id,name,role,status,created_at,updated_at,revision) "
                      "VALUES (7,'ci-prior-version','owner','active',1,2,2);"
                      "INSERT INTO dc24h.dc24h_account_credentials "
                      "(account_id,verifier,created_at) VALUES (7,UNHEX('00FF61'),2);")
            private_root = base / "gateway"
            private_root.mkdir(mode=0o700)
            matched.make_gateway_fixture(private_root)
            snapshot = base / "snapshot"
            matched.private.snapshot(private_root, snapshot)
            dump = base / "checkpoint.sql"
            first.dump(dump)
            pair = base / "pair"
            pair.mkdir(mode=0o700)
            matched.bundle.create_pair(dump, snapshot, pair, sha)
            matched.bundle.verified_pair(dump, snapshot, pair / matched.bundle.NAME, sha)
            first.stop()

            # Reproduce an incompatible copied package, then select the
            # immutable previous binary in an entirely *new* installation.
            staged = base / "candidate"
            shutil.copy2(current_binary, staged)
            staged.write_bytes(b"corrupted isolated candidate")
            if fingerprint(staged) == fingerprint(previous_binary):
                raise AssertionError("Corrupt candidate falsely matches historical package")
            rolled_back = base / "prior-hub"
            shutil.copy2(previous_binary, rolled_back)
            if fingerprint(rolled_back) != fingerprint(previous_binary):
                raise AssertionError("Historical package rollback byte mismatch")
            if execute(rolled_back, "--version") != PREVIOUS_VERSION:
                raise AssertionError("Restored package is not the previous version")

            prior.initialize(); prior.start()
            config = prior.setup_schema()
            prior.restore_sql(dump)
            restored = base / "restored-gateway"
            matched.private.restore_to_fresh_directory(snapshot, restored)
            matched.verify_recovered(prior, restored, rolled_back)
            status = execute(rolled_back, "--config", str(config), "--database-status")
            if "version=11 latest=11 pending=0 dirty=no" not in status:
                raise AssertionError("Prior binary rejected unchanged MariaDB v11 schema")
            # Full service restart checks the same binary/config/SQL shape,
            # never writes to the active development workspace or live host.
            prior.stop(); prior.start()
            matched.verify_recovered(prior, restored, rolled_back)

            report_dir = os.environ.get("DC24H_VM_REPORT_DIR")
            if report_dir:
                with (Path(report_dir) / "beta06-previous-version-rollback.json").open(
                    "x", encoding="utf-8") as output:
                    json.dump({"schema": 1, "source_sha": sha,
                               "prior_source_sha": PREVIOUS_SHA,
                               "os_image": f"ubuntu-{os_version}",
                               "current_version": current_version.removeprefix("dc24h.eu "),
                               "previous_version": PREVIOUS_VERSION.removeprefix("dc24h.eu "),
                               "historical_binary_distinct": True,
                               "prior_package_byte_exact": True,
                               "fresh_mariadb_v11_and_five_private_files_restored": True,
                               "previous_binary_database_status_accepted": True,
                               "fresh_database_service_restarted": True,
                               "package_manager_rollback_exercised": False,
                               "production_rollback_claim": False,
                               "real_browser_reauthentication_exercised": False},
                              output, indent=2, sort_keys=True)
                    output.write("\n")
        finally:
            first.stop(); prior.stop()


if __name__ == "__main__":
    if len(sys.argv) != 4:
        raise SystemExit("Usage: beta06_previous_version_rollback.py CURRENT PREVIOUS PREVIOUS_CHECKOUT")
    run(Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve(), Path(sys.argv[3]).resolve())
