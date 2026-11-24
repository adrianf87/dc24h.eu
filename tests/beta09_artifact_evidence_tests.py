"""Synthetic GitHub and ZIP fixtures: never claim actual hosted CI acceptance."""

import copy
import hashlib
import io
import json
from pathlib import Path
import stat
import sys
import unittest
import warnings
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import beta09_artifact_evidence as gate
from beta08_github_evidence_tests import fixture as previous_fixture, SHA


def zipped(files, *, symlink=None):
    out = io.BytesIO()
    with zipfile.ZipFile(out, "w", compression=zipfile.ZIP_DEFLATED) as writer:
        for name, report in files.items():
            payload = json.dumps(report).encode("utf-8") if isinstance(report, dict) else report
            if name == symlink:
                info = zipfile.ZipInfo(name)
                info.external_attr = (stat.S_IFLNK | 0o777) << 16
                writer.writestr(info, payload)
            else:
                writer.writestr(name, payload)
    return out.getvalue()


def safe_report(name):
    report = {"schema": 1, "source_sha": SHA, "redacted": True}
    if name == "beta09-os-provenance.json":
        report.update(runner_environment="github-hosted", fully_reproducible_os_build=False,
                      apt_repository_snapshot_immutable=False,
                      selected_public_apt_versions={name: "1.0" for name in gate.host.REQUIRED_APT},
                      apt_full_installed_inventory_sha256="c" * 64,
                      npm_lock_sha256="d" * 64)
    if name == "beta08-build-chain-audit.json":
        report.update(runtime_and_build_chain_audited=True,
                      scope_enforced_by_invoking_npm=True, unreviewed_high_or_critical=0,
                      dependency_exceptions=[], raw_advisories_exported=False)
    if name == "beta09-security-envelope.json":
        profiles = {profile: {"windows": 3, "samples": 3 * policy["requests"],
                              "failed_requests": 0, "ci_regression_envelope_passed": True}
                    for profile, policy in gate.perf.POLICY.items()}
        report.update(profiles=profiles, within_profile_regression_envelope_passed=True,
                      all_required_faults_bounded=True,
                      across_protocol_capacity_equivalence=False,
                      production_sla_or_capacity_approved=False)
    if name == "beta09-fuzz-envelope.json":
        report.update(both_required_fuzz_checks_passed=True,
                      distinct_fuzzer_profiles=["adc_parser", "adc_fragments"],
                      required_runs=2048, observed_runs=2048, sanitizer_findings=0,
                      raw_inputs_exported=False)
    if name == "tsan-provenance.json":
        report.update(result="passed", suite="beta08-tsan", tests=8)
    if name == "beta09-stage3-gate.json":
        report.update(beta_0_9_9_old_installed_cpp_auth_browser_verified=True,
                      beta_0_9_10_private_dirfd_regressions_required_in_unit_job=True,
                      beta_0_9_11_hmac_six_component_recovery_verified=True,
                      beta_0_9_12_actual_systemd_cpp_and_node_ci_verified=True,
                      production_deployment_or_rollback_proven=False)
    if name == "beta09-systemd-node.json":
        report.update(compiled_node_production_only_dependencies=True,
                      actual_systemd_start_restart_stop=True, unprivileged_systemd_service=True,
                      loopback_only_listener=True, private_owner_only_five_file_state=True,
                      hardened_systemd_unit_properties=True, production_service_changed=False)
    return report


def fixture(mode="pr"):
    data = copy.deepcopy(previous_fixture())
    branch = gate.BRANCH if mode == "pr" else "main"
    data.pop("/branches/beta-0.8.x-stage-full")
    data[f"/branches/{branch}"] = {"name": branch, "commit": {"sha": SHA}}
    data[f"/pulls/{gate.PR}"] = {
        "state": "open", "head": {"ref": gate.BRANCH, "sha": SHA,
                                  "repo": {"full_name": gate.prior.REPO}},
        "base": {"ref": "main", "repo": {"full_name": gate.prior.REPO}}}
    runs = data[f"/actions/runs?head_sha={SHA}&per_page=100"]["workflow_runs"]
    archives = {}
    for run in runs:
        run["head_branch"] = branch
        run["event"] = "pull_request" if mode == "pr" else "push"
        artifacts = data[f"/actions/runs/{run['id']}/artifacts?per_page=100"]["artifacts"]
        for artifact in artifacts:
            artifact["workflow_run"]["head_branch"] = branch
            _allow, mandatory = gate._file_policy(artifact["name"])
            members = {name: safe_report(name) for name in mandatory}
            archive = zipped(members)
            artifact["digest"] = "sha256:" + hashlib.sha256(archive).hexdigest()
            artifact["size_in_bytes"] = len(archive)
            archives[artifact["id"]] = archive
    return data, archives


