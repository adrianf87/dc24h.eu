"""Actual CI-only dpkg previous -> current -> previous plus matched v11 restore.

Runs exclusively on disposable GitHub-hosted Ubuntu machines. The unique test
package owns only a binary, inert non-enabled systemd unit and provenance. It
is purged after testing. Never run this on an operator or production host.
"""

import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import beta07_ci_deb as deb
import beta08_recovery_auth as authenticity
import stage1_matched_recovery_tests as matched
import beta06_previous_version_rollback as previous


def quiet(argv, success=True, timeout=50, cwd=None):
    result = subprocess.run(argv, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, timeout=timeout, check=False, cwd=cwd)
    if success and result.returncode:
        raise AssertionError("Isolated dpkg fixture transition refused; inspect runner privately")
    return result


def exact_file_sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def run(current, historical, checkout, *, current_admin=None, historical_admin=None, seeder=None):
    historical_auth = (current_admin, historical_admin, seeder)
    if any(value is None for value in historical_auth) and any(value is not None for value in historical_auth):
        raise AssertionError("Both maintenance binaries and the synthetic account seeder are required")
    if (os.environ.get("CI") != "true" or os.environ.get("GITHUB_ACTIONS") != "true" or
            os.environ.get("RUNNER_ENVIRONMENT") != "github-hosted" or
            os.environ.get("RUNNER_OS") != "Linux" or
            platform.freedesktop_os_release().get("ID") != "ubuntu" or
            platform.freedesktop_os_release().get("VERSION_ID") not in ("22.04", "24.04", "26.04") or
            not shutil.which("sudo")):
        raise AssertionError("The dpkg test is allowed only on disposable GitHub-hosted Ubuntu")
    sha = os.environ.get("SOURCE_SHA", "")
    if not re.fullmatch(r"[0-9a-f]{40}", sha):
        raise AssertionError("Exact source SHA required")
    if quiet(["git", "-C", str(checkout), "rev-parse", "HEAD"]).stdout.decode().strip() != previous.PREVIOUS_SHA:
        raise AssertionError("Unexpected previous package source")
    if previous.execute(historical, "--version") != previous.PREVIOUS_VERSION:
        raise AssertionError("Previous binary version mismatch")
    if previous.execute(current, "--version") != "dc24h.eu beta-0.9.20":
        raise AssertionError("Current test fixture version mismatch")
    if quiet(["dpkg-query", "-W", "-f=${Status}", deb.PACKAGE], success=False).returncode == 0:
        raise AssertionError("CI-only package name is already installed; refuse interference")
    if Path("/__dc24h_ci_fixture_never_start__").exists():
        raise AssertionError("The inert fixture unit guard must remain absent")

    installed_binary = Path("/usr/lib") / deb.PACKAGE / "dc24h"
    installed_admin = Path("/") / deb.ADMIN_BIN
    with tempfile.TemporaryDirectory(prefix="beta07-hosted-dpkg-") as directory:
        root = Path(directory)
        old_pkg = deb.build(historical, root / "old-stage", deb.VERSIONS["previous"], previous.PREVIOUS_SHA,
                            account_admin=historical_admin)
        new_pkg = deb.build(current, root / "new-stage", deb.VERSIONS["current"], sha,
                            account_admin=current_admin)
        failed_pkg = deb.build(current, root / "failed-stage", deb.FAILED_UPGRADE_VERSION,
                               sha, account_admin=current_admin, fail_preinst=True)
        post_failed_pkg = deb.build(current, root / "failed-post-stage", deb.FAILED_POSTINST_VERSION,
                                    sha, account_admin=current_admin, fail_postinst=True)
        original, recovered = matched.IsolatedDatabase(root / "source"), matched.IsolatedDatabase(root / "recovered")
        installed = False
        try:
            # Independently paired SQL + all five private files, captured
            # before any package switch. No production database/service used.
            original.initialize(); original.start()
            config = original.setup_schema()
            previous.execute(current, "--config", str(config), "--migrate-database")
            original.sql("INSERT INTO dc24h.dc24h_accounts "
                         "(id,name,role,status,created_at,updated_at,revision) "
                         "VALUES (7,'ci-dpkg','owner','active',1,2,2);"
                         "INSERT INTO dc24h.dc24h_accounts "
                         "(id,name,role,status,created_at,updated_at,revision) "
                         "VALUES (8,'ci-disabled','administrator','disabled',1,2,3);"
                         "INSERT INTO dc24h.dc24h_account_credentials "
                         "(account_id,verifier,created_at) VALUES (7,UNHEX('00FF61'),2);")
            gateway = root / "gateway"; gateway.mkdir(mode=0o700)
            matched.make_gateway_fixture(gateway)
            snapshot = root / "snapshot"
            dump = root / "state.sql"
            pair_dir = root / "pair"; pair_dir.mkdir(mode=0o700)
            with original.locked_offline_capture():
                before = {name: matched.bundle.private_hash(gateway / name, maximum)
                          for name, maximum in matched.private.FILES.items()}
                original.dump(dump)
                if matched.offline_preflight.sql_migration_row_count(dump) != 11:
                    raise AssertionError("Package rollback source is not a v11 SQL export")
                matched.private.snapshot(gateway, snapshot)
                matched.bundle.create_pair(dump, snapshot, pair_dir, sha)
                after = {name: matched.bundle.private_hash(gateway / name, maximum)
                         for name, maximum in matched.private.FILES.items()}
                if before != after:
                    raise AssertionError("Private state changed during package capture")
            trust = root / "offline-trust"; trust.mkdir(mode=0o700)
            key = trust / "key"
            matched.private.write_private(trust, key.name, os.urandom(32))
            ledger = trust / "accepted-recovery-pairs"
            authenticity.initialize_ledger(ledger)
            signed = root / "authenticated-pair"; signed.mkdir(mode=0o700)
            pinned_id = authenticity.sign(pair_dir, key, signed)
            original.stop()

            quiet(["sudo", "dpkg", "--install", str(old_pkg)]); installed = True
            if previous.execute(installed_binary, "--version") != previous.PREVIOUS_VERSION:
                raise AssertionError("Installed previous binary failed to execute")
            quiet(["sudo", "dpkg", "--install", str(new_pkg)])
            if previous.execute(installed_binary, "--version") != "dc24h.eu beta-0.9.20":
                raise AssertionError("Installed current binary failed to execute")
            if exact_file_sha(installed_binary) != exact_file_sha(current):
                raise AssertionError("Installed current executable hash mismatch")
            if current_admin is not None and exact_file_sha(installed_admin) != exact_file_sha(current_admin):
                raise AssertionError("Installed current maintenance executable hash mismatch")
            if quiet(["dpkg-query", "-W", "-f=${Status}", deb.PACKAGE]).stdout.decode() != "install ok installed":
                raise AssertionError("Current version is not configured in dpkg")
            # Distinguish an archive-reader refusal from a real dpkg upgrade
            # attempt. Both must leave the earlier executable byte-identical.
            rejected = root / "broken-upgrade.deb"
            rejected.write_bytes(b"invalid test-only dpkg archive\n")
            if quiet(["sudo", "dpkg", "--install", str(rejected)], success=False).returncode == 0:
                raise AssertionError("Corrupt upgrade unexpectedly accepted")
            if exact_file_sha(installed_binary) != exact_file_sha(current):
                raise AssertionError("Failed upgrade changed live test binary")
            if quiet(["sudo", "dpkg", "--install", str(failed_pkg)], success=False).returncode == 0:
                raise AssertionError("Injected dpkg preinst failure unexpectedly succeeded")
            if exact_file_sha(installed_binary) != exact_file_sha(current):
                raise AssertionError("Failed pre-unpack upgrade replaced the installed executable")
            if quiet(["dpkg-query", "-W", "-f=${Status}|${Version}", deb.PACKAGE]).stdout.decode() != (
                    "install ok installed|" + deb.VERSIONS["current"]):
                raise AssertionError("Failed pre-unpack upgrade altered dpkg installed state")

            # A genuine post-unpack failure is more disruptive than preinst:
            # dpkg may have replaced files and left the new package in a
            # half-configured state. Recovery must perform an actual downgrade.
            if quiet(["sudo", "dpkg", "--install", str(post_failed_pkg)], success=False).returncode == 0:
                raise AssertionError("Injected dpkg postinst failure unexpectedly succeeded")
            status = quiet(["dpkg-query", "-W", "-f=${Status}|${Version}", deb.PACKAGE]).stdout.decode()
            if status == "install ok installed|" + deb.FAILED_POSTINST_VERSION or (
                    not status.endswith("|" + deb.FAILED_POSTINST_VERSION)):
                raise AssertionError("Post-unpack failure state was not captured honestly")
            if quiet(["systemctl", "is-active", "--quiet", deb.UNIT], success=False).returncode == 0:
                raise AssertionError("Inert CI fixture service unexpectedly activated")

            quiet(["sudo", "dpkg", "--install", str(old_pkg)])  # Real package downgrade.
            if exact_file_sha(installed_binary) != exact_file_sha(historical) or (
                    previous.execute(installed_binary, "--version") != previous.PREVIOUS_VERSION):
                raise AssertionError("Previous installed executable not restored")
            if historical_admin is not None:
                if (exact_file_sha(installed_admin) != exact_file_sha(historical_admin) or
                        previous.execute(installed_admin, "--version") !=
                        "dc24h-account-admin " + previous.PREVIOUS_VERSION.split(" ")[-1]):
                    raise AssertionError("Actual installed historical maintenance executable not restored")
            owned = quiet(["dpkg-query", "-L", deb.PACKAGE]).stdout.decode("utf8")
            if not all(path in owned for path in ("/" + deb.BIN, "/" + deb.UNIT_PATH,
                                                   "/" + deb.DOC_PATH) +
                       (("/" + deb.ADMIN_BIN,) if historical_admin is not None else ())):
                raise AssertionError("Package lacks exact binary/unit/provenance ownership")
            if quiet(["dpkg-query", "-W", "-f=${Version}", deb.PACKAGE]).stdout.decode() != deb.VERSIONS["previous"]:
                raise AssertionError("dpkg database did not record previous version")
            if quiet(["dpkg-query", "-W", "-f=${Status}", deb.PACKAGE]).stdout.decode() != "install ok installed":
                raise AssertionError("Previous version was not configured after downgrade")
            pair_result = matched.bundle.verified_pair(dump, snapshot, pair_dir / matched.bundle.NAME, sha)
            if pair_result["verified_files"] != 6:
                raise AssertionError("Incomplete matched rollback pair")
            restored = root / "restored-gateway"
            preflight = matched.offline_preflight.preflight(
                dump, snapshot, pair_dir, sha, recovered.datadir, restored)
            if preflight["verified_components"] != 6 or preflight["sql_export_applied_migration_rows"] != 11:
                raise AssertionError("Matched rollback preflight refused the v11 source")
            authenticity.verify(dump, snapshot, pair_dir, sha, signed, key, pinned_id, ledger,
                                consume=True)
            recovered.initialize(); recovered.start(); recovered_config = recovered.setup_schema()
            recovered.restore_sql(dump)
            matched.private.restore_to_fresh_directory(snapshot, restored)
            matched.verify_recovered(recovered, restored, installed_binary)
            recovered.stop(); recovered.start()
            matched.verify_recovered(recovered, restored, installed_binary)
            if recovered.sql("SELECT status FROM dc24h.dc24h_accounts WHERE id=8;") != "disabled":
                raise AssertionError("Revoked account unexpectedly active after prior-binary restart")
            if historical_admin is not None:
                from beta09_systemd_cpp_service import rehearse
                rehearse(installed_admin, recovered_config, recovered.root)
            # Reopen the restored private stores in the actual Node gateway
            # verifier after the previous C++ package and fresh DB restart.
            quiet(["node", "--import", "tsx", "tests/beta08-postrollback-state.ts", str(restored)],
                  cwd=Path(__file__).resolve().parents[1] / "webadmin")
            try:
                authenticity.verify(dump, snapshot, pair_dir, sha, signed, key, pinned_id, ledger)
            except ValueError:
                pass
            else:
                raise AssertionError("Consumed rollback pair was erroneously reusable")

            # This is deliberately a *second*, independent v11+five-store
            # disposable matched restoration. It exercises the actual old
            # account-maintenance executable installed by dpkg (not merely a
            # linked historical library) through the real Node + Apache TLS
            # + Chromium password/TOTP and password/passkey flows. The auth
            # fixture itself enforces restored replay, revocation and grants.
            # Any protocol incompatibility is a hard test failure.
            if historical_admin is not None:
                auth_environment = dict(os.environ)
                auth_environment.pop("DC24H_VM_REPORT_DIR", None)
                fixture = Path(__file__).with_name("beta06_authenticated_api_process_tests.py")
                result = subprocess.run(
                    [sys.executable, str(fixture), str(current), str(seeder), str(installed_admin)],
                    stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                    cwd=Path(__file__).resolve().parents[1], env=auth_environment, timeout=600)
                if result.returncode:
                    raise AssertionError("Historical installed maintenance login or matched recovery refused")

            report_dir = os.environ.get("DC24H_VM_REPORT_DIR")
            if report_dir:
                report = {"schema": 1, "source_sha": sha, "previous_sha": previous.PREVIOUS_SHA,
                          "os_image": "ubuntu-" + platform.freedesktop_os_release()["VERSION_ID"],
                          "package": deb.PACKAGE, "previous_package": deb.VERSIONS["previous"],
                          "candidate_package": deb.VERSIONS["current"],
                          "actual_dpkg_installed_upgrade_downgrade": True,
                          "failed_upgrade_refused_without_binary_replacement": True,
                          "valid_preinst_failure_kept_previous_installed_state": True,
                          "valid_postinst_failure_required_explicit_previous_package_recovery": True,
                          "inert_ci_service_remained_inactive": True,
                          "installed_binary_and_unit_owned": True,
                          "previous_executable_hash_and_version_matched": True,
                          "fresh_sql_and_five_store_preflight_succeeded": True,
                          "schema_v11_and_private_five_file_restore_verified": True,
                          "independently_pinned_offline_hmac_and_anti_reuse_gate": True,
                          "reopened_node_totp_revocation_audit_and_budget_state": True,
                          "revoked_account_stays_disabled_after_previous_binary_restart": True,
                          "restart_of_fresh_database_accepted": True,
                          "historical_maintenance_binary_installed_and_hashed": historical_admin is not None,
                          "historical_maintenance_actual_password_totp_and_browser_passkey": historical_admin is not None,
                          "historical_maintenance_independent_matched_v11_five_store_recovery": historical_admin is not None,
                          "actual_systemd_old_cpp_maintenance_start_restart_stop": historical_admin is not None,
                          "production_package_or_rollback_claim": False}
                with (Path(report_dir) / "beta07-dpkg-rollback.json").open("x", encoding="utf8") as output:
                    json.dump(report, output, sort_keys=True); output.write("\n")
        finally:
            original.stop(); recovered.stop()
            if installed:
                quiet(["sudo", "dpkg", "--purge", deb.PACKAGE])
    print("Disposable hosted Ubuntu dpkg upgrade/downgrade and private v11 rollback passed")


if __name__ == "__main__":
    if len(sys.argv) not in (4, 7):
        raise SystemExit("Usage: beta07_dpkg_process_tests.py CURRENT PREVIOUS PREVIOUS-CHECKOUT "
                         "[CURRENT-ADMIN PREVIOUS-ADMIN SEEDER]")
    run(Path(sys.argv[1]).resolve(strict=True), Path(sys.argv[2]).resolve(strict=True),
        Path(sys.argv[3]).resolve(strict=True),
        **(dict(zip(("current_admin", "historical_admin", "seeder"),
                    (Path(value).resolve(strict=True) for value in sys.argv[4:]))) if len(sys.argv) == 7 else {}))
