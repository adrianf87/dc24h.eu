"""Negative evidence tests: no invented success from a missing or failed gate."""
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import beta06_integrated_chaos_gate as gate

SHA = "a" * 40
OS = "ubuntu-24.04"


class IntegratedGateTests(unittest.TestCase):
    def fixtures(self, root):
        data = {
            "beta06-authenticated-https.json": {
                "schema": 1, "source_sha": SHA, "os_image": OS,
                "evidence": "real-account-admin-node-apache-authenticated-totp",
                "maintenance_mode": True, "hub_readonly_boundary_verified": True,
                "requests": 24, "success": 24, "failure": 0,
                "request_spacing_ms": 1100,
                "batch_spacing_ms": 2200,
                "capacity_claim": False, "synthetic_credentials_only": True,
                "p99_ms": 45, "max_ci_p99_ms": 2500,
                "successful_requests_per_second": 17.6, "cpu_mean_percent_one_core": 45,
                "rss_after_kib": {"account_admin": 1234, "gateway": 4567, "apache": 2222},
            },
            "beta06-restored-login.json": {
                "schema": 1, "source_sha": SHA, "os_image": OS,
                "fresh_password_totp_login_accepted": True, "old_browser_cookie_refused": True,
                "revoked_passkey_login_options_refused": True, "production_recovery_claim": False,
                "real_browser_webauthn_exercised": True,
                "restored_browser_password_passkey_accepted": True,
                "restored_browser_summary_accepted": True,
                "restored_browser_assertion_replay_refused": True,
                "restored_browser_revoke_invalidated_session": True,
                "revoked_passkey_persisted_after_restart": True,
            },
            "beta06-previous-version-rollback.json": {
                "schema": 1, "source_sha": SHA, "os_image": OS,
                "historical_binary_distinct": True, "previous_binary_database_status_accepted": True,
                "fresh_mariadb_v11_and_five_private_files_restored": True,
                "previous_version": "beta-0.5.16", "current_version": "beta-0.8.20",
                "production_rollback_claim": False,
            },
            "stage4-endurance.json": {
                "schema": 1, "source_sha": SHA, "os": OS,
                "ci_regression_gate_passed": True, "production_capacity_claim": False,
            },
            "loopback-probes.json": {
                "schema": 1, "evidence": "isolated-ubuntu-vm-only",
                "profiles": {name: {"profile": name, "requests": 12, "success": 12,
                                    "failure": 0, "capacity_claim": False} for name in gate.PROFILES},
            },
            "beta06-authenticated-gate.json": {
                "schema": 1, "source_sha": SHA, "os_image": OS,
                "ci_regression_gate_passed": True,
            },
        }
        for name, payload in data.items():
            (root / name).write_text(json.dumps(payload), encoding="utf-8")
        junit = root / "faults.xml"
        junit.write_text('<testsuite>' + ''.join(
            f'<testcase name="{name}"/>' for name in gate.FAULT_CASES) + '</testsuite>', encoding='utf-8')
        return data, junit

    def test_full_matching_evidence(self):
        with tempfile.TemporaryDirectory() as path:
            root = Path(path)
            _, junit = self.fixtures(root)
            result = gate.assess(root, junit, SHA, OS)
            self.assertTrue(result["ci_regression_gate_passed"])
            self.assertFalse(result["production_claim"])

    def test_missing_stale_forged_skipped_and_failed_fail_closed(self):
        for mutate in ("missing", "sha", "os", "fake_success", "rss", "no_browser", "no_fault", "skipped", "failed", "duplicate"):
            with self.subTest(mutate=mutate), tempfile.TemporaryDirectory() as path:
                root = Path(path)
                data, junit = self.fixtures(root)
                if mutate == "missing":
                    (root / "beta06-restored-login.json").unlink()
                elif mutate == "no_browser":
                    key = "beta06-restored-login.json"
                    data[key]["restored_browser_password_passkey_accepted"] = False
                    (root / key).write_text(json.dumps(data[key]))
                elif mutate in ("sha", "os", "fake_success", "rss"):
                    key = "beta06-authenticated-https.json"
                    if mutate == "sha": data[key]["source_sha"] = "b" * 40
                    if mutate == "os": data[key]["os_image"] = "debian-13"
                    if mutate == "fake_success": data[key]["success"] = True
                    if mutate == "rss": data[key]["rss_after_kib"] = {}
                    (root / key).write_text(json.dumps(data[key]))
                else:
                    name = gate.FAULT_CASES[0]
                    if mutate == "no_fault": junit.write_text('<testsuite/>')
                    if mutate == "skipped": junit.write_text(f'<testsuite><testcase name="{name}"><skipped/></testcase></testsuite>')
                    if mutate == "failed": junit.write_text(f'<testsuite><testcase name="{name}"><failure/></testcase></testsuite>')
                    if mutate == "duplicate": junit.write_text('<testsuite>' + ''.join(
                        f'<testcase name="{item}"/>' for item in gate.FAULT_CASES) +
                        f'<testcase name="{name}"/></testsuite>')
                with self.assertRaises((ValueError, FileNotFoundError)):
                    gate.assess(root, junit, SHA, OS)


if __name__ == "__main__":
    unittest.main()
