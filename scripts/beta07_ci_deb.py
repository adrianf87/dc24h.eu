#!/usr/bin/env python3
"""Build an inert, disposable dpkg fixture; NEVER publish it as a release.

The exact binary under test is copied into an otherwise private .deb. No
maintainer scripts, postinst, network listeners, enable/start, or credentials.
This is deliberately not an installable production service package.
"""

import hashlib
import os
from pathlib import Path
import re
import shutil
import subprocess


PACKAGE = "dc24h-high-starter-ci-fixture"
UNIT = "dc24h-high-starter-ci-fixture.service"
BIN = f"usr/lib/{PACKAGE}/dc24h"
UNIT_PATH = f"lib/systemd/system/{UNIT}"
DOC_PATH = f"usr/share/doc/{PACKAGE}/ci-provenance"
VERSIONS = {"previous": "0.5.16+ci1", "current": "0.8.20+ci1"}
# A real dpkg pre-unpack failure, not merely a malformed .deb rejected by the
# archive reader. This higher version is never a deployable package.
FAILED_UPGRADE_VERSION = "0.8.20+ci2"
# A separate, valid package that fails *after* unpack during configuration.
# The hosted test must explicitly reinstall the known-good previous package.
FAILED_POSTINST_VERSION = "0.8.20+ci3"


def build(binary: Path, destination: Path, version: str, sha: str, *, fail_preinst=False, fail_postinst=False):
    if fail_preinst and fail_postinst:
        raise ValueError("Only one controlled package failure is allowed")
    allowed = (version == FAILED_UPGRADE_VERSION if fail_preinst else
               version == FAILED_POSTINST_VERSION if fail_postinst else version in VERSIONS.values())
    if not allowed or not re.fullmatch(r"[0-9a-f]{40}", sha):
        raise ValueError("Unreviewed package version or source identity")
    binary = Path(binary).resolve(strict=True)
    destination = Path(destination)
    if not binary.is_file() or not os.access(binary, os.X_OK) or not destination.is_absolute():
        raise ValueError("Exact executable and absolute fixture directory required")
    if destination.exists() or destination.is_symlink():
        raise ValueError("CI staging directory must not exist")
    package = destination.parent / f"{PACKAGE}_{version}_amd64.deb"
    if package.exists() or package.is_symlink():
        raise ValueError("Cannot overwrite existing package")
    if subprocess.run(["dpkg", "--print-architecture"], capture_output=True, check=True).stdout.strip() != b"amd64":
        raise ValueError("CI package fixture is pinned to amd64")
    destination.mkdir(mode=0o700)
    control = destination / "DEBIAN"
    control.mkdir(mode=0o755)
    (control / "control").write_text(
        f"Package: {PACKAGE}\nVersion: {version}\nArchitecture: amd64\n"
        "Section: admin\nPriority: optional\n"
        "Maintainer: Isolated CI Fixture <ci@example.invalid>\n"
        "Depends: libc6, libstdc++6\n"
        "Description: Ephemeral dc24h package-manager regression fixture\n" +
        (" Injected pre-unpack failure; never deploy or start.\n" if fail_preinst else
         " Injected post-unpack failure; never deploy or start.\n" if fail_postinst else
         " Not a deployable service, cannot be enabled, has no maintainer scripts.\n"),
        encoding="ascii")
    (control / "control").chmod(0o644)
    if fail_preinst:
        # Deliberately abort before dpkg can unpack or replace an installed
        # executable. Never put a maintainer script in the normal fixture.
        preinst = control / "preinst"
        preinst.write_text("#!/bin/sh\nexit 41\n", encoding="ascii")
        preinst.chmod(0o755)
    if fail_postinst:
        postinst = control / "postinst"
        postinst.write_text("#!/bin/sh\nexit 42\n", encoding="ascii")
        postinst.chmod(0o755)
    executable = destination / BIN
    executable.parent.mkdir(parents=True, mode=0o755)
    shutil.copy2(binary, executable)
    executable.chmod(0o755)
    unit = destination / UNIT_PATH
    unit.parent.mkdir(parents=True, mode=0o755)
    unit.write_text(
        "[Unit]\nDescription=Disposable dc24h CI fixture; MUST NOT START\n"
        "ConditionPathExists=/__dc24h_ci_fixture_never_start__\n"
        "[Service]\nType=oneshot\nExecStart=/usr/lib/" + PACKAGE + "/dc24h --version\n"
        "NoNewPrivileges=yes\nPrivateNetwork=yes\n",
        encoding="ascii")
    unit.chmod(0o644)
    record = destination / DOC_PATH
    record.parent.mkdir(parents=True, mode=0o755)
    record.write_text(f"synthetic-test-only\nsource-sha={sha}\n"
                      f"binary-sha256={hashlib.sha256(binary.read_bytes()).hexdigest()}\n",
                      encoding="ascii")
    record.chmod(0o644)
    result = subprocess.run(["dpkg-deb", "--build", "--root-owner-group", str(destination), str(package)],
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=30)
    if result.returncode:
        raise RuntimeError("Disposable fixture package build refused")
    return package
