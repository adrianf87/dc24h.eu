#!/usr/bin/env python3
"""Gate full npm runtime AND build-chain advisories without exposing raw JSON."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import resource
import subprocess
import tempfile

import beta07_npm_audit_gate as audit

AUDIT_COMMAND = ("npm", "audit", "--include=dev", "--include=prod", "--audit-level=high", "--json")
MAX_AUDIT_BYTES = 4 * 1024 * 1024
EXACT_VERSION = re.compile(r"[0-9]+\.[0-9]+\.[0-9]+(?:-[0-9A-Za-z.-]+)?\Z")
INTEGRITY = re.compile(r"sha512-[A-Za-z0-9+/]+={0,2}\Z")


def _limit_audit_output():
    resource.setrlimit(resource.RLIMIT_FSIZE, (MAX_AUDIT_BYTES, MAX_AUDIT_BYTES))


def collect_full_audit(root):
    """Own the exact npm invocation; a user-supplied prod-only JSON is not evidence."""
    with tempfile.TemporaryFile(mode="w+b") as private_report:
        completed = subprocess.run(AUDIT_COMMAND, cwd=root / "webadmin", stdout=private_report,
                                   stderr=subprocess.DEVNULL, check=False, timeout=180,
                                   preexec_fn=_limit_audit_output)
        private_report.seek(0)
        bounded = private_report.read(MAX_AUDIT_BYTES + 1)
        if not bounded or len(bounded) >= MAX_AUDIT_BYTES:
            raise ValueError("Missing or oversized full-dependency npm audit")
    return json.loads(bounded.decode("utf-8")), completed.returncode


def locked_manifest(manifestbytes, lockbytes):
    """Verify exact direct pins and registry integrity, not just a lock hash."""
    if (not isinstance(manifestbytes, bytes) or not manifestbytes or
            not isinstance(lockbytes, bytes) or not lockbytes):
        raise ValueError("Missing npm manifest/lock")
    manifest, lock = json.loads(manifestbytes), json.loads(lockbytes)
    if not isinstance(manifest, dict) or not isinstance(lock, dict):
        raise ValueError("Invalid npm manifest/lock structure")
    packages = lock.get("packages")
    if (lock.get("lockfileVersion") != 3 or lock.get("requires") is not True or
            not isinstance(packages, dict) or not isinstance(packages.get(""), dict) or
            manifest.get("name") != lock.get("name") or
            manifest.get("version") != lock.get("version") or
            any(packages[""].get(field) != manifest.get(field) for field in
                ("name", "version", "engines", "dependencies", "devDependencies"))):
        raise ValueError("npm manifest does not match v3 lock root")
    for section in ("dependencies", "devDependencies"):
        direct = manifest.get(section, {})
        if not isinstance(direct, dict):
            raise ValueError("Malformed npm dependency section")
        for name, version in direct.items():
            if (not isinstance(name, str) or not isinstance(version, str) or
                    not EXACT_VERSION.fullmatch(version) or
                    not isinstance(packages.get("node_modules/" + name), dict) or
                    packages["node_modules/" + name].get("version") != version):
                raise ValueError("Unpinned or mismatched direct npm dependency")
    if not any(manifest.get(key) for key in ("dependencies", "devDependencies")):
        raise ValueError("Empty npm dependency inventory")
    for name, package in packages.items():
        if name == "":
            continue
        if (not isinstance(name, str) or not name.startswith("node_modules/") or
                not isinstance(package, dict) or
                not isinstance(package.get("resolved"), str) or
                not package["resolved"].startswith("https://registry.npmjs.org/") or
                not isinstance(package.get("integrity"), str) or
                not INTEGRITY.fullmatch(package["integrity"])):
            raise ValueError("Unreviewed transitive npm source or missing SHA-512 integrity")
    return hashlib.sha256(lockbytes).hexdigest()


def evaluate(raw, sha, checked_out, image, audit_exit, lockbytes, manifestbytes):
    old = audit.evaluate(raw, sha, checked_out, image, audit_exit)
    lock_sha = locked_manifest(manifestbytes, lockbytes)
    lock = json.loads(lockbytes)
    packages = lock.get("packages") if isinstance(lock, dict) else None
    if (not isinstance(packages, dict) or
            not any(isinstance(meta, dict) and meta.get("dev") is True
                    for path, meta in packages.items() if path.startswith("node_modules/")) or
            not any(isinstance(meta, dict) and meta.get("dev") is not True
                    for path, meta in packages.items() if path.startswith("node_modules/"))):
        raise ValueError("Missing dev or runtime dependencies in the audited lockfile")
    return {"schema": 1, "source_sha": sha, "os_image": image,
            "npm_lock_sha256": lock_sha,
            "runtime_and_build_chain_audited": True,
            "scope_enforced_by_invoking_npm": True,
            "advisory_totals": old["advisory_totals"],
            "unreviewed_high_or_critical": 0,
            "dependency_exceptions": [],  # Never invent a reviewer-approved waiver.
            "raw_advisories_exported": False, "production_release_approved": False}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--sha", required=True)
    parser.add_argument("--os-image", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        actual = subprocess.check_output(["git", "-C", str(args.root), "rev-parse", "HEAD"],
                                         timeout=10).decode().strip()
        if actual != args.sha:
            raise ValueError("Wrong checkout source SHA")
        raw, audit_exit = collect_full_audit(args.root)
        report = evaluate(raw, args.sha, actual, args.os_image, audit_exit,
                          (args.root / "webadmin/package-lock.json").read_bytes(),
                          (args.root / "webadmin/package.json").read_bytes())
        with args.output.open("x", encoding="utf8") as output:
            json.dump(report, output, sort_keys=True)
            output.write("\n")
    except (OSError, ValueError, TypeError, KeyError, UnicodeError, subprocess.SubprocessError):
        parser.exit(1, "Full npm build-chain advisory gate refused; raw details suppressed.\n")
    print("Runtime and build-chain npm advisories reviewed at exact head; no release authorization")


if __name__ == "__main__":
    raise SystemExit(main())
