"""Disposable Stage IV binary/config/MariaDB/private-state outage rehearsal.

This is a same-version *fixture package* rollback, not a package-manager
downgrade, live service operation, production recovery or session login test.
"""

from concurrent.futures import ThreadPoolExecutor
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
import unittest

import stage1_matched_recovery_tests as matched
import stage4_endurance_gate as endurance_gate


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


class IsolatedFailureRollback(unittest.TestCase):
    def test_disposable_service_outage_and_all_state_layers_recovered(self):
        if os.environ.get("GITHUB_ACTIONS") != "true":
            self.skipTest("Stage IV recovery measurements run only on Debian 13 self-hosted CI")
        self.assertEqual(os.environ.get("RUNNER_ENVIRONMENT"), "self-hosted")
        self.assertEqual(os.environ.get("RUNNER_OS"), "Linux")
        binary = Path(sys.argv[1]).resolve()
        self.assertTrue(binary.is_file())
        with tempfile.TemporaryDirectory(prefix="dc24h-stage4-rollback-") as temporary:
            root = Path(temporary)
            source = matched.IsolatedDatabase(root / "primary")
            recovery = matched.IsolatedDatabase(root / "recovery")
            try:
                source.initialize()
                source.start()
                config = source.setup_schema()
                matched.command([str(binary), "--config", str(config), "--migrate-database"])
                source.sql("INSERT INTO dc24h.dc24h_accounts "
                           "(id,name,role,status,created_at,updated_at,revision) "
                           "VALUES (7,'ci-rollback','owner','active',1,2,2);"
                           "INSERT INTO dc24h.dc24h_account_credentials "
                           "(account_id,verifier,created_at) VALUES (7,UNHEX('00FF61'),2);")

                private_root = root / "gateway"
                private_root.mkdir(mode=0o700)
                matched.make_gateway_fixture(private_root)
                snapshot = root / "snapshot"
                matched.private.snapshot(private_root, snapshot)
                dump = root / "checkpoint.sql"
                source.dump(dump)
                pair = root / "pair"
                pair.mkdir(mode=0o700)
                sha = endurance_gate.required_source_sha(matched.SOURCE_SHA)
                matched.bundle.create_pair(dump, snapshot, pair, sha)
                self.assertEqual(matched.bundle.verified_pair(dump, snapshot,
                                 pair / matched.bundle.NAME)["verified_files"], 6)

                # The "package" is an isolated copy of the just-built binary,
                # never /usr/bin, dpkg state, a real unit, or a live config.
                package = root / "installed-hub"
                package_backup = root / "package-backup"
                shutil.copy2(binary, package)
                shutil.copy2(package, package_backup)
                config_backup = root / "config-backup"
                shutil.copy2(config, config_backup)
                expected_package_hash = digest(package)
                expected_config_hash = digest(config)

                # Repeatable, bounded SQL activity on a Unix socket only.
                def check_query(_):
                    start = time.monotonic()
                    actual = source.sql("SELECT role FROM dc24h.dc24h_accounts WHERE id=7;")
                    return actual, (time.monotonic() - start) * 1000

                started = time.monotonic()
                with ThreadPoolExecutor(max_workers=4) as executor:
                    requests = list(executor.map(check_query, range(40)))
                elapsed = time.monotonic() - started
                self.assertTrue(all(value == "owner" for value, _ in requests))
                self.assertLess(elapsed, 15)
                p99 = sorted(item[1] for item in requests)[-1]

                # Inject database service/socket failure and a damaged local
                # package/config before the offline restore, without touching
                # any OS services or installed packages.
                outage_at = time.monotonic()
                source.stop()
                failed = subprocess.run([str(binary), "--config", str(config), "--database-status"],
                                        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                                        check=False, timeout=5)
                self.assertNotEqual(failed.returncode, 0)
                package.write_bytes(b"deliberately-invalid-test-package")
                config.write_text("[database]\nenabled=on\ninvalid=fixture\n", encoding="ascii")
                self.assertNotEqual(digest(package), expected_package_hash)
                self.assertNotEqual(digest(config), expected_config_hash)
                rejected = subprocess.run([str(binary), "--config", str(config), "--check-config"],
                                          stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                                          check=False, timeout=5)
                self.assertNotEqual(rejected.returncode, 0)

                shutil.copy2(package_backup, package)
                shutil.copy2(config_backup, config)
                self.assertEqual(digest(package), expected_package_hash)
                self.assertEqual(digest(config), expected_config_hash)
                self.assertEqual(subprocess.run([str(package), "--version"],
                                 stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                                 timeout=5).returncode, 0)

                recovery.initialize()
                recovery.start()
                recovery.setup_schema()
                recovery.restore_sql(dump)
                restored_state = root / "restored-state"
                matched.private.restore_to_fresh_directory(snapshot, restored_state)
                matched.verify_recovered(recovery, restored_state, package)
                restored_rto = time.monotonic() - outage_at
                self.assertLess(restored_rto, matched.MAX_RECOVERY_SECONDS)
                # A second service outage must leave recovered grants and
                # revocation records intact after a clean daemon restart.
                recovery.stop()
                recovery.start()
                matched.verify_recovered(recovery, restored_state, package)

                report_dir = os.environ.get("DC24H_VM_REPORT_DIR")
                if report_dir:
                    target = Path(report_dir) / "stage4-failure-rollback.json"
                    with target.open("x", encoding="utf-8") as file:
                        json.dump({
                            "schema": 1, "evidence": "debian-13-package-config-db-state",
                            "source_sha": sha,
                            "os": platform.freedesktop_os_release().get("PRETTY_NAME"),
                            "service_socket_outage_observed": True,
                            "corrupted_package_and_config_rejected": True,
                            "package_and_config_restored_byte_exact": True,
                            "fresh_mariadb_v11_and_five_private_files_restored": True,
                            "grants_replay_audit_passkeys_revalidated": True,
                            "recovered_database_service_restarted": True,
                            "synthetic_rpo_lost_acknowledged_writes": 0,
                            "synthetic_rto_seconds": round(restored_rto, 3),
                            "mariadb_query_requests": len(requests), "mariadb_query_errors": 0,
                            "mariadb_query_p99_ms_including_client_startup": round(p99, 3),
                            "mariadb_query_successful_rps": round(len(requests) / elapsed, 3),
                            "capacity_claim": False, "production_rpo_rto_claim": False,
                            "package_manager_downgrade_exercised": False,
                            "gateway_browser_session_reauthentication_exercised": False,
                        }, file, indent=2, sort_keys=True)
                        file.write("\n")
            finally:
                source.stop()
                recovery.stop()


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("Usage: stage4_rollback_process_tests.py BUILT_DC24H")
    unittest.main(argv=[sys.argv[0]])
