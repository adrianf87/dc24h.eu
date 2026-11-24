#!/usr/bin/env python3
"""Offline final-head Stage V evidence consistency gate; NEVER merges/releases.

The job manifest must be assembled from GitHub Actions records for the same
head. This local checker validates shape and report provenance, but cannot
authenticate the remote GitHub records by itself. Owner must separately verify
their origin before any acceptance; a self-written manifest grants no rights.
"""

import argparse
import hashlib
import json
import math
import re
import subprocess
from pathlib import Path

import beta07_bounded_fault_gate as faults_module
import beta07_stage1_completion_gate as stages
import beta07_supply_chain as supply_chain
import beta07_ubuntu_perf_matrix as performance


REPORTS = {
    "beta07-supply-chain.json": ("npm_lock_sha256", "sbom_components", "pinned_actions"),
    "beta07-npm-audit.json": ("production_dependencies_audited", "advisory_totals"),
    "beta07-transport-performance.json": ("profiles", "within_profile_windows_comparable"),
    "beta07-bounded-faults.json": ("required_cases", "all_required_fault_cases_passed"),
    "beta07-dpkg-rollback.json": ("actual_dpkg_installed_upgrade_downgrade",
                                "fresh_sql_and_five_store_preflight_succeeded"),
    "stage1-matched-recovery.json": ("matched_restore_completed", "fresh_rollback_completed"),
    "beta06-integrated-chaos.json": ("ci_regression_gate_passed", "production_claim"),
}


def _report(root, image, name, sha):
    path = root / image / name
    if not path.is_file() or not 0 < path.stat().st_size <= 262144:
        raise ValueError("Missing/oversized final-head report")
    value = json.loads(path.read_text(encoding="utf8"))
    if not isinstance(value, dict) or value.get("schema") != 1 or value.get("source_sha") != sha:
        raise ValueError("Wrong-head or malformed final-head report")
    if name != "stage1-matched-recovery.json" and value.get("os_image") != image:
        raise ValueError("Wrong-image final-head report")
    if any(key not in value for key in REPORTS[name]):
        raise ValueError("Missing final-head report evidence")
    return value


def bounded_number(value, lower, upper):
    return type(value) in (int, float) and math.isfinite(value) and lower <= value <= upper


