#!/usr/bin/env python3
"""Reject partial/non-equivalent hosted Ubuntu integration + fault evidence.

Only reads synthetic CI aggregates. This is not a production SLO, load or
recovery-time claim, nor proof of third-party ADC client interoperability.
"""

import argparse
import json
from pathlib import Path
import re
import xml.etree.ElementTree as ET


PROFILES = ("adc", "adcs")
FAULT_CASES = ("transfers.protocol.process", "adc.login.process", "adc.close.process",
               "adcs.tls_security.process", "beta.04.stage4.recovery", "systemd.sandbox")
FILES = ("beta06-authenticated-https.json", "beta06-restored-login.json",
         "beta06-previous-version-rollback.json", "stage4-endurance.json", "loopback-probes.json",
         "beta06-authenticated-gate.json")


def require(predicate, explanation):
    if not predicate:
        raise ValueError(explanation)


def read_json(directory, name):
    path = Path(directory) / name
    require(path.is_file() and path.stat().st_size <= 65536, "Missing/oversized CI evidence: " + name)
    value = json.loads(path.read_text(encoding="utf-8"))
    require(type(value) is dict and value.get("schema") == 1, "Invalid CI schema: " + name)
    return value


def assess(directory, junit, sha, os_image):
    require(bool(re.fullmatch(r"[0-9a-f]{40}", sha)), "Invalid source SHA")
    require(os_image in ("ubuntu-22.04", "ubuntu-24.04", "ubuntu-26.04"), "Unapproved Ubuntu image")
    rows = {name: read_json(directory, name) for name in FILES}
    auth, restore, rollback, endurance, probe, gate = (rows[name] for name in FILES)
    for name in FILES[:3]:
        require(rows[name].get("source_sha") == sha and rows[name].get("os_image") == os_image,
                "Stale/wrong-head evidence: " + name)
    require(endurance.get("source_sha") == sha and endurance.get("os") == os_image and
            endurance.get("ci_regression_gate_passed") is True and
            endurance.get("production_capacity_claim") is False,
            "Endurance regression or wrong provenance")
    require(type(auth.get("requests")) is int and type(auth.get("success")) is int and
            type(auth.get("failure")) is int and
            auth.get("requests") == 24 and auth.get("success") == 24 and auth.get("failure") == 0 and
            auth.get("request_spacing_ms") == 1100 and
            auth.get("batch_spacing_ms") == 2200 and
            auth.get("evidence") == "real-account-admin-node-apache-authenticated-totp" and
            auth.get("maintenance_mode") is True and
            auth.get("hub_readonly_boundary_verified") is True and
            auth.get("capacity_claim") is False and auth.get("synthetic_credentials_only") is True and
            type(auth.get("p99_ms")) in (float, int) and
            0 <= auth["p99_ms"] <= auth.get("max_ci_p99_ms", -1) and
            type(auth.get("successful_requests_per_second")) in (float, int) and
            auth["successful_requests_per_second"] > 0 and
            type(auth.get("cpu_mean_percent_one_core")) in (float, int) and
            0 <= auth["cpu_mean_percent_one_core"] <= 400,
            "Real authenticated fixture failed numeric evidence contract")
    for category in ("account_admin", "gateway", "apache"):
        require(type(auth.get("rss_after_kib")) is dict and
                type(auth["rss_after_kib"].get(category)) is int and
                0 < auth["rss_after_kib"][category] <= 524288,
                "Missing/implausible process RSS: " + category)
    require(restore.get("fresh_password_totp_login_accepted") is True and
            restore.get("old_browser_cookie_refused") is True and
            restore.get("revoked_passkey_login_options_refused") is True and
            restore.get("production_recovery_claim") is False,
            "Failed post-restore private authentication")
    require(rollback.get("historical_binary_distinct") is True and
            rollback.get("previous_binary_database_status_accepted") is True and
            rollback.get("fresh_mariadb_v11_and_five_private_files_restored") is True and
            rollback.get("production_rollback_claim") is False and
            rollback.get("previous_version") != rollback.get("current_version"),
            "Rollback is not distinct/fresh/matched")
    require(probe.get("evidence") == "isolated-ubuntu-vm-only" and
            type(probe.get("profiles")) is dict and set(probe["profiles"]) == set(PROFILES),
            "Missing ADC/ADCS real process sample")
    for profile in PROFILES:
        sample = probe["profiles"][profile]
        require(type(sample) is dict and sample.get("profile") == profile and
                type(sample.get("requests")) is int and type(sample.get("success")) is int and
                type(sample.get("failure")) is int and
                sample.get("requests") == 12 and sample.get("success") == 12 and
                sample.get("failure") == 0 and sample.get("capacity_claim") is False,
                "Invalid equivalent ADC/ADCS smoke workload")
    require(gate.get("source_sha") == sha and gate.get("os_image") == os_image and
            gate.get("ci_regression_gate_passed") is True, "Authenticated baseline gate failed")
    cases = ET.parse(junit).getroot().findall(".//testcase")
    names = [item.get("name") for item in cases]
    require(len(names) == len(set(names)), "Duplicate fault test result")
    require(set(FAULT_CASES).issubset(set(names)), "Missing fault injection tests")
    for item in cases:
        if item.get("name") in FAULT_CASES:
            require(not any(item.find(kind) is not None for kind in ("failure", "error", "skipped")),
                    "Failed/skipped required fault test")
    return {"schema": 1, "source_sha": sha, "os_image": os_image,
            "evidence": "bounded-disposable-chaos-with-independent-aggregate-gates",
            "required_fault_cases": list(FAULT_CASES), "evidence_files": list(FILES),
            "authenticated_requests": 24, "adc_adcs_smoke_each": 12,
            "ci_regression_gate_passed": True, "production_claim": False,
            "limitations": ["Separate synthetic fault windows; no atomic cross-process chaos transaction",
                            "No production capacity/RPO/RTO or actual browser passkey ceremony",
                            "No DC++/AirDC++ interoperability guarantee"]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--evidence-dir", type=Path, required=True)
    parser.add_argument("--junit", type=Path, required=True)
    parser.add_argument("--sha", required=True)
    parser.add_argument("--os-image", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = assess(args.evidence_dir, args.junit, args.sha, args.os_image)
    with args.output.open("x", encoding="utf-8") as output:
        json.dump(result, output, indent=2, sort_keys=True)
        output.write("\n")
    print("Authenticated/ADC/ADCS/restore/rollback/chaos CI aggregate PASS (synthetic only)")


if __name__ == "__main__":
    main()
