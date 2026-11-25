#!/usr/bin/env python3
"""Bind beta-0.9 Stage III hosted service, auth and recovery results to one head.

Only consumes already-produced synthetic aggregate reports; no raw secrets,
credentials, screenshots or browser cookies enter the resulting artifact.
"""

import argparse
import json
from pathlib import Path
import re


DPKG_PROOFS = (
    "actual_dpkg_installed_upgrade_downgrade",
    "valid_postinst_failure_required_explicit_previous_package_recovery",
    "previous_executable_hash_and_version_matched",
    "fresh_sql_and_five_store_preflight_succeeded",
    "independently_pinned_offline_hmac_and_anti_reuse_gate",
    "reopened_node_totp_revocation_audit_and_budget_state",
    "historical_maintenance_binary_installed_and_hashed",
    "historical_maintenance_actual_password_totp_and_browser_passkey",
    "historical_maintenance_pinned_status_adapter_ci_only",
    "historical_maintenance_independent_matched_v11_five_store_recovery",
    "actual_systemd_old_cpp_maintenance_start_restart_stop",
)
NODE_PROOFS = (
    "compiled_node_production_only_dependencies",
    "actual_systemd_start_restart_stop",
    "unprivileged_systemd_service",
    "loopback_only_listener",
    "private_owner_only_five_file_state",
    "hardened_systemd_unit_properties",
)


def require_report(path, sha, os_image, fields):
    data = json.loads(Path(path).read_text(encoding="utf8"))
    if (type(data) is not dict or data.get("schema") != 1 or
            data.get("source_sha") != sha or data.get("os_image") != os_image):
        raise ValueError("Stage III evidence is not bound to the checked-out head and OS")
    if not all(data.get(field) is True for field in fields):
        raise ValueError("Stage III evidence lacks a required executed proof")
    return data


def evaluate(dpkg, node, sha, os_image):
    if (not re.fullmatch(r"[0-9a-f]{40}", sha) or
            os_image not in ("ubuntu-22.04", "ubuntu-24.04", "ubuntu-26.04")):
        raise ValueError("Stage III gate requires exact SHA and hosted Ubuntu image")
    dpkg_record = require_report(dpkg, sha, os_image, DPKG_PROOFS)
    node_record = require_report(node, sha, os_image, NODE_PROOFS)
    if dpkg_record.get("production_package_or_rollback_claim") is not False or (
            node_record.get("production_service_changed") is not False):
        raise ValueError("Stage III proof must remain disposable CI-only")
    return {"schema": 1, "source_sha": sha, "os_image": os_image,
            "beta_0_9_9_old_installed_cpp_auth_browser_verified": True,
            "beta_0_9_10_private_dirfd_regressions_required_in_unit_job": True,
            "beta_0_9_11_hmac_six_component_recovery_verified": True,
            "beta_0_9_12_actual_systemd_cpp_and_node_ci_verified": True,
            "production_deployment_or_rollback_proven": False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dpkg", required=True, type=Path)
    parser.add_argument("--systemd-node", required=True, type=Path)
    parser.add_argument("--sha", required=True)
    parser.add_argument("--os-image", required=True)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    result = evaluate(args.dpkg, args.systemd_node, args.sha, args.os_image)
    with args.output.open("x", encoding="utf8") as output:
        json.dump(result, output, sort_keys=True)
        output.write("\n")


if __name__ == "__main__":
    main()