class IndependentArtifactTests(unittest.TestCase):
    def test_all_synthetic_artifacts_are_checked_without_owner_authorization(self):
        data, archives = fixture()
        outcome = gate.evaluate(lambda key: data[key], lambda i: archives[i], SHA)
        self.assertTrue(outcome["artifact_sha256_independently_verified"])
        self.assertGreaterEqual(outcome["downloaded_artifact_count"], 12)
        self.assertEqual(outcome["legacy_reports_without_embedded_sha"], 0)
        self.assertFalse(outcome["owner_release_approval"])
        self.assertFalse(outcome["merge_authorized"])

    def test_wrong_pr_head_and_stale_main_are_rejected(self):
        data, archives = fixture()
        data[f"/pulls/{gate.PR}"]["head"]["sha"] = "b" * 40
        with self.assertRaisesRegex(ValueError, "Pull request"):
            gate.evaluate(lambda key: data[key], lambda i: archives[i], SHA)
        data, archives = fixture("main")
        outcome = gate.evaluate(lambda key: data[key], lambda i: archives[i], SHA, "main")
        self.assertEqual(outcome["mode"], "main")
        data["/branches/main"]["commit"]["sha"] = "c" * 40
        with self.assertRaises(ValueError):
            gate.evaluate(lambda key: data[key], lambda i: archives[i], SHA, "main")

    def test_wrong_digest_and_missing_archive_fail_closed(self):
        data, archives = fixture()
        artifact = data["/actions/runs/701/artifacts?per_page=100"]["artifacts"][0]
        archives[artifact["id"]] += b"changed"
        with self.assertRaises(ValueError):
            gate.evaluate(lambda key: data[key], lambda i: archives[i], SHA)
        data, archives = fixture()
        with self.assertRaises((KeyError, ValueError)):
            gate.evaluate(lambda key: data[key], lambda i: None, SHA)

    def test_zip_slip_symlink_raw_secret_wrong_head_missing_and_bomb(self):
        base = {"schema": 1, "source_sha": SHA}
        for name, contents, symlink in (
            ("../summary.json", base, None),
            ("summary.json", base, "summary.json"),
            ("summary.json", {**base, "password": "secret"}, None),
            ("summary.json", {**base, "nan_like": 1e999}, None),
            ("summary.json", {"schema": 1, "source_sha": "b" * 40}, None),
            ("other.json", base, None),
            ("summary.json", {**base, "very_large": "x" * (gate.MAX_ENTRY + 1)}, None),
        ):
            with self.subTest(name=name, symlink=symlink, contents=tuple(contents)):
                archive = zipped({name: contents}, symlink=symlink)
                artifact = {"name": "sanitizer-address-101-1", "size_in_bytes": len(archive),
                            "digest": "sha256:" + hashlib.sha256(archive).hexdigest()}
                with self.assertRaises(ValueError):
                    gate.inspect_archive(archive, artifact, SHA)

    def test_duplicate_name_and_legacy_unbound_report_are_explicit(self):
        result = zipped({"summary.json": {"schema": 1, "source_sha": SHA}})
        metadata = {"name": "sanitizer-address-101-1", "size_in_bytes": len(result),
                    "digest": "sha256:" + hashlib.sha256(result).hexdigest()}
        self.assertEqual(gate.inspect_archive(result, metadata, SHA)["reports"], 1)
        zip_buffer = io.BytesIO()
        with warnings.catch_warnings():
            warnings.simplefilter("ignore", UserWarning)
            with zipfile.ZipFile(zip_buffer, "w") as writer:
                for _ in range(2):
                    writer.writestr("summary.json", json.dumps({"schema": 1, "source_sha": SHA}))
        result = zip_buffer.getvalue()
        metadata["size_in_bytes"] = len(result)
        metadata["digest"] = "sha256:" + hashlib.sha256(result).hexdigest()
        with self.assertRaises(ValueError):
            gate.inspect_archive(result, metadata, SHA)

    def test_stage3_service_and_installed_binary_reports_are_required_and_verified(self):
        mandatory = gate._file_policy("stage4-ubuntu-vm-ubuntu-24.04-901-1")[1]
        self.assertIn("beta09-stage3-gate.json", mandatory)
        self.assertIn("beta09-systemd-node.json", mandatory)
        reports = {name: safe_report(name) for name in mandatory}
        for tamper, key in (("beta09-stage3-gate.json", "beta_0_9_9_old_installed_cpp_auth_browser_verified"),
                            ("beta09-systemd-node.json", "actual_systemd_start_restart_stop")):
            with self.subTest(tamper=tamper):
                changed = copy.deepcopy(reports)
                changed[tamper][key] = False
                archive = zipped(changed)
                artifact = {"name": "stage4-ubuntu-vm-ubuntu-24.04-901-1",
                            "size_in_bytes": len(archive),
                            "digest": "sha256:" + hashlib.sha256(archive).hexdigest()}
                with self.assertRaises(ValueError):
                    gate.inspect_archive(archive, artifact, SHA)


if __name__ == "__main__":
    unittest.main()
