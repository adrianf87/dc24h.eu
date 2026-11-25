#!/usr/bin/env python3
"""Beta-0.9.20 offline candidate consistency, never autonomous Owner approval.

Requires separately downloaded, sanitized, exact-head GitHub-hosted evidence;
no fabricated local JSON can authorize merge, publication or deployment.
"""

import argparse
import json
from pathlib import Path
import re
import subprocess

import beta07_ubuntu_perf_matrix as matrix
import beta08_build_chain_audit as build_audit
import beta09_os_provenance as host


SHA = re.compile(r"[0-9a-f]{40}\Z")
IMAGES = host.IMAGES
PER_IMAGE = ("beta09-os-provenance.json", "beta09-security-envelope.json",
             "beta08-build-chain-audit.json", "beta08-transport-evidence.json",
             "beta09-stage3-gate.json", "beta09-systemd-node.json")


def _read(base, name, sha, image):
    path = base / name
    if not path.is_file() or not 0 < path.stat().st_size <= 256 * 1024:
        raise ValueError("Missing or oversized candidate report")
    report = json.loads(path.read_text(encoding="utf8"))
    if (not isinstance(report, dict) or report.get("schema") != 1 or
            report.get("source_sha") != sha or report.get("os_image") != image):
        raise ValueError("Wrong-head or wrong-host candidate report")
    return report


def _metadata(root):
    required = {
        "CMakeLists.txt": ('project(dc24h VERSION 0.9.20 LANGUAGES CXX)',
                           'set(DC24H_VERSION "beta-0.9.20")'),
        "webadmin/src/lib/status.ts": ('WEBADMIN_VERSION = "beta-0.9.20"',),
        "webadmin/tests/remote-incident-journal.test.ts": ('version: "beta-0.9.20"',),
        "README.md": ('Current development version: beta-0.9.20',),
        "doc/README.md": ('Current development version: beta-0.9.20',),
        "webadmin/README.md": ('beta-0.9.20',),
        "CHANGELOG.md": ('beta-0.9.17–beta-0.9.20',),
        "doc/us/BETA_0.9_STAGE_V.md": ('NO-GO',),
        "doc/pl/BETA_0.9_STAGE_V.md": ('NO-GO',),
    }
    for filename, phrases in required.items():
        text = (root / filename).read_text(encoding="utf8")
        if any(phrase not in text for phrase in phrases):
            raise ValueError("Incomplete beta-0.9.20 candidate metadata or risk register")
    packagebytes = (root / "webadmin/package.json").read_bytes()
    package = json.loads(packagebytes)
    lockbytes = (root / "webadmin/package-lock.json").read_bytes()
    lock = json.loads(lockbytes)
    if (package.get("version") != "0.9.20" or lock.get("version") != "0.9.20" or
            lock.get("packages", {}).get("", {}).get("version") != "0.9.20" or
            lock.get("name") != package.get("name") or
            lock.get("packages", {}).get("", {}).get("name") != package.get("name")):
        raise ValueError("C++/Node/npm candidate identity is not aligned")
    return build_audit.locked_manifest(packagebytes, lockbytes)


