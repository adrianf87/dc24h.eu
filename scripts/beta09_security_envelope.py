#!/usr/bin/env python3
"""Recompute three hosted transport windows and bounded slow-peer/drain faults.

Proof of reproducible *CI regression envelopes*, not external capacity, SLA or
physical host behavior. TSAN/fuzz/CodeQL are independent mandatory workflows.
"""

import argparse
import json
import math
from pathlib import Path
import re
import subprocess

import beta07_ubuntu_perf_matrix as matrix
import beta07_bounded_fault_gate as faults
import beta09_os_provenance as host


def evaluate(sha, image, adc, https, transport, fault, provenance):
    if (not isinstance(sha, str) or not re.fullmatch(r"[0-9a-f]{40}", sha) or
            image not in host.IMAGES):
        raise ValueError("Wrong source or unsupported measurement image")
    measured = matrix.evaluate(sha, image, adc, https)
    if (not isinstance(transport, dict) or transport.get("schema") != 1 or
            transport.get("source_sha") != sha or transport.get("os_image") != image or
            transport.get("profiles") != measured["profiles"] or
            transport.get("all_three_profiles_recomputed") is not True or
            transport.get("cross_protocol_throughput_equivalent") is not False or
            transport.get("production_sla_or_capacity_claim") is not False):
        raise ValueError("Fabricated or incomparable transport profile summary")
    if (not isinstance(fault, dict) or fault.get("schema") != 1 or
            fault.get("source_sha") != sha or fault.get("os_image") != image or
            fault.get("all_required_fault_cases_passed") is not True or
            fault.get("production_sla_claim") is not False or
            set(fault.get("required_cases", [])) != set(faults.REQUIRED) or
            not isinstance(fault.get("case_seconds"), dict) or
            set(fault["case_seconds"]) != set(faults.REQUIRED)):
        raise ValueError("Incomplete slow-peer/half-open/drain fault cases")
    for value in fault["case_seconds"].values():
        if type(value) not in (int, float) or not math.isfinite(value) or not 0 <= value <= faults.MAX_CASE_SECONDS:
            raise ValueError("Unbounded or nonfinite slow-peer/drain fault duration")
    if (not isinstance(provenance, dict) or provenance.get("schema") != 1 or
            provenance.get("source_sha") != sha or provenance.get("os_image") != image or
            provenance.get("runner_environment") != "github-hosted" or
            not isinstance(provenance.get("selected_public_apt_versions"), dict) or
            set(provenance["selected_public_apt_versions"]) != set(host.REQUIRED_APT) or
            not isinstance(provenance.get("apt_full_installed_inventory_sha256"), str) or
            not re.fullmatch(r"[0-9a-f]{64}", provenance["apt_full_installed_inventory_sha256"]) or
            provenance.get("compiler_version") != transport.get("compiler_version") or
            provenance.get("node_version") != transport.get("node_version") or
            provenance.get("npm_lock_sha256") != transport.get("npm_lock_sha256") or
            provenance.get("apt_repository_snapshot_immutable") is not False or
            provenance.get("fully_reproducible_os_build") is not False):
        raise ValueError("Unbound compiler/Node/npm/Ubuntu apt provenance")
    return {"schema": 1, "source_sha": sha, "os_image": image,
            "distinct_transports": list(matrix.POLICY),
            "windows_per_transport": matrix.WINDOWS,
            "bounded_fault_case_count": len(faults.REQUIRED),
            "profiles": measured["profiles"],
            "npm_lock_sha256": provenance["npm_lock_sha256"],
            "apt_full_installed_inventory_sha256": provenance["apt_full_installed_inventory_sha256"],
            "compiler_version": provenance["compiler_version"],
            "node_version": provenance["node_version"],
            "within_profile_regression_envelope_passed": True,
            "all_required_faults_bounded": True,
            "across_protocol_capacity_equivalence": False,
            "production_sla_or_capacity_approved": False,
            "physical_hardware_certified": False}


def _read(path):
    if not path.is_file() or not 0 < path.stat().st_size <= 256 * 1024:
        raise ValueError("Missing or excessive numeric-only redacted report")
    return json.loads(path.read_text(encoding="utf8"))


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--sha", required=True)
    parser.add_argument("--os-image", choices=host.IMAGES, required=True)
    for name in ("adc", "https", "transport", "fault", "provenance", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        actual = subprocess.check_output(["git", "-C", str(args.root), "rev-parse", "HEAD"],
                                         timeout=10).decode().strip()
        if actual != args.sha:
            raise ValueError("Wrong checkout")
        report = evaluate(args.sha, args.os_image, *(_read(getattr(args, key)) for key in (
            "adc", "https", "transport", "fault", "provenance")))
        with args.output.open("x", encoding="utf8") as stream:
            json.dump(report, stream, sort_keys=True)
            stream.write("\n")
    except (OSError, ValueError, TypeError, KeyError, UnicodeError,
            subprocess.SubprocessError, json.JSONDecodeError):
        parser.exit(1, "Three-window performance, hosted provenance or fault/drain gate refused; NO-GO.\n")
    print("Three distinct hosted transport envelopes and five bounded faults qualified; no SLA")


if __name__ == "__main__":
    raise SystemExit(main())
