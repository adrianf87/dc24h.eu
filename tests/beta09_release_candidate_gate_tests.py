"""Offline synthetic NO-GO fixture; no fabricated report can authorize merge."""

import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import beta09_release_candidate_gate as gate


SHA = "a" * 40


def fixture(root):
    source, reports, hardening = root / "source", root / "reports", root / "hardening"
    entries = {
        "CMakeLists.txt": 'project(dc24h VERSION 0.9.20 LANGUAGES CXX)\nset(DC24H_VERSION "beta-0.9.20")',
        "webadmin/src/lib/status.ts": 'WEBADMIN_VERSION = "beta-0.9.20"',
        "webadmin/tests/remote-incident-journal.test.ts": 'version: "beta-0.9.20"',
        "README.md": "Current development version: beta-0.9.20",
        "doc/README.md": "Current development version: beta-0.9.20",
        "webadmin/README.md": "beta-0.9.20",
        "CHANGELOG.md": "beta-0.9.17–beta-0.9.20",
        "doc/us/BETA_0.9_STAGE_V.md": "NO-GO",
        "doc/pl/BETA_0.9_STAGE_V.md": "NO-GO",
    }
    for name, content in entries.items():
        path = source / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content)
    package = {"name": "dc24h-webadmin", "version": "0.9.20",
               "dependencies": {"runtime": "1.0.0"},
               "devDependencies": {"build": "2.0.0"}}
    (source / "webadmin/package.json").write_text(json.dumps(package))
    (source / "webadmin/package-lock.json").write_text(json.dumps({
        **{k: package[k] for k in ("name", "version")}, "lockfileVersion": 3,
        "requires": True, "packages": {"": package,
        "node_modules/runtime": {"version": "1.0.0", "resolved": "https://registry.npmjs.org/runtime/-/runtime-1.0.0.tgz", "integrity": "sha512-" + "a" * 44},
        "node_modules/build": {"version": "2.0.0", "dev": True, "resolved": "https://registry.npmjs.org/build/-/build-2.0.0.tgz", "integrity": "sha512-" + "a" * 44}}}))
    locksha = hashlib.sha256((source / "webadmin/package-lock.json").read_bytes()).hexdigest()
    profiles = {name: {"windows": 3, "samples": 3 * policy["requests"],
                       "failed_requests": 0, "ci_regression_envelope_passed": True,
                       "max_observed_p99_ms": 10, "min_success_rps": 8.0,
                       "max_cpu_percent_one_core": 10, "rss_growth_kib": 1024}
                for name, policy in gate.matrix.POLICY.items()}
    for image in gate.IMAGES:
        folder = reports / image
        folder.mkdir(parents=True)
        standard = {"schema": 1, "source_sha": SHA, "os_image": image}
        records = {
            "beta09-os-provenance.json": {
                "npm_lock_sha256": locksha, "runner_environment": "github-hosted",
                "fully_reproducible_os_build": False, "apt_repository_snapshot_immutable": False,
                "selected_public_apt_versions": {name: "1.0" for name in gate.host.REQUIRED_APT},
                "apt_full_installed_inventory_sha256": "c" * 64,
                "compiler_version": "13.3.0", "node_version": "v22.14.0"},
            "beta08-transport-evidence.json": {
                "npm_lock_sha256": locksha, "compiler_version": "13.3.0",
                "node_version": "v22.14.0", "profiles": profiles},
            "beta09-security-envelope.json": {
                "npm_lock_sha256": locksha, "apt_full_installed_inventory_sha256": "c" * 64,
                "profiles": profiles, "within_profile_regression_envelope_passed": True,
                "all_required_faults_bounded": True,
                "across_protocol_capacity_equivalence": False,
                "production_sla_or_capacity_approved": False},
            "beta08-build-chain-audit.json": {
                "npm_lock_sha256": locksha, "runtime_and_build_chain_audited": True,
                "scope_enforced_by_invoking_npm": True,
                "unreviewed_high_or_critical": 0, "dependency_exceptions": [],
                "raw_advisories_exported": False},
            "beta09-stage3-gate.json": {
                "beta_0_9_9_old_installed_cpp_auth_browser_verified": True,
                "beta_0_9_10_private_dirfd_regressions_required_in_unit_job": True,
                "beta_0_9_11_hmac_six_component_recovery_verified": True,
                "beta_0_9_12_actual_systemd_cpp_and_node_ci_verified": True,
                "production_deployment_or_rollback_proven": False},
            "beta09-systemd-node.json": {
                "compiled_node_production_only_dependencies": True,
                "actual_systemd_start_restart_stop": True,
                "unprivileged_systemd_service": True, "loopback_only_listener": True,
                "private_owner_only_five_file_state": True,
                "hardened_systemd_unit_properties": True,
                "production_service_changed": False},
        }
        for name, values in records.items():
            (folder / name).write_text(json.dumps({**standard, **values}))
    hardening_dir = hardening / "ubuntu-24.04"
    hardening_dir.mkdir(parents=True)
    (hardening_dir / "tsan-provenance.json").write_text(json.dumps({
        "schema": 1, "source_sha": SHA, "os_image": "ubuntu-24.04",
        "result": "passed", "suite": "beta08-tsan", "tests": 8}))
    (hardening_dir / "beta09-fuzz-envelope.json").write_text(json.dumps({
        "schema": 1, "source_sha": SHA, "os_image": "ubuntu-24.04",
        "both_required_fuzz_checks_passed": True,
        "distinct_fuzzer_profiles": ["adc_parser", "adc_fragments"],
        "required_runs": 2048, "observed_runs": 2048,
        "sanitizer_findings": 0, "raw_inputs_exported": False}))
    return source, reports, hardening


