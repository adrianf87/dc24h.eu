#!/usr/bin/env python3
"""Require executed Chromium and matched C++/MariaDB/Node recovery evidence.

This verifies redacted artifacts on one hosted Ubuntu source head. A synthetic
test report cannot independently authenticate a GitHub workflow, physical
authenticator, offline private-store MAC or production recovery guarantee.
"""

import argparse
import json
import math
from pathlib import Path
import re
import xml.etree.ElementTree as ET

IMAGES = {"ubuntu-22.04", "ubuntu-24.04", "ubuntu-26.04"}
REQUIRED_BROWSER = (
    "real virtual authenticator authenticates after fresh private-file restore and refuses replay",
    "a new password plus TOTP login invalidates the prior sibling view in EN and PL",
)
RESTORE_FLAGS = (
    "old_browser_cookie_refused", "fresh_password_totp_login_accepted",
    "fresh_private_summary_accepted", "replay_ledger_preserved",
    "revoked_passkey_catalog_restored", "revoked_passkey_login_options_refused",
    "restored_browser_password_passkey_accepted", "restored_browser_summary_accepted",
    "restored_browser_assertion_replay_refused",
    "restored_browser_revoke_invalidated_session",
    "revoked_passkey_persisted_after_restart", "real_browser_webauthn_exercised",
)


def assess(restore, authenticated, browser_xml, *, sha, image):
    if not isinstance(sha, str) or not re.fullmatch(r"[0-9a-f]{40}", sha) or sha == "0" * 40:
        raise ValueError("Invalid exact-head SHA")
    if image not in IMAGES:
        raise ValueError("Only GitHub-hosted Ubuntu images are accepted")
    if not isinstance(restore, dict) or restore.get("schema") != 1 or (
        restore.get("source_sha") != sha or restore.get("os_image") != image or
        restore.get("evidence") != "fresh-mariadb-five-store-real-reauth" or
        restore.get("production_recovery_claim") is not False or
        any(restore.get(key) is not True for key in RESTORE_FLAGS)
    ):
        raise ValueError("Missing or wrong-head real browser and matched recovery evidence")
    if not isinstance(authenticated, dict) or authenticated.get("schema") != 1 or (
        authenticated.get("source_sha") != sha or authenticated.get("os_image") != image or
        authenticated.get("ci_regression_gate_passed") is not True or
        authenticated.get("production_capacity_claim") is not False
    ):
        raise ValueError("Missing or wrong-head real C++/Node/Apache authentication gate")
    root = ET.fromstring(browser_xml)
    cases = root.findall(".//testcase")
    if not cases:
        raise ValueError("Missing browser JUnit cases")
    for expected in REQUIRED_BROWSER:
        matches = [case for case in cases if case.get("name") == expected]
        if len(matches) != 1:
            raise ValueError("Missing or duplicate required browser case")
        case = matches[0]
        if any(case.find(tag) is not None for tag in ("failure", "error", "skipped")):
            raise ValueError("Required browser case failed, errored or skipped")
        try:
            duration = float(case.attrib["time"])
        except (ValueError, KeyError) as error:
            raise ValueError("Missing browser duration") from error
        if not math.isfinite(duration) or duration <= 0:
            raise ValueError("Invalid browser duration")
    return {
        "schema": 1, "source_sha": sha, "os_image": image,
        "evidence": "isolated-chromium-and-matched-cpp-node-mariadb-private-restore",
        "required_browser_cases": len(REQUIRED_BROWSER),
        "restored_private_components": 5, "mariadb_schema": 11,
        "ci_regression_gate_passed": True, "physical_authenticator_tested": False,
        "offline_catalog_authenticity_proven": False,
        "production_recovery_claim": False,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--restore", required=True, type=Path)
    parser.add_argument("--auth", required=True, type=Path)
    parser.add_argument("--browser", required=True, type=Path)
    parser.add_argument("--sha", required=True)
    parser.add_argument("--os-image", required=True)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    for path, bound in ((args.restore, 65536), (args.auth, 65536), (args.browser, 1048576)):
        if not path.is_file() or path.stat().st_size > bound:
            parser.error("Missing/oversized exact-head stage II evidence")
    try:
        result = assess(json.loads(args.restore.read_text(encoding="utf-8")),
                        json.loads(args.auth.read_text(encoding="utf-8")),
                        args.browser.read_text(encoding="utf-8"),
                        sha=args.sha, image=args.os_image)
    except (OSError, ValueError, ET.ParseError) as error:
        parser.error("Stage II evidence refused: " + str(error))
    with args.output.open("x", encoding="utf-8") as out:
        json.dump(result, out, sort_keys=True, indent=2)
        out.write("\n")
    print("Verified isolated Stage II Chromium + matched C++/Node/MariaDB evidence")


if __name__ == "__main__":
    main()
