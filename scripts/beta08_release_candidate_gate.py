#!/usr/bin/env python3
"""Offline beta-0.8.20 candidate consistency; GitHub API acceptance is separate.

Never treat a user-provided report or green job manifest as authenticated CI.
This tool validates files/versions and remains NO-GO without the later direct
GitHub Actions API verification and explicit Owner merge authorization.
"""

import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import subprocess

import beta07_ubuntu_perf_matrix as matrix


IMAGES = ("ubuntu-22.04", "ubuntu-24.04", "ubuntu-26.04")
SHA = re.compile(r"[0-9a-f]{40}\Z")
REPORTS = ("beta07-supply-chain.json", "beta07-transport-performance.json",
           "beta08-transport-evidence.json", "beta08-build-chain-audit.json",
           "beta07-bounded-faults.json", "beta07-dpkg-rollback.json",
           "stage1-matched-recovery.json", "beta06-integrated-chaos.json")


def load_report(root, image, name, sha):
    path = root / image / name
    if not path.is_file() or not 0 < path.stat().st_size <= 262144:
        raise ValueError("Missing or oversized candidate report")
    doc = json.loads(path.read_text(encoding="utf8"))
    if (not isinstance(doc, dict) or doc.get("schema") != 1 or
            doc.get("source_sha") != sha or
            (name != "stage1-matched-recovery.json" and doc.get("os_image") != image)):
        raise ValueError("Wrong-head, wrong-image or malformed candidate report")
    return doc


def evaluate(checkout, sha, reports, hardening):
    if not isinstance(sha, str) or not SHA.fullmatch(sha):
        raise ValueError("Missing exact source head")
    expected = {
        "CMakeLists.txt": ('project(dc24h VERSION 0.8.20', 'set(DC24H_VERSION "beta-0.8.20")'),
        "webadmin/src/lib/status.ts": ('WEBADMIN_VERSION = "beta-0.8.20"',),
        "README.md": ("Current development version: beta-0.8.20",),
        "CHANGELOG.md": ("beta-0.8.17–beta-0.8.20",),
        "doc/us/BETA_0.8_STAGE_V.md": ("NO-GO",),
        "doc/pl/BETA_0.8_STAGE_V.md": ("NO-GO",),
    }
    for path, fragments in expected.items():
        text = (checkout / path).read_text(encoding="utf8")
        if any(fragment not in text for fragment in fragments):
            raise ValueError("Missing or false release-candidate metadata")
    package = json.loads((checkout / "webadmin/package.json").read_text())
    lockbytes = (checkout / "webadmin/package-lock.json").read_bytes()
    lock = json.loads(lockbytes)
    if (package.get("version") != "0.8.20" or lock.get("version") != "0.8.20" or
            lock.get("packages", {}).get("", {}).get("version") != "0.8.20" or
            package.get("name") != lock.get("name") or
            package.get("name") != lock.get("packages", {}).get("", {}).get("name")):
        raise ValueError("Unaligned npm candidate identity")
    digest = hashlib.sha256(lockbytes).hexdigest()
    for image in IMAGES:
        content = {name: load_report(reports, image, name, sha) for name in REPORTS}
        supply = content["beta07-supply-chain.json"]
        if (supply.get("npm_lock_sha256") != digest or
                not isinstance(supply.get("pinned_actions"), list) or not supply["pinned_actions"]):
            raise ValueError("Dependency/action provenance does not match source")
        audit = content["beta08-build-chain-audit.json"]
        if (audit.get("npm_lock_sha256") != digest or
                audit.get("runtime_and_build_chain_audited") is not True or
                audit.get("unreviewed_high_or_critical") != 0 or
                audit.get("dependency_exceptions") != [] or
                audit.get("raw_advisories_exported") is not False):
            raise ValueError("Unreviewed runtime or build-chain advisories")
        original = content["beta07-transport-performance.json"]
        transport = content["beta08-transport-evidence.json"]
        if (transport.get("all_three_profiles_recomputed") is not True or
                transport.get("profiles") != original.get("profiles") or
                transport.get("npm_lock_sha256") != digest or
                transport.get("cross_protocol_throughput_equivalent") is not False or
                transport.get("production_sla_or_capacity_claim") is not False):
            raise ValueError("Missing reproducible transport evidence")
        for name, budget in matrix.POLICY.items():
            row = transport["profiles"].get(name, {})
            if (row.get("windows") != 3 or row.get("samples") != 3 * budget["requests"] or
                    row.get("failed_requests") != 0 or row.get("ci_regression_envelope_passed") is not True or
                    not isinstance(row.get("max_observed_p99_ms"), (int, float)) or
                    not math.isfinite(row["max_observed_p99_ms"]) or
                    row["max_observed_p99_ms"] > budget["p99_ceiling_ms"]):
                raise ValueError("Incomplete bounded transport samples")
        fault = content["beta07-bounded-faults.json"]
        if (fault.get("all_required_fault_cases_passed") is not True or
                fault.get("production_sla_claim") is not False):
            raise ValueError("Missing bounded fault/drain evidence")
        dpkg = content["beta07-dpkg-rollback.json"]
        recovery = content["stage1-matched-recovery.json"]
        chaos = content["beta06-integrated-chaos.json"]
        if (dpkg.get("actual_dpkg_installed_upgrade_downgrade") is not True or
                recovery.get("matched_restore_completed") is not True or
                chaos.get("ci_regression_gate_passed") is not True):
            raise ValueError("Incomplete recovery/package/chaos checks")
    tsan = load_report(hardening, "ubuntu-24.04", "tsan-provenance.json", sha)
    fuzz = load_report(hardening, "ubuntu-24.04", "fuzz-smoke.json", sha)
    if (tsan.get("result") != "passed" or tsan.get("suite") != "beta08-tsan" or
            type(tsan.get("tests")) is not int or tsan["tests"] < 3 or
            fuzz.get("all_required_fuzz_checks_passed") is not True or
            fuzz.get("observed_runs", 0) < 1024 or
            fuzz.get("sanitizer_findings") != 0 or
            fuzz.get("raw_traces_uploaded") is not False):
        raise ValueError("TSAN/fuzz missing, unsupported or incomplete")
    return {"schema": 1, "source_sha": sha, "ubuntu_images": list(IMAGES),
            "locally_consistent_redacted_reports": len(IMAGES) * len(REPORTS) + 2,
            "offline_structure_qualified": True,
            "authenticated_github_api_verification_still_required": True,
            "owner_release_approval": False, "merge_authorized": False,
            "production_deployment_authorized": False}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--reports", type=Path, required=True)
    parser.add_argument("--hardening", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(argv)
    sha = subprocess.check_output(["git", "-C", str(args.root), "rev-parse", "HEAD"],
                                  timeout=10).decode().strip()
    report = evaluate(args.root, sha, args.reports, args.hardening)
    with args.output.open("x", encoding="utf8") as output:
        json.dump(report, output, sort_keys=True)
        output.write("\n")
    print("Offline evidence shape consistent; GitHub API authenticity and Owner approval still NO-GO")


if __name__ == "__main__":
    raise SystemExit(main())
