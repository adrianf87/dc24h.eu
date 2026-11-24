"""Offline negative qualification tests: never infer real GitHub CI from fixtures."""

import copy
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import beta07_release_candidate_gate as gate
from beta07_stage1_completion_gate_tests import artificial_complete_manifest, SHA


def source_and_reports(root):
    checkout, reports = root / "checkout", root / "reports"
    for name, label in {
        "CMakeLists.txt": 'set(DC24H_VERSION "beta-0.7.20")',
        "webadmin/src/lib/status.ts": 'WEBADMIN_VERSION = "beta-0.7.20"',
        "README.md": "Current development version: beta-0.7.20",
        "CHANGELOG.md": "beta-0.7.20",
        "doc/us/BETA_0.7_STAGE_V.md": "NO-GO",
        "doc/pl/BETA_0.7_STAGE_V.md": "NO-GO",
    }.items():
        target = checkout / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(label)
    package = {"name": "dc24h-webadmin", "version": "0.7.20"}
    (checkout / "webadmin/package.json").write_text(json.dumps(package))
    (checkout / "webadmin/package-lock.json").write_text(json.dumps({
        **package, "packages": {"": package}}))
    lock_sha = hashlib.sha256((checkout / "webadmin/package-lock.json").read_bytes()).hexdigest()
    for image in gate.stages.IMAGES:
        folder = reports / image
        folder.mkdir(parents=True)
        fixtures = {
            "beta07-supply-chain.json": {"npm_lock_sha256": lock_sha,
                                         "npm_transitive_locked_packages": 1,
                                         "npm_registry_integrity_entries": 1,
                                         "sbom_components": [{"package": "public-pkg", "version": "1.2.3",
                                                              "integrity_sha256": "c" * 64}],
                                         "pinned_actions": ["actions/checkout@" + "b" * 40]},
            "beta07-npm-audit.json": {"production_dependencies_audited": True,
                                      "advisory_totals": {"high": 0, "critical": 0},
                                      "unreviewed_high_or_critical": 0,
                                      "raw_advisories_exported": False},
            "beta07-transport-performance.json": {"profiles": {
                name: {"windows": 3, "samples": 3 * policy["requests"], "failed_requests": 0,
                       "ci_regression_envelope_passed": True, "max_observed_p99_ms": 20,
                       "min_success_rps": policy["min_rps"], "max_cpu_percent_one_core": 20,
                       "rss_growth_kib": 1024}
                for name, policy in gate.performance.POLICY.items()},
                "within_profile_windows_comparable": True,
                "production_capacity_or_sla_claim": False},
            "beta07-bounded-faults.json": {"required_cases": list(gate.faults_module.REQUIRED),
                                          "case_seconds": {name: 2.0 for name in gate.faults_module.REQUIRED},
                                          "case_terminal_bound_seconds": 240.0,
                                          "read_only_aggregate_evidence": True,
                                          "all_required_fault_cases_passed": True,
                                          "production_sla_claim": False},
            "beta07-dpkg-rollback.json": {"actual_dpkg_installed_upgrade_downgrade": True,
                                          "fresh_sql_and_five_store_preflight_succeeded": True,
                                          "production_package_or_rollback_claim": False},
            "stage1-matched-recovery.json": {"matched_restore_completed": True,
                                             "fresh_rollback_completed": True,
                                             "production_sla_claim": False},
            "beta06-integrated-chaos.json": {"ci_regression_gate_passed": True,
                                             "production_claim": False},
        }
        for name, payload in fixtures.items():
            (folder / name).write_text(json.dumps({"schema": 1, "source_sha": SHA,
                                                   "os_image": image, **payload}))
    return checkout, reports


class ReleaseCandidateGateTests(unittest.TestCase):
    def test_synthetic_provenance_never_authorizes_release_or_claims_api_verification(self):
        with tempfile.TemporaryDirectory() as directory:
            checkout, reports = source_and_reports(Path(directory))
            result = gate.evaluate(artificial_complete_manifest(), SHA, reports, checkout)
            self.assertEqual(result["mandatory_redacted_reports_checked"], 21)
            self.assertFalse(result["github_run_authenticity_independently_verified"])
            self.assertFalse(result["merge_authorized"])
            self.assertFalse(result["production_deployment_authorized"])

    def test_missing_wrong_sha_or_production_claimed_evidence_refuses(self):
        with tempfile.TemporaryDirectory() as directory:
            checkout, reports = source_and_reports(Path(directory))
            os_image = gate.stages.IMAGES[0]
            report = reports / os_image / "beta07-npm-audit.json"
            doc = json.loads(report.read_text())
            doc["source_sha"] = "b" * 40
            report.write_text(json.dumps(doc))
            with self.assertRaises(ValueError):
                gate.evaluate(artificial_complete_manifest(), SHA, reports, checkout)
            doc["source_sha"] = SHA
            doc["unreviewed_high_or_critical"] = 1
            report.write_text(json.dumps(doc))
            with self.assertRaises(ValueError):
                gate.evaluate(artificial_complete_manifest(), SHA, reports, checkout)
            report.unlink()
            with self.assertRaises(ValueError):
                gate.evaluate(artificial_complete_manifest(), SHA, reports, checkout)

    def test_pending_or_skipped_job_is_not_overridden_by_good_local_reports(self):
        with tempfile.TemporaryDirectory() as directory:
            checkout, reports = source_and_reports(Path(directory))
            manifest = copy.deepcopy(artificial_complete_manifest())
            manifest["jobs"][0]["status"] = "in_progress"
            with self.assertRaises(ValueError):
                gate.evaluate(manifest, SHA, reports, checkout)

    def test_contradictory_benchmark_fault_and_sbom_data_cannot_claim_release_ready(self):
        with tempfile.TemporaryDirectory() as directory:
            checkout, reports = source_and_reports(Path(directory))
            image = gate.stages.IMAGES[0]
            corruptions = (
                ("beta07-transport-performance.json", lambda doc: doc["profiles"]["adc"].update(samples=1)),
                ("beta07-transport-performance.json", lambda doc: doc["profiles"]["adcs"].update(ci_regression_envelope_passed=False)),
                ("beta07-transport-performance.json", lambda doc: doc["profiles"]["authenticated_https"].update(min_success_rps=float("nan"))),
                ("beta07-bounded-faults.json", lambda doc: doc.update(case_seconds={"adc.login.process": 0})),
                ("beta07-bounded-faults.json", lambda doc: doc["case_seconds"].update({"adc.login.process": 241})),
                ("beta07-supply-chain.json", lambda doc: doc.update(npm_lock_sha256="b" * 64)),
                ("beta07-supply-chain.json", lambda doc: doc["sbom_components"][0].update(integrity_sha256="bad")),
            )
            for name, corrupt in corruptions:
                with self.subTest(name=name, corruption=corrupt):
                    report = reports / image / name
                    original = report.read_text()
                    payload = json.loads(original)
                    corrupt(payload)
                    report.write_text(json.dumps(payload))
                    with self.assertRaises(ValueError):
                        gate.evaluate(artificial_complete_manifest(), SHA, reports, checkout)
                    report.write_text(original)
            package = checkout / "webadmin/package.json"
            package.write_text(json.dumps({"name": "dc24h-webadmin", "version": "0.6.20"}))
            with self.assertRaisesRegex(ValueError, "Unaligned npm"):
                gate.evaluate(artificial_complete_manifest(), SHA, reports, checkout)


if __name__ == "__main__":
    unittest.main()
