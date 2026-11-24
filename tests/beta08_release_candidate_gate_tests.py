"""Synthetic NO-GO release fixtures; never claim a real hosted CI result."""

import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import beta08_release_candidate_gate as gate


SHA = "a" * 40


def fixture(root):
    checkout, reports, hardening = root / "checkout", root / "reports", root / "hardening"
    fragments = {
        "CMakeLists.txt": 'project(dc24h VERSION 0.8.20\nset(DC24H_VERSION "beta-0.8.20")',
        "webadmin/src/lib/status.ts": 'WEBADMIN_VERSION = "beta-0.8.20"',
        "README.md": "Current development version: beta-0.8.20",
        "CHANGELOG.md": "beta-0.8.17–beta-0.8.20",
        "doc/us/BETA_0.8_STAGE_V.md": "NO-GO",
        "doc/pl/BETA_0.8_STAGE_V.md": "NO-GO",
    }
    for path, content in fragments.items():
        target = checkout / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(content)
    package = {"name": "dc24h-webadmin", "version": "0.8.20"}
    (checkout / "webadmin/package.json").write_text(json.dumps(package))
    (checkout / "webadmin/package-lock.json").write_text(json.dumps({
        **package, "packages": {"": package}}))
    locksha = hashlib.sha256((checkout / "webadmin/package-lock.json").read_bytes()).hexdigest()
    profiles = {key: {"windows": 3, "samples": 3 * value["requests"], "failed_requests": 0,
                      "ci_regression_envelope_passed": True, "max_observed_p99_ms": 10}
                for key, value in gate.matrix.POLICY.items()}
    for image in gate.IMAGES:
        folder = reports / image
        folder.mkdir(parents=True)
        source = {"schema": 1, "source_sha": SHA, "os_image": image}
        values = {
            "beta07-supply-chain.json": {"npm_lock_sha256": locksha, "pinned_actions": ["pinned"]},
            "beta07-transport-performance.json": {"profiles": profiles},
            "beta08-transport-evidence.json": {
                "profiles": profiles, "npm_lock_sha256": locksha,
                "all_three_profiles_recomputed": True,
                "cross_protocol_throughput_equivalent": False,
                "production_sla_or_capacity_claim": False},
            "beta08-build-chain-audit.json": {
                "npm_lock_sha256": locksha, "runtime_and_build_chain_audited": True,
                "unreviewed_high_or_critical": 0, "dependency_exceptions": [],
                "raw_advisories_exported": False},
            "beta07-bounded-faults.json": {
                "all_required_fault_cases_passed": True, "production_sla_claim": False},
            "beta07-dpkg-rollback.json": {"actual_dpkg_installed_upgrade_downgrade": True},
            "stage1-matched-recovery.json": {"matched_restore_completed": True},
            "beta06-integrated-chaos.json": {"ci_regression_gate_passed": True},
        }
        for path, value in values.items():
            (folder / path).write_text(json.dumps({**source, **value}))
    hardening_dir = hardening / "ubuntu-24.04"
    hardening_dir.mkdir(parents=True)
    (hardening_dir / "tsan-provenance.json").write_text(json.dumps({
        "schema": 1, "source_sha": SHA, "os_image": "ubuntu-24.04",
        "result": "passed", "suite": "beta08-tsan", "tests": 3}))
    (hardening_dir / "fuzz-smoke.json").write_text(json.dumps({
        "schema": 1, "source_sha": SHA, "os_image": "ubuntu-24.04",
        "all_required_fuzz_checks_passed": True, "observed_runs": 1024,
        "sanitizer_findings": 0, "raw_traces_uploaded": False}))
    return checkout, reports, hardening


class CandidateGateTests(unittest.TestCase):
    def test_synthetic_records_never_authorize_a_merge(self):
        with tempfile.TemporaryDirectory() as directory:
            checkout, reports, hardening = fixture(Path(directory))
            report = gate.evaluate(checkout, SHA, reports, hardening)
            self.assertTrue(report["authenticated_github_api_verification_still_required"])
            self.assertFalse(report["merge_authorized"])
            self.assertEqual(report["locally_consistent_redacted_reports"], 26)

    def test_wrong_head_and_missing_audit_or_hardening_are_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            checkout, reports, hardening = fixture(Path(directory))
            with self.assertRaises(ValueError):
                gate.evaluate(checkout, "b" * 40, reports, hardening)
            target = reports / "ubuntu-24.04/beta08-build-chain-audit.json"
            data = json.loads(target.read_text())
            data["runtime_and_build_chain_audited"] = False
            target.write_text(json.dumps(data))
            with self.assertRaises(ValueError):
                gate.evaluate(checkout, SHA, reports, hardening)
            target.unlink()
            with self.assertRaises(ValueError):
                gate.evaluate(checkout, SHA, reports, hardening)


if __name__ == "__main__":
    unittest.main()