def github_fixture(mode="pr"):
    return {"source_sha": SHA, "mode": mode,
            "repository": "adrianf87/dc24h.eu-high-starter",
            "branch": {"pr": "beta-0.9.x-stage-full",
                       "fix-pass": "beta-0.9.x-stage-full-fix-pass",
                       "main": "main"}[mode],
            "github_api_metadata_verified": True,
            "artifact_sha256_independently_verified": True,
            "downloaded_artifact_count": 19,
            "raw_credentials_exported": False,
            "production_deployment_authorized": False,
            "workflow_run_ids": {name: {"id": index, "attempt": 1} for index, name in enumerate((
                "build.yml", "stage4-ubuntu-vm-validation.yml", "beta07-codeql.yml",
                "beta08-hardening.yml", "beta09-stage4-storage.yml"), start=1)},
            "owner_release_approval": False, "merge_authorized": False}


class CandidateTests(unittest.TestCase):
    def test_offline_records_always_remain_no_go(self):
        with tempfile.TemporaryDirectory() as directory:
            checkout, reports, hardening = fixture(Path(directory))
            result = gate.evaluate(checkout, SHA, reports, hardening)
            self.assertEqual(result["status"], "NO-GO")
            self.assertTrue(result["final_github_authenticity_required"])
            self.assertFalse(result["merge_authorized"])
            self.assertEqual(result["locally_consistent_sanitized_reports"], 20)

    def test_wrong_head_missing_audit_tsan_fuzz_and_version_fail(self):
        with tempfile.TemporaryDirectory() as directory:
            checkout, reports, hardening = fixture(Path(directory))
            with self.assertRaises(ValueError):
                gate.evaluate(checkout, "b" * 40, reports, hardening)
            audit = reports / "ubuntu-22.04/beta08-build-chain-audit.json"
            raw = json.loads(audit.read_text())
            raw["scope_enforced_by_invoking_npm"] = False
            audit.write_text(json.dumps(raw))
            with self.assertRaises(ValueError):
                gate.evaluate(checkout, SHA, reports, hardening)
            audit.unlink()
            with self.assertRaises(ValueError):
                gate.evaluate(checkout, SHA, reports, hardening)

    def test_github_zip_digest_and_owner_decision_stay_separate(self):
        with tempfile.TemporaryDirectory() as directory:
            checkout, reports, hardening = fixture(Path(directory))
            metadata = github_fixture()
            result = gate.evaluate(checkout, SHA, reports, hardening, metadata)
            self.assertTrue(result["independently_hashed_artifact_evidence_present"])
            self.assertFalse(result["owner_release_approval"])
            metadata["artifact_sha256_independently_verified"] = False
            with self.assertRaises(ValueError):
                gate.evaluate(checkout, SHA, reports, hardening, metadata)

    def test_fix_pass_evidence_can_be_inspected_but_never_approves_rc(self):
        with tempfile.TemporaryDirectory() as directory:
            checkout, reports, hardening = fixture(Path(directory))
            github = github_fixture("fix-pass")
            result = gate.evaluate(checkout, SHA, reports, hardening, github)
            self.assertEqual(result["status"], "NO-GO")
            self.assertFalse(result["merge_authorized"])
            github["workflow_run_ids"].pop("beta09-stage4-storage.yml")
            with self.assertRaises(ValueError):
                gate.evaluate(checkout, SHA, reports, hardening, github)

    def test_post_merge_main_evidence_is_checked_without_release_approval(self):
        with tempfile.TemporaryDirectory() as directory:
            checkout, reports, hardening = fixture(Path(directory))
            verified_main = github_fixture("main")
            result = gate.evaluate(checkout, SHA, reports, hardening, verified_main)
            self.assertEqual(result["status"], "NO-GO")
            self.assertTrue(result["independently_hashed_artifact_evidence_present"])
            self.assertFalse(result["final_github_authenticity_required"])
            self.assertFalse(result["owner_release_approval"])
            self.assertFalse(result["merge_authorized"])
            self.assertFalse(result["production_deployment_authorized"])

            for field, wrong_value in (
                ("branch", "beta-0.9.x-stage-full-fix-pass"),
                ("mode", "fix-pass"),
                ("source_sha", "b" * 40),
                ("repository", "adrianf87/dc24h.eu-twilight"),
                ("github_api_metadata_verified", False),
                ("artifact_sha256_independently_verified", False),
                ("owner_release_approval", True),
                ("merge_authorized", True),
                ("production_deployment_authorized", True),
            ):
                forged = github_fixture("main")
                forged[field] = wrong_value
                with self.subTest(field=field), self.assertRaises(ValueError):
                    gate.evaluate(checkout, SHA, reports, hardening, forged)

            incomplete = github_fixture("main")
            incomplete["workflow_run_ids"].pop("beta09-stage4-storage.yml")
            with self.assertRaises(ValueError):
                gate.evaluate(checkout, SHA, reports, hardening, incomplete)

            wrong_attempt = github_fixture("main")
            wrong_attempt["workflow_run_ids"]["build.yml"]["attempt"] = 0
            with self.assertRaises(ValueError):
                gate.evaluate(checkout, SHA, reports, hardening, wrong_attempt)

    def test_malicious_perf_resource_summary_and_unpinned_dependency_are_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            checkout, reports, hardening = fixture(Path(directory))
            envelope = reports / "ubuntu-22.04/beta09-security-envelope.json"
            original = json.loads(envelope.read_text())
            for key, bad in (("min_success_rps", 0), ("max_cpu_percent_one_core", 500),
                             ("rss_growth_kib", 9999999), ("max_observed_p99_ms", float("nan"))):
                changed = json.loads(envelope.read_text())
                changed["profiles"]["adc"][key] = bad
                envelope.write_text(json.dumps(changed))
                with self.subTest(key=key), self.assertRaises(ValueError):
                    gate.evaluate(checkout, SHA, reports, hardening)
                envelope.write_text(json.dumps(original))
            package_path = checkout / "webadmin/package.json"
            package = json.loads(package_path.read_text())
            package["dependencies"]["runtime"] = "^1.0.0"
            package_path.write_text(json.dumps(package))
            with self.assertRaises(ValueError):
                gate.evaluate(checkout, SHA, reports, hardening)


if __name__ == "__main__":
    unittest.main()
