#!/usr/bin/env python3
"""Fail-closed source, action pin and npm lock inventory for hosted CI.

No credentials, source files, raw advisory text, npm access tokens or private
package registry URLs enter this narrow evidence. npm audit and CodeQL run as
separate gates; this inventory is not a vulnerability scanner.
"""

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess


SHA = re.compile(r"[0-9a-f]{40}\Z")
PIN = re.compile(r"([A-Za-z0-9_.-]+(?:/[A-Za-z0-9_.-]+){1,2})@([0-9a-f]{40})\Z")
SEMVER = re.compile(r"\d+\.\d+\.\d+(?:[-+][0-9A-Za-z.-]+)?\Z")
IMAGES = {"ubuntu-22.04", "ubuntu-24.04", "ubuntu-26.04"}


def inventory(root: Path, source_sha: str, os_image: str, checked_out_sha: str):
    if not SHA.fullmatch(source_sha) or not SHA.fullmatch(checked_out_sha) or (
            source_sha != checked_out_sha or os_image not in IMAGES):
        raise ValueError("Wrong-head or unexpected runner identity")
    lockfile = root / "webadmin/package-lock.json"
    packagefile = root / "webadmin/package.json"
    lockbytes = lockfile.read_bytes()
    lock = json.loads(lockbytes)
    package = json.loads(packagefile.read_bytes())
    if lock.get("lockfileVersion") != 3 or lock.get("name") != package.get("name") or (
            not isinstance(lock.get("packages"), dict) or "" not in lock["packages"]):
        raise ValueError("Missing immutable npm lock inventory")
    root_package = lock["packages"][""]
    for kind in ("dependencies", "devDependencies"):
        if root_package.get(kind) != package.get(kind):
            raise ValueError("Unreviewed direct package dependency drift")
        for version in package[kind].values():
            if not isinstance(version, str) or not SEMVER.fullmatch(version):
                raise ValueError("Floating npm direct dependency")
    resolved = 0
    for name, entry in lock["packages"].items():
        if not name:
            continue
        if not name.startswith("node_modules/") or not isinstance(entry, dict) or (
                not SEMVER.fullmatch(str(entry.get("version", "")))):
            raise ValueError("Unexpected npm lock entry")
        if "resolved" in entry:
            if not isinstance(entry["resolved"], str) or not entry["resolved"].startswith("https://registry.npmjs.org/") or (
                    not isinstance(entry.get("integrity"), str) or
                    not re.fullmatch(r"sha512-[A-Za-z0-9+/=]+", entry["integrity"])):
                raise ValueError("Missing authenticated npm registry package hash")
            resolved += 1
    actions = set()
    workflows = sorted((root / ".github/workflows").glob("*.yml"))
    if not workflows:
        raise ValueError("No reviewable workflows")
    for workflow in workflows:
        for line in workflow.read_text(encoding="utf8").splitlines():
            m = re.match(r"^\s*-?\s*uses:\s*(\S+)", line)
            if not m:
                continue
            if not PIN.fullmatch(m.group(1)):
                raise ValueError("Unpinned GitHub Action")
            actions.add(m.group(1))
    if not actions or not resolved:
        raise ValueError("Missing verifiable dependency inventory")
    return {"schema": 1, "source_sha": source_sha, "os_image": os_image,
            "npm_lock_sha256": hashlib.sha256(lockbytes).hexdigest(),
            "npm_direct_dependencies": len(package["dependencies"]) + len(package["devDependencies"]),
            "npm_transitive_locked_packages": len(lock["packages"]) - 1,
            "npm_registry_integrity_entries": resolved,
            "pinned_actions": sorted(actions),
            "npm_advisories_checked_separately": True,
            "codeql_checked_separately": True, "production_release_approved": False}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--sha", required=True)
    parser.add_argument("--os-image", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(argv)
    actual_sha = subprocess.check_output(["git", "-C", str(args.root), "rev-parse", "HEAD"],
                                         timeout=10).decode("ascii").strip()
    result = inventory(args.root, args.sha, args.os_image, actual_sha)
    with args.output.open("x", encoding="utf8") as file:
        json.dump(result, file, sort_keys=True); file.write("\n")
    print("Exact-head action and npm integrity inventory recorded; release remains unapproved")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