def evaluate(manifest, sha, reports_directory, checkout):
    verified = stages.evaluate(manifest, sha)
    expected = {
        "CMakeLists.txt": 'set(DC24H_VERSION "beta-0.7.20")',
        "webadmin/src/lib/status.ts": 'WEBADMIN_VERSION = "beta-0.7.20"',
        "README.md": "Current development version: beta-0.7.20",
        "CHANGELOG.md": "beta-0.7.20",
        "doc/us/BETA_0.7_STAGE_V.md": "NO-GO",
        "doc/pl/BETA_0.7_STAGE_V.md": "NO-GO",
    }
    for name, label in expected.items():
        if label not in (checkout / name).read_text(encoding="utf8"):
            raise ValueError("Unaligned final-candidate metadata or risk document")
    version = json.loads((checkout / "webadmin/package.json").read_text(encoding="utf8"))
    lock = json.loads((checkout / "webadmin/package-lock.json").read_text(encoding="utf8"))
    if (version.get("version") != "0.7.20" or lock.get("version") != "0.7.20" or
            lock.get("packages", {}).get("", {}).get("version") != "0.7.20" or
            version.get("name") != lock.get("name") or
            version.get("name") != lock.get("packages", {}).get("", {}).get("name")):
        raise ValueError("Unaligned npm release-candidate metadata")
    lock_sha = hashlib.sha256((checkout / "webadmin/package-lock.json").read_bytes()).hexdigest()
    for image in stages.IMAGES:
        reports = {name: _report(reports_directory, image, name, sha) for name in REPORTS}
        supply = reports["beta07-supply-chain.json"]
        if (not isinstance(supply["sbom_components"], list) or not supply["sbom_components"] or
                supply["npm_lock_sha256"] != lock_sha or
                supply.get("npm_transitive_locked_packages") != len(supply["sbom_components"]) or
                supply.get("npm_registry_integrity_entries") != len(supply["sbom_components"]) or
                any(not isinstance(row, dict) or set(row) != {"package", "version", "integrity_sha256"} or
                    not isinstance(row["package"], str) or not row["package"] or
                    not supply_chain.SEMVER.fullmatch(str(row["version"])) or
                    not re.fullmatch(r"[0-9a-f]{64}", str(row["integrity_sha256"]))
                    for row in supply["sbom_components"]) or
                not isinstance(supply["pinned_actions"], list) or not supply["pinned_actions"] or
                any(not isinstance(pin, str) or not supply_chain.PIN.fullmatch(pin)
                    for pin in supply["pinned_actions"])):
            raise ValueError("Invalid npm/action supply-chain provenance")
        audit = reports["beta07-npm-audit.json"]
        if (audit["production_dependencies_audited"] is not True or
                audit.get("unreviewed_high_or_critical") != 0 or
                audit.get("raw_advisories_exported") is not False):
            raise ValueError("Production dependency advisory gate missing")
        metrics = reports["beta07-transport-performance.json"]
        if (metrics["within_profile_windows_comparable"] is not True or
                not isinstance(metrics["profiles"], dict) or
                set(metrics["profiles"]) != {"adc", "adcs", "authenticated_https"} or
                metrics.get("production_capacity_or_sla_claim") is not False):
            raise ValueError("Incomparable or missing three-transport windows")
        for profile, policy in performance.POLICY.items():
            observed = metrics["profiles"][profile]
            if (not isinstance(observed, dict) or observed.get("windows") != 3 or
                    observed.get("samples") != 3 * policy["requests"] or
                    observed.get("failed_requests") != 0 or
                    observed.get("ci_regression_envelope_passed") is not True or
                    not bounded_number(observed.get("max_observed_p99_ms"), 0, policy["p99_ceiling_ms"]) or
                    not bounded_number(observed.get("min_success_rps"), policy["min_rps"], 10000) or
                    not bounded_number(observed.get("max_cpu_percent_one_core"), 0,
                                       policy["max_cpu_percent_one_core"]) or
                    not bounded_number(observed.get("rss_growth_kib"), 0, policy["max_rss_growth_kib"])):
                raise ValueError("Missing or impossible numeric transport benchmark evidence")
        faults = reports["beta07-bounded-faults.json"]
        if (faults["all_required_fault_cases_passed"] is not True or
                faults.get("production_sla_claim") is not False or
                faults.get("read_only_aggregate_evidence") is not True or
                faults.get("required_cases") != list(faults_module.REQUIRED) or
                faults.get("case_terminal_bound_seconds") != faults_module.MAX_CASE_SECONDS or
                not isinstance(faults.get("case_seconds"), dict) or
                set(faults["case_seconds"]) != set(faults_module.REQUIRED) or
                any(not bounded_number(value, 0, faults_module.MAX_CASE_SECONDS)
                    for value in faults["case_seconds"].values())):
            raise ValueError("Incomplete bounded exhaustion evidence")
        dpkg = reports["beta07-dpkg-rollback.json"]
        if (dpkg["actual_dpkg_installed_upgrade_downgrade"] is not True or
                dpkg["fresh_sql_and_five_store_preflight_succeeded"] is not True or
                dpkg.get("production_package_or_rollback_claim") is not False):
            raise ValueError("Incomplete disposable package rollback")
        recovery = reports["stage1-matched-recovery.json"]
        if recovery["matched_restore_completed"] is not True or recovery["fresh_rollback_completed"] is not True or (
                recovery.get("production_sla_claim") is not False):
            raise ValueError("Incomplete matched SQL/private recovery")
        chaos = reports["beta06-integrated-chaos.json"]
        if chaos["ci_regression_gate_passed"] is not True or chaos["production_claim"] is not False:
            raise ValueError("Integrated chaos evidence incomplete")
    return {"schema": 1, "source_sha": sha, "images": list(stages.IMAGES),
            "required_jobs_declared_completed": verified["successful_required_jobs"],
            "mandatory_redacted_reports_checked": len(REPORTS) * len(stages.IMAGES),
            "offline_evidence_structure_complete": True,
            "github_run_authenticity_independently_verified": False,
            "owner_acceptance_required": True, "merge_authorized": False,
            "production_deployment_authorized": False}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--reports", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(argv)
    sha = subprocess.check_output(["git", "-C", str(args.root), "rev-parse", "HEAD"],
                                  timeout=10).decode("ascii").strip()
    result = evaluate(json.loads(args.manifest.read_text(encoding="utf8")),
                      sha, args.reports, args.root)
    with args.output.open("x", encoding="utf8") as output:
        json.dump(result, output, sort_keys=True); output.write("\n")
    print("Offline exact-head evidence structure complete; GitHub authenticity and Owner NO-GO remain")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
