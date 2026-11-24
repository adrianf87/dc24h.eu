#!/usr/bin/env python3
"""Strict beta-0.8 Stage I acceptance from executed hosted-CTest evidence.

This composes the separately pinned ncdc process report with *additional*
hub-process cases. A Python socket fixture is not misrepresented as a second
independent ADC client, and DC++/AirDC++ remain untested until actual native
executables and verified executed cases can be supplied in a later review.
"""

import argparse
import json
import os
from pathlib import Path
import platform
import subprocess

from beta07_client_evidence import (CAPABILITIES, COMMIT_SHA, PINS,
                                    collect_tests, require_hosted_runner)


# These are CTest cases, not Python method names. Missing, failed, skipped,
# duplicated or nonfinite JUnit cases must not become a Stage I PASS.
PATCH_CASES = {
    "0.8.1": ("adc.ncdc", "adcs.ncdc", "adc.capabilities.ncdc", "adcs.capabilities.ncdc"),
    "0.8.2": ("adc.login.process", "accounts.adc_authentication.process",
              "adc.sup.ncdc", "adc.inf.ncdc", "adc.identity.ncdc"),
    "0.8.3": ("adc.sch.process", "adc.ctm.process", "adc.rcm.process",
              "adc.activepassive.ncdc", "adcs.activepassive.ncdc",
              "adc.searchsecurity.ncdc", "adcs.searchsecurity.ncdc",
              "adc.transferlimits.ncdc", "adcs.transferlimits.ncdc",
              "adc.transfererrors.ncdc", "adcs.transfererrors.ncdc"),
    "0.8.4": ("adcs.process", "adcs.tls_security.process",
              "adc.ctm.adcs.ncdc", "adcs.ctm.adcs.ncdc",
              "adc.rcm.adcs.ncdc", "adcs.rcm.adcs.ncdc"),
}

EXTERNAL_CANDIDATES = ("DC++", "AirDC++")


def verify(junit: Path, previous: dict, source_sha: str, os_image: str) -> dict:
    if not isinstance(source_sha, str) or not COMMIT_SHA.fullmatch(source_sha):
        raise ValueError("Invalid source SHA")
    if os_image not in PINS or not isinstance(previous, dict):
        raise ValueError("Invalid Ubuntu image or client evidence")
    if (previous.get("schema") != 1 or previous.get("result") != "passed" or
            previous.get("source_sha") != source_sha or previous.get("os_image") != os_image or
            previous.get("git_checkout_head_verified") is not True):
        raise ValueError("Missing, failed or wrong-head independent client evidence")
    verified = previous.get("client")
    if not isinstance(verified, dict) or verified.get("package") != "ncdc" or verified.get("version") != PINS[os_image]:
        raise ValueError("Unpinned independent client binary")
    import re
    for name in ("package_sha256", "executable_sha256"):
        if not isinstance(verified.get(name), str) or not re.fullmatch(r"[0-9a-f]{64}", verified[name]):
            raise ValueError("Unverified independent client hash")

    clients = previous.get("capabilities")
    if not isinstance(clients, dict) or set(clients) != {"ncdc", *EXTERNAL_CANDIDATES}:
        raise ValueError("Unknown or missing external client matrix")
    required = set(CAPABILITIES.values())
    for names in PATCH_CASES.values():
        required.update(names)
    actual = collect_tests(junit, allowed_names=required)
    ncdc = clients["ncdc"]
    if not isinstance(ncdc, dict) or set(ncdc) != set(CAPABILITIES):
        raise ValueError("Incomplete ncdc capability inventory")
    for capability, name in CAPABILITIES.items():
        if ncdc.get(capability) != {"state": "passed", "test": name} or actual.get(name) != "passed":
            raise ValueError("Missing or forged ncdc process case: " + name)
    for name in EXTERNAL_CANDIDATES:
        # A vendor name/version cannot qualify itself with a JSON assertion.
        # This plan intentionally does not execute either native binary.
        if clients[name] != {"state": "untested", "reason": "no executable client fixture"}:
            raise ValueError("Unexecuted external client advertised as verified")

    patches = {}
    for patch, names in PATCH_CASES.items():
        missing = [name for name in names if actual.get(name) != "passed"]
        if missing:
            raise ValueError("Stage I required CTest case missing/failed: " + ",".join(missing))
        patches[patch] = {"state": "passed", "tests": list(names)}
    return {
        "schema": 1, "source_sha": source_sha, "os_image": os_image,
        "scope": "synthetic GitHub-hosted Ubuntu; not physical client or production certification",
        "ncdc_package_version": verified["version"],
        "ncdc_executable_sha256": verified["executable_sha256"],
        "ncdc_package_sha256": verified["package_sha256"],
        "additional_native_clients": {
            name: {"state": "not_executed", "version": None,
                   "reason": "no version-pinned executable and process CTest evidence on this hosted image"}
            for name in EXTERNAL_CANDIDATES
        },
        "patches": patches, "result": "passed",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--junit", required=True, type=Path)
    parser.add_argument("--ncdc-report", required=True, type=Path)
    parser.add_argument("--sha", required=True)
    parser.add_argument("--os-image", required=True, choices=tuple(PINS))
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    require_hosted_runner(args.os_image, os.environ,
                          platform.freedesktop_os_release().get("VERSION_ID"))
    root = Path(__file__).resolve().parents[1]
    actual_sha = subprocess.check_output(
        ["git", "-C", str(root), "rev-parse", "HEAD"], text=True, timeout=8).strip()
    if args.sha != actual_sha:
        raise ValueError("Wrong checked-out head")
    previous = json.loads(args.ncdc_report.read_text(encoding="utf-8"))
    report = verify(args.junit, previous, args.sha, args.os_image)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("x", encoding="utf-8") as file:
        json.dump(report, file, indent=2, sort_keys=True)
        file.write("\n")
    print("Verified four beta-0.8 Stage I patches from exact-head executed process evidence")


if __name__ == "__main__":
    main()
