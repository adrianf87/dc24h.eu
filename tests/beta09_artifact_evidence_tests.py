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
import twilight_stage6_preflight as stage6
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
                              "failed_requests": 0, "ci_regression_envelope_passed": True,
                              "max_observed_p99_ms": 12.0, "min_success_rps": 8.0,
                              "max_cpu_percent_one_core": 10.0, "rss_growth_kib": 1024.0}
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
    branch = (gate.BRANCH if mode == "pr" else gate.FIX_BRANCH if mode == "fix-pass"
              else gate.TWILIGHT_BRANCH if mode == "twilight-pr" else "main")
    repository = gate.prior.TWILIGHT_REPO if mode == "twilight-pr" else gate.prior.REPO
    event = "pull_request" if mode in ("pr", "twilight-pr") else "push"
    data = previous_fixture(branch, event, repository=repository)
    pr_number = 17 if mode == "twilight-pr" else gate.PR
    data[f"/pulls/{pr_number}"] = {
        "state": "open", "head": {"ref": branch, "sha": SHA,
                                  "repo": {"full_name": repository}},
        "base": {"ref": "main", "repo": {"full_name": repository}}}
    runs = data[f"/actions/runs?head_sha={SHA}&per_page=100"]["workflow_runs"]
    archives = {}
    for run in runs:
        run["head_branch"] = branch
        run["event"] = event
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
    def test_twilight_stage_vi_preflight_artifact_cannot_claim_release_go(self):
        name = "twilight-stage6-preflight.json"
        self.assertIn(name, gate.COMMON)
        report = stage6.evaluate(Path(__file__).resolve().parents[1], SHA, "ubuntu-24.04")
        gate._report_semantics(name, report)
        for field, forged_value in (
                ("status", "GO"), ("owner_release_acceptance_recorded", True),
                ("complete_final_sha_github_ci_verified", True),
                ("passkey_origin_rpid_guards_present", False),
                ("github_workflows_checked", 4)):
            with self.subTest(field=field):
                tampered = {**report, field: forged_value}
                with self.assertRaises(ValueError):
                    gate._report_semantics(name, tampered)

    def test_twilight_pr_zip_evidence_is_bound_to_pr_branch_and_repository(self):
        data, archives = fixture("twilight-pr")
        check = lambda records: gate.evaluate(lambda key: records[key],
                                               lambda i: archives[i], SHA,
                                               "twilight-pr", repository=gate.prior.TWILIGHT_REPO,
                                               branch=gate.TWILIGHT_BRANCH, pr_number=17)
        result = check(data)
        self.assertEqual(result["repository"], gate.prior.TWILIGHT_REPO)
        self.assertEqual(result["branch"], gate.TWILIGHT_BRANCH)
        self.assertGreaterEqual(result["downloaded_artifact_count"], 12)
        self.assertFalse(result["owner_release_approval"])
        for field, value in (("sha", "b" * 40), ("ref", "rebrand/unreviewed"),
                             ("repo", {"full_name": gate.prior.REPO})):
            forged = copy.deepcopy(data)
            forged["/pulls/17"]["head"][field] = value
            with self.subTest(field=field), self.assertRaisesRegex(ValueError, "Pull request"):
                check(forged)
        forged = copy.deepcopy(data)
        forged["/pulls/17"]["base"]["repo"]["full_name"] = gate.prior.REPO
        with self.assertRaisesRegex(ValueError, "Pull request"):
            check(forged)
        foreign = copy.deepcopy(data)
        foreign[f"/actions/runs?head_sha={SHA}&per_page=100"]["workflow_runs"][0][
            "repository"]["full_name"] = gate.prior.REPO
        with self.assertRaisesRegex(ValueError, "wrong-repository"):
            check(foreign)
        with self.assertRaises(ValueError):
            gate.evaluate(lambda key: data[key], lambda i: archives[i], SHA,
                          "twilight-pr", repository=gate.prior.REPO,
                          branch=gate.TWILIGHT_BRANCH, pr_number=17)
        wrong_number = copy.deepcopy(data)
        wrong_number["/pulls/18"] = {**wrong_number["/pulls/17"], "state": "closed"}
        with self.assertRaises(ValueError):
            gate.evaluate(lambda key: wrong_number[key], lambda i: archives[i], SHA,
                          "twilight-pr", repository=gate.prior.TWILIGHT_REPO,
                          branch=gate.TWILIGHT_BRANCH, pr_number=18)

    def test_fix_pass_push_is_verified_without_fake_pr(self):
        data, archives = fixture("fix-pass")
        data.pop(f"/pulls/{gate.PR}")
        result = gate.evaluate(lambda key: data[key], lambda i: archives[i], SHA, "fix-pass")
        self.assertEqual(result["branch"], gate.FIX_BRANCH)
        self.assertIn("beta09-stage4-storage.yml", result["workflow_run_ids"])
        self.assertFalse(result["merge_authorized"])

    def test_performance_envelope_rejects_low_rps_or_unbounded_rss(self):
        name = "beta09-security-envelope.json"
        report = safe_report(name)
        for profile in ("adc", "adcs", "authenticated_https"):
            changed = copy.deepcopy(report)
            changed["profiles"][profile]["min_success_rps"] = 0.0
            with self.subTest(profile=profile), self.assertRaises(ValueError):
                gate._report_semantics(name, changed)
        report["profiles"]["adcs"]["rss_growth_kib"] = 9999999
        with self.assertRaises(ValueError):
            gate._report_semantics(name, report)

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

    def test_optional_new_stage4_reports_are_validated_without_rewriting_rc1_history(self):
        for name, extra in (
                ("twilight-stage4-name-migration.json", {
                    "legacy_package": "dc24h-high-starter-ci-fixture",
                    "twilight_package": "dc24h-twilight-ci-fixture",
                    "legacy_hub_sha256": "c" * 64,
                    "twilight_hub_sha256": "d" * 64,
                    "production_deployment_or_migration_claim": False,
                    **{field: True for field in (
                        "actual_two_distinct_package_names", "clean_old_name_install",
                        "new_name_refused_without_explicit_old_purge",
                        "failed_new_install_refused_and_old_name_restored",
                        "explicit_old_to_new_migration", "legacy_install_refused_while_twilight_installed",
                        "actual_new_to_old_name_rollback", "binary_and_account_admin_hashes_verified",
                        "inert_package_units_not_enabled_or_active", "systemd_owner_marker_required",
                        "systemd_conflict_stopped_old_without_parallel_activation",
                        "synthetic_systemd_rollback_restored_old")}}),
                ("twilight-stage4-migration-gate.json", {
                    "two_distinct_package_names_transition_verified": True,
                    "negative_and_failed_transition_refused_without_silent_replacement": True,
                    "actual_old_new_old_package_and_unit_rehearsal_verified": True,
                    "github_hosted_ci_only": True,
                    "production_upgrade_or_migration_authorized": False})):
            with self.subTest(name=name):
                report = {"schema": 1, "source_sha": SHA, **extra}
                for altered in (False, True):
                    candidate = copy.deepcopy(report)
                    if altered:
                        candidate[next(key for key in extra if type(extra[key]) is bool and extra[key] is True)] = False
                    mandatory = gate._file_policy("stage4-ubuntu-vm-ubuntu-24.04-901-1")[1]
                    reports = {file: safe_report(file) for file in mandatory}
                    reports[name] = candidate
                    archive = zipped(reports)
                    metadata = {"name": "stage4-ubuntu-vm-ubuntu-24.04-901-1",
                                "size_in_bytes": len(archive),
                                "digest": "sha256:" + hashlib.sha256(archive).hexdigest()}
                    if altered:
                        with self.assertRaises(ValueError):
                            gate.inspect_archive(archive, metadata, SHA)
                    else:
                        self.assertEqual(gate.inspect_archive(archive, metadata, SHA)["reports"], len(reports))


if __name__ == "__main__":
    unittest.main()