def evaluate(root, sha, reports, hardening, github=None):
    if not isinstance(sha, str) or not SHA.fullmatch(sha) or sha == "0" * 40:
        raise ValueError("Invalid final source head")
    locksha = _metadata(root)
    for image in IMAGES:
        files = {name: _read(reports / image, name, sha, image) for name in PER_IMAGE}
        os_report = files["beta09-os-provenance.json"]
        transport = files["beta08-transport-evidence.json"]
        envelope = files["beta09-security-envelope.json"]
        audit = files["beta08-build-chain-audit.json"]
        stage3 = files["beta09-stage3-gate.json"]
        systemd = files["beta09-systemd-node.json"]
        if (not all(stage3.get(field) is True for field in (
                "beta_0_9_9_old_installed_cpp_auth_browser_verified",
                "beta_0_9_10_private_dirfd_regressions_required_in_unit_job",
                "beta_0_9_11_hmac_six_component_recovery_verified",
                "beta_0_9_12_actual_systemd_cpp_and_node_ci_verified")) or
                stage3.get("production_deployment_or_rollback_proven") is not False or
                not all(systemd.get(field) is True for field in (
                "compiled_node_production_only_dependencies", "actual_systemd_start_restart_stop",
                "unprivileged_systemd_service", "loopback_only_listener",
                "private_owner_only_five_file_state", "hardened_systemd_unit_properties")) or
                systemd.get("production_service_changed") is not False):
            raise ValueError("Missing installed historical-login and isolated systemd proof")
        if (os_report.get("npm_lock_sha256") != locksha or
                os_report.get("runner_environment") != "github-hosted" or
                os_report.get("fully_reproducible_os_build") is not False or
                os_report.get("apt_repository_snapshot_immutable") is not False or
                not isinstance(os_report.get("selected_public_apt_versions"), dict) or
                set(os_report["selected_public_apt_versions"]) != set(host.REQUIRED_APT) or
                not isinstance(os_report.get("apt_full_installed_inventory_sha256"), str) or
                not re.fullmatch(r"[0-9a-f]{64}", os_report["apt_full_installed_inventory_sha256"])):
            raise ValueError("Missing hosted Ubuntu apt/build-chain inventory")
        if (audit.get("npm_lock_sha256") != locksha or
                audit.get("runtime_and_build_chain_audited") is not True or
                audit.get("scope_enforced_by_invoking_npm") is not True or
                type(audit.get("unreviewed_high_or_critical")) is not int or
                audit["unreviewed_high_or_critical"] != 0 or
                audit.get("dependency_exceptions") != [] or
                audit.get("raw_advisories_exported") is not False):
            raise ValueError("Excluded/unreviewed full npm supply-chain dependencies")
        if (transport.get("npm_lock_sha256") != locksha or
                transport.get("compiler_version") != os_report.get("compiler_version") or
                transport.get("node_version") != os_report.get("node_version") or
                envelope.get("npm_lock_sha256") != locksha or
                envelope.get("apt_full_installed_inventory_sha256") != os_report.get("apt_full_installed_inventory_sha256") or
                envelope.get("profiles") != transport.get("profiles") or
                envelope.get("within_profile_regression_envelope_passed") is not True or
                envelope.get("all_required_faults_bounded") is not True or
                envelope.get("across_protocol_capacity_equivalence") is not False or
                envelope.get("production_sla_or_capacity_approved") is not False):
            raise ValueError("Unpinned or fabricated three-profile performance/drain evidence")
        for profile, policy in matrix.POLICY.items():
            matrix.validate_profile_summary(profile, envelope["profiles"].get(profile))
    tsan = _read(hardening / "ubuntu-24.04", "tsan-provenance.json", sha, "ubuntu-24.04")
    fuzz = _read(hardening / "ubuntu-24.04", "beta09-fuzz-envelope.json", sha, "ubuntu-24.04")
    if (tsan.get("result") != "passed" or tsan.get("suite") != "beta08-tsan" or
            type(tsan.get("tests")) is not int or tsan["tests"] < 8 or
            fuzz.get("both_required_fuzz_checks_passed") is not True or
            fuzz.get("distinct_fuzzer_profiles") != ["adc_parser", "adc_fragments"] or
            fuzz.get("required_runs") != 2048 or
            type(fuzz.get("observed_runs")) is not int or fuzz["observed_runs"] < 2048 or
            fuzz.get("sanitizer_findings") != 0 or
            fuzz.get("raw_inputs_exported") is not False):
        raise ValueError("TSAN, two independent fuzz targets or sanitizer evidence missing")
    independently_hashed = False
    if github is not None:
        expected_workflows = set(("build.yml", "stage4-ubuntu-vm-validation.yml",
                                  "beta07-codeql.yml", "beta08-hardening.yml",
                                  "beta09-stage4-storage.yml"))
        runs = github.get("workflow_run_ids") if isinstance(github, dict) else None
        if (not isinstance(github, dict) or github.get("source_sha") != sha or
                # This RC1 gate evaluates the historical High Starter lineage,
                # never artifacts originating from a different GitHub repo.
                github.get("repository") != "adrianf87/dc24h.eu-high-starter" or
                (github.get("mode"), github.get("branch")) not in (
                    ("pr", "beta-0.9.x-stage-full"),
                    ("fix-pass", "beta-0.9.x-stage-full-fix-pass"),
                    ("main", "main")) or
                github.get("github_api_metadata_verified") is not True or
                github.get("artifact_sha256_independently_verified") is not True or
                type(github.get("downloaded_artifact_count")) is not int or
                github["downloaded_artifact_count"] < 12 or
                github.get("raw_credentials_exported") is not False or
                github.get("production_deployment_authorized") is not False or
                not isinstance(runs, dict) or set(runs) != expected_workflows or
                any(not isinstance(value, dict) or
                    type(value.get("id")) is not int or value["id"] <= 0 or
                    type(value.get("attempt")) is not int or value["attempt"] <= 0
                    for value in runs.values()) or
                len({value["id"] for value in runs.values()}) != len(expected_workflows) or
                github.get("owner_release_approval") is not False or
                github.get("merge_authorized") is not False):
            raise ValueError("Unverified final-head GitHub ZIP-byte/PR evidence")
        independently_hashed = True
    return {"schema": 1, "source_sha": sha,
            "ubuntu_images": list(IMAGES),
            "locally_consistent_sanitized_reports": len(IMAGES) * len(PER_IMAGE) + 2,
            "offline_candidate_structure_qualified": True,
            "independently_hashed_artifact_evidence_present": independently_hashed,
            "final_github_authenticity_required": not independently_hashed,
            "physical_devices_and_native_second_client_certified": False,
            "production_rpo_rto_or_capacity_certified": False,
            "owner_release_approval": False, "merge_authorized": False,
            "production_deployment_authorized": False, "status": "NO-GO"}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--reports", type=Path, required=True)
    parser.add_argument("--hardening", type=Path, required=True)
    parser.add_argument("--github", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        actual = subprocess.check_output(["git", "-C", str(args.root), "rev-parse", "HEAD"],
                                         timeout=10).decode().strip()
        report = evaluate(args.root, actual, args.reports, args.hardening,
                          json.loads(args.github.read_text()) if args.github else None)
        with args.output.open("x", encoding="utf8") as stream:
            json.dump(report, stream, sort_keys=True)
            stream.write("\n")
    except (OSError, ValueError, TypeError, KeyError, UnicodeError,
            subprocess.SubprocessError, json.JSONDecodeError):
        parser.exit(1, "Beta-0.9.20 candidate/CI evidence incomplete; NO-GO.\n")
    print("Candidate offline evidence consistent; explicit Owner acceptance and CI still separate")


if __name__ == "__main__":
    raise SystemExit(main())
