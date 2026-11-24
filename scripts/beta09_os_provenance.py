#!/usr/bin/env python3
"""Bounded GitHub Ubuntu APT/build-chain snapshot, not a reproducible OS SBOM.

Collect only public package names/versions and hashed full apt inventory. Never
copy environment variables, apt sources, credentials or raw audit disclosures.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import subprocess

import beta07_supply_chain as supply


SHA = re.compile(r"[0-9a-f]{40}\Z")
VERSION = re.compile(r"v?[0-9]+\.[0-9]+(?:\.[0-9]+)?(?:[+._~-][A-Za-z0-9.+_~-]+)?\Z")
PACKAGE = re.compile(r"[a-zA-Z0-9][a-zA-Z0-9.+:_-]{0,100}\Z")
APT_VERSION = re.compile(r"[A-Za-z0-9][A-Za-z0-9.+:~_-]{0,120}\Z")
IMAGES = ("ubuntu-22.04", "ubuntu-24.04", "ubuntu-26.04")
REQUIRED_APT = ("g++", "cmake", "libssl-dev", "libmariadb-dev", "libgcrypt20-dev",
                "libutf8proc-dev", "openssl", "ncdc")
MAX_APT_BYTES = 1024 * 1024


def parse_apt(raw):
    if not isinstance(raw, str) or not 0 < len(raw.encode("utf8")) <= MAX_APT_BYTES:
        raise ValueError("Missing/oversized Ubuntu package inventory")
    inventory = {}
    for row in raw.splitlines():
        parts = row.split("\t")
        if (len(parts) != 2 or not PACKAGE.fullmatch(parts[0]) or
                not APT_VERSION.fullmatch(parts[1]) or parts[0] in inventory):
            raise ValueError("Malformed or duplicate apt package/version entry")
        inventory[parts[0]] = parts[1]
    if len(inventory) > 5000:
        raise ValueError("Unbounded native Ubuntu package inventory")
    return inventory


def selected_apt(apt):
    selected = {}
    for required in REQUIRED_APT:
        matches = [name for name in apt if name == required or name.startswith(required + ":")]
        if len(matches) != 1:
            raise ValueError("Missing or ambiguous native Ubuntu build dependency")
        selected[required] = apt[matches[0]]
    return selected


def evaluate(root, sha, checked_out, image, os_version, environment, apt_raw,
             compiler, node, npm, cmake):
    if (not isinstance(sha, str) or not SHA.fullmatch(sha) or sha == "0" * 40 or
            checked_out != sha or image not in IMAGES or
            os_version != image.removeprefix("ubuntu-") or
            environment.get("GITHUB_ACTIONS") != "true" or
            environment.get("RUNNER_ENVIRONMENT") != "github-hosted" or
            environment.get("RUNNER_OS") != "Linux"):
        raise ValueError("Wrong-head, non-hosted or wrong Ubuntu image")
    for value in (compiler, node, npm, cmake):
        if not isinstance(value, str) or not VERSION.fullmatch(value):
            raise ValueError("Missing pinned compiler/Node/npm/CMake version")
    apt = parse_apt(apt_raw)
    versions = selected_apt(apt)
    canonical = "".join(f"{name}\t{apt[name]}\n" for name in sorted(apt))
    locked = supply.inventory(root, sha, image, checked_out)
    actions = "\n".join(locked["pinned_actions"]) + "\n"
    return {"schema": 1, "source_sha": sha, "os_image": image,
            "runner_environment": "github-hosted", "compiler_version": compiler,
            "node_version": node, "npm_version": npm, "cmake_version": cmake,
            "npm_lock_sha256": locked["npm_lock_sha256"],
            "npm_locked_package_count": locked["npm_transitive_locked_packages"],
            "pinned_actions_sha256": hashlib.sha256(actions.encode()).hexdigest(),
            "pinned_actions_count": len(locked["pinned_actions"]),
            "apt_full_installed_inventory_sha256": hashlib.sha256(canonical.encode()).hexdigest(),
            "apt_installed_package_count": len(apt),
            "selected_public_apt_versions": versions,
            "apt_repository_snapshot_immutable": False,
            "fully_reproducible_os_build": False,
            "runtime_and_build_chain_advisories_checked_separately": True,
            "production_attestation": False, "owner_release_approval": False}


def _run(*command):
    value = subprocess.check_output(command, timeout=20)
    if len(value) > MAX_APT_BYTES:
        raise ValueError("Unbounded host provenance process output")
    return value.decode("utf8").strip()


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--sha", required=True)
    parser.add_argument("--os-image", choices=IMAGES, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        checkout = _run("git", "-C", str(args.root), "rev-parse", "HEAD")
        apt_raw = _run("dpkg-query", "-W", "--showformat=${binary:Package}\\t${Version}\\n")
        compiler = _run("g++", "-dumpfullversion", "-dumpversion")
        node = _run("node", "--version")
        npm = _run("npm", "--version")
        cmake = _run("cmake", "--version").splitlines()[0].removeprefix("cmake version ")
        report = evaluate(args.root, args.sha, checkout, args.os_image,
                          platform.freedesktop_os_release().get("VERSION_ID"),
                          os.environ, apt_raw, compiler, node, npm, cmake)
        with args.output.open("x", encoding="utf8") as stream:
            json.dump(report, stream, sort_keys=True)
            stream.write("\n")
    except (OSError, ValueError, TypeError, UnicodeError, KeyError, subprocess.SubprocessError):
        parser.exit(1, "Exact-head hosted Ubuntu build/OS evidence unavailable; NO-GO.\n")
    print("GitHub-hosted Ubuntu package/build inventory hashed; OS reproducibility not certified")


if __name__ == "__main__":
    raise SystemExit(main())
