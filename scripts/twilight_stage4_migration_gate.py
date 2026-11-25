#!/usr/bin/env python3
"""Fail closed on incomplete Stage IV name-migration evidence from hosted CI.

This cannot authorize a real-host upgrade, migration or production rollout.
"""

import argparse
import json
from pathlib import Path
import re

import beta07_ci_deb as deb
import beta06_previous_version_rollback as previous


PROOFS = (
    "actual_two_distinct_package_names",
    "clean_old_name_install",
    "new_name_refused_without_explicit_old_purge",
    "failed_new_install_refused_and_old_name_restored",
    "explicit_old_to_new_migration",
    "legacy_install_refused_while_twilight_installed",
    "actual_new_to_old_name_rollback",
    "binary_and_account_admin_hashes_verified",
    "inert_package_units_not_enabled_or_active",
    "systemd_owner_marker_required",
    "systemd_conflict_stopped_old_without_parallel_activation",
    "synthetic_systemd_rollback_restored_old",
)


def evaluate(report_path, sha, os_image):
    if (not re.fullmatch(r"[0-9a-f]{40}", sha) or
            os_image not in ("ubuntu-22.04", "ubuntu-24.04", "ubuntu-26.04")):
        raise ValueError("Exact source and supported hosted Ubuntu image are required")
    report = json.loads(Path(report_path).read_text(encoding="utf8"))
    if type(report) is not dict or any((
        report.get("schema") != 1,
        report.get("source_sha") != sha,
        report.get("previous_sha") != previous.PREVIOUS_SHA,
        report.get("os_image") != os_image,
        report.get("legacy_package") != deb.LEGACY_PACKAGE,
        report.get("twilight_package") != deb.PACKAGE,
        report.get("production_deployment_or_migration_claim") is not False,
    )):
        raise ValueError("Stage IV migration proof has invalid provenance or production scope")
    if not all(report.get(field) is True for field in PROOFS):
        raise ValueError("Missing executed Stage IV old/new transition or systemd proof")
    for field in ("legacy_hub_sha256", "twilight_hub_sha256"):
        if not isinstance(report.get(field), str) or not re.fullmatch(r"[0-9a-f]{64}", report[field]):
            raise ValueError("Stage IV binary hash missing")
    if report["legacy_hub_sha256"] == report["twilight_hub_sha256"]:
        raise ValueError("Stage IV old/new binary identities must differ")
    return {"schema": 1, "source_sha": sha, "previous_sha": previous.PREVIOUS_SHA,
            "os_image": os_image, "two_distinct_package_names_transition_verified": True,
            "negative_and_failed_transition_refused_without_silent_replacement": True,
            "actual_old_new_old_package_and_unit_rehearsal_verified": True,
            "github_hosted_ci_only": True, "production_upgrade_or_migration_authorized": False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--report", required=True, type=Path)
    parser.add_argument("--sha", required=True)
    parser.add_argument("--os-image", required=True)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    result = evaluate(args.report, args.sha, args.os_image)
    with args.output.open("x", encoding="utf8") as handle:
        json.dump(result, handle, sort_keys=True); handle.write("\n")


if __name__ == "__main__":
    main()
