#!/usr/bin/env python3
"""Fail-closed third-party ADC client evidence for hosted Ubuntu.

This intentionally does not run DC++ or AirDC++: the only executable client
in the hosted fixtures is ncdc. Its version must match the Ubuntu package
pin, its installed files must pass dpkg verification, and the downloaded .deb
must match the SHA-256 from the apt authenticated package index. Only an
executed, successful CTest case can be advertised as validated.
"""

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile
import xml.etree.ElementTree as ET


PINS = {
    "ubuntu-22.04": "1.22.1-3",
    "ubuntu-24.04": "1.23.1-1build2",
    "ubuntu-26.04": "1.23.1-1.1",
}
CAPABILITIES = {
    "adc_guest_login": "adc.ncdc",
    "adcs_guest_login": "adcs.ncdc",
    "adc_tth_search": "adc.tth.ncdc",
    "adcs_tth_search": "adcs.tth.ncdc",
    "adc_active_passive": "adc.activepassive.ncdc",
    "adcs_active_passive": "adcs.activepassive.ncdc",
    "adc_ctm": "adc.ctm.ncdc",
    "adcs_ctm": "adcs.ctm.ncdc",
    "adc_rcm": "adc.rcm.ncdc",
    "adcs_rcm": "adcs.rcm.ncdc",
}
SHA = re.compile(r"[0-9a-f]{64}\Z")
COMMIT_SHA = re.compile(r"[0-9a-f]{40}\Z")


def run(*args, cwd=None):
    return subprocess.run(args, cwd=cwd, text=True, capture_output=True,
                          timeout=45, check=True).stdout.strip()


def file_sha256(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(65536), b""):
            digest.update(block)
    return digest.hexdigest()


def package_sha256(version):
    """Fetch an exact-version .deb and verify its authenticated apt-index digest."""
    candidate = run("apt-cache", "show", "--no-all-versions", f"ncdc={version}")
    matches = re.findall(r"(?m)^SHA256: ([0-9a-f]{64})$", candidate)
    if len(matches) != 1:
        raise ValueError("Missing/ambiguous authenticated apt SHA-256")
    with tempfile.TemporaryDirectory(prefix="beta07-ncdc-package-") as temp:
        run("apt-get", "download", f"ncdc={version}", cwd=temp)
        packages = list(Path(temp).glob("ncdc_*.deb"))
        if len(packages) != 1:
            raise ValueError("Expected exactly one downloaded package")
        digest = file_sha256(packages[0])
    if digest != matches[0]:
        raise ValueError("ncdc package SHA-256 does not match apt index")
    return digest


def verify_ncdc(os_image):
    expected = PINS.get(os_image)
    if expected is None:
        raise ValueError("Unsupported runner image")
    installed = run("dpkg-query", "-W", "-f=${Version}", "ncdc")
    if installed != expected:
        raise ValueError(f"ncdc version drift on {os_image}")
    # dpkg -V reports altered package files; do not export its paths/details.
    run("dpkg", "--verify", "ncdc")
    executable = Path(run("readlink", "-f", "/usr/bin/ncdc"))
    if not executable.is_file() or not executable.is_relative_to(Path("/usr")):
        raise ValueError("Unexpected installed client path")
    executable_sha = file_sha256(executable)
    return {"package": "ncdc", "version": installed,
            "package_sha256": package_sha256(installed),
            "executable_sha256": executable_sha}


def collect_tests(junit):
    root = ET.parse(junit).getroot()
    cases = root.findall(".//testcase")
    names = [item.attrib.get("name") for item in cases]
    if len(names) != len(set(names)):
        raise ValueError("Duplicate CTest test case")
    states = {}
    for item in cases:
        name = item.attrib.get("name", "")
        if name in CAPABILITIES.values():
            states[name] = ("passed" if not any(item.find(tag) is not None for tag in
                                                 ("failure", "error", "skipped")) and
                            float(item.attrib.get("time", "0")) >= 0 else "failed")
    return states


def produce(os_image, source_sha, client, states):
    if not COMMIT_SHA.fullmatch(source_sha):
        raise ValueError("Invalid source SHA")
    if not client.get("package_sha256") or not SHA.fullmatch(client["package_sha256"]):
        raise ValueError("No verified client package digest")
    matrix = {"ncdc": {capability: {"state": states.get(test, "not_executed"),
                                 "test": test}
                       for capability, test in CAPABILITIES.items()},
              "DC++": {"state": "untested", "reason": "no executable client fixture"},
              "AirDC++": {"state": "untested", "reason": "no executable client fixture"}}
    if any(row["state"] != "passed" for row in matrix["ncdc"].values()):
        raise ValueError("Required real ncdc process evidence is incomplete")
    return {"schema": 1, "source_sha": source_sha, "os_image": os_image,
            "client": client, "capabilities": matrix, "result": "passed",
            "scope": "synthetic hosted Ubuntu; not third-party interoperability certification"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--junit", type=Path, required=True)
    parser.add_argument("--sha", required=True)
    parser.add_argument("--os-image", required=True, choices=tuple(PINS))
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    report = produce(args.os_image, args.sha, verify_ncdc(args.os_image),
                     collect_tests(args.junit))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("x", encoding="utf-8") as stream:
        json.dump(report, stream, indent=2, sort_keys=True)
        stream.write("\n")
    print("Validated pinned ncdc package and ten real ADC/ADCS process cases")


if __name__ == "__main__":
    main()
