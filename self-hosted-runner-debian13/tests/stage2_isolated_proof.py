#!/usr/bin/env python3
"""Synthetic-only recovery and static systemd proof on a Debian 13 runner.

Never reads or modifies the real /etc/dc24h, /var/lib/dc24h, hub units,
databases, credentials, network listeners, or production private state.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time


ROOT = Path(__file__).resolve().parents[2]
UNIT = ROOT / "config/dc24h-twilight.service.example"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def snapshot(folder):
    return {p.name: {"sha256": digest(p), "mode": p.stat().st_mode & 0o777}
            for p in sorted(folder.iterdir()) if p.is_file()}


def exact_copy(source, destination):
    shutil.copytree(source, destination, copy_function=shutil.copy2)
    if snapshot(source) != snapshot(destination):
        raise AssertionError("Synthetic copy does not preserve content and modes")


def synthetic_recovery(scratch):
    live = scratch / "synthetic-state"
    live.mkdir(mode=0o700)
    (live / "users.fixture").write_bytes(b"synthetic-user=review-only\n")
    (live / "checkpoint.fixture").write_bytes(b"schema=v1\ncheckpoint=1\n")
    for item in live.iterdir():
        item.chmod(0o600)
    original = snapshot(live)
    t0 = time.monotonic()
    backup = scratch / "synthetic-backup"
    exact_copy(live, backup)

    # Inject an interrupted upgrade before it is committed, then restore.
    (live / "checkpoint.fixture").write_bytes(b"BROKEN-STAGED-UPGRADE\n")
    failed_upgrade_detected = snapshot(live) != original
    if not failed_upgrade_detected:
        raise AssertionError("Injected failure was not observed")
    shutil.rmtree(live)  # Only this unique, freshly created synthetic path.
    exact_copy(backup, live)
    if snapshot(live) != original:
        raise AssertionError("Restore after injected failure did not match")
    restored_after_failure = True

    # Successfully stage synthetic v2 data, then roll it back to known-good v1.
    (live / "checkpoint.fixture").write_bytes(b"schema=v2\ncheckpoint=2\n")
    upgraded = snapshot(live) != original
    if not upgraded:
        raise AssertionError("Upgrade simulation did not change the checkpoint")
    shutil.rmtree(live)
    exact_copy(backup, live)
    if snapshot(live) != original:
        raise AssertionError("Synthetic rollback changed source content or modes")
    elapsed = round(time.monotonic() - t0, 6)
    return {"backup_verified": True, "restore_after_injected_failure": restored_after_failure,
            "upgrade_simulated": upgraded, "rollback_verified": True,
            "synthetic_rto_seconds": elapsed, "rpo": "synthetic-only; not measured for hub"}


def systemd_contract():
    content = UNIT.read_text(encoding="utf-8")
    expected = (
        "Conflicts=dc24h.service",
        "AssertPathExists=/etc/dc24h/twilight-unit-migration-approved",
        "NoNewPrivileges=yes",
        "ProtectSystem=strict",
        "Restart=on-failure",
    )
    missing = [line for line in expected if line not in content.splitlines()]
    if missing:
        raise AssertionError("Missing required systemd template guards: " + ", ".join(missing))
    return {"template_contract_pass": True,
            "systemd_manager_activated": False,
            "live_unit_conflict_executed": False,
            "reason": "Actual unit activation/conflict tests require a disposable VM"}


def monitoring_simulation():
    started = time.monotonic()
    normal = subprocess.run((sys.executable, "-c", "print('synthetic-ok')"),
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            text=True, timeout=5, check=False)
    abnormal = subprocess.run((sys.executable, "-c", "raise SystemExit(17)"),
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                              text=True, timeout=5, check=False)
    if normal.returncode != 0 or normal.stdout.strip() != "synthetic-ok" or abnormal.returncode != 17:
        raise AssertionError("Synthetic process exit code monitoring failed")
    return {"normal_exit_detected": True, "abnormal_exit_detected": True,
            "elapsed_seconds": round(time.monotonic() - started, 6),
            "power_loss_simulated": False, "hub_restart_tested": False}


def run(temp_parent):
    parent = Path(temp_parent).resolve(strict=True)
    if not parent.is_dir():
        raise ValueError("A real temporary directory is required")
    with tempfile.TemporaryDirectory(prefix="twilight-stage2-synthetic-", dir=parent) as temp:
        scratch = Path(temp)
        return {"schema": 1, "scope": "ISOLATED_SYNTHETIC_ONLY",
                "production_go": False, "migration_go": False, "public_listener_go": False,
                "recovery": synthetic_recovery(scratch),
                "systemd": systemd_contract(),
                "monitoring": monitoring_simulation()}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--scratch-parent", required=True)
    args = parser.parse_args(argv)
    print(json.dumps(run(args.scratch_parent), indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    sys.exit(main())
