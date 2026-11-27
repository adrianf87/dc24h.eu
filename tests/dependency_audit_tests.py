#!/usr/bin/env python3
"""Check direct hub libraries, the WebAdmin lockfile, and registered security tests."""

import json
import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EXACT_VERSION = re.compile(r"^[0-9]+\.[0-9]+\.[0-9]+$")
NEEDED = re.compile(r"\(NEEDED\)\s+Shared library: \[([^\]]+)\]")
# CMake 3 writes add_test([=[name]=] ...). CMake 4.4 quotes a name that
# contains no quote, dollar sign or backslash: add_test("name" ...).
TEST_NAME = re.compile(
    r'^\s*add_test\((?:\[=*\['
    r'(?P<bracket>.*?)'
    r'\]=*\]|"(?P<quoted>(?:\\.|[^"\\])*)")')
PRODUCT_LIBRARIES = frozenset({
    "libcrypto.so.3",
    "libgcrypt.so.20",
    "libmariadb.so.3",
    "libssl.so.3",
})
# Ubuntu 22.04 ships SONAME 2. Debian 12/13 and Ubuntu 24.04/26.04 ship 3.
UTF8PROC_LIBRARIES = frozenset({
    "libutf8proc.so.2",
    "libutf8proc.so.3",
})
RUNTIME_LIBRARIES = frozenset({
    "libc.so.6",
    "libgcc_s.so.1",
    "libm.so.6",
    "libstdc++.so.6",
})
SANITIZER_LIBRARIES = {
    # GCC 11 from Ubuntu 22.04 links libasan.so.6/libtsan.so.0;
    # newer GCC toolchains link libasan.so.8/libtsan.so.2.
    "address": frozenset({"libasan.so.6", "libasan.so.8"}),
    "undefined": frozenset({"libubsan.so.1"}),
    "thread": frozenset({"libtsan.so.0", "libtsan.so.2"}),
}


def fail(message):
    print(message, file=sys.stderr)
    return False


def needed_entries(binary):
    result = subprocess.run(
        ["readelf", "-d", str(binary)],
        check=False, capture_output=True, text=True,
        env={**os.environ, "LC_ALL": "C"})
    if result.returncode != 0:
        raise RuntimeError(f"Cannot read dynamic section of {binary.name}")
    if "(RPATH)" in result.stdout or "(RUNPATH)" in result.stdout:
        raise RuntimeError(f"{binary.name} has a runtime search path")
    return set(NEEDED.findall(result.stdout))


def check_binary(binary, mode):
    name = binary.name
    needed = needed_entries(binary)
    missing = PRODUCT_LIBRARIES - needed
    if missing:
        return fail(f"{name} is missing a required library: {', '.join(sorted(missing))}")
    utf8 = needed & UTF8PROC_LIBRARIES
    if len(utf8) != 1:
        found = sorted(item for item in needed if item.startswith("libutf8proc.so."))
        return fail(
            f"{name} must link exactly one of {', '.join(sorted(UTF8PROC_LIBRARIES))}; "
            f"found {', '.join(found) if found else 'none'}")
    allowed = set(PRODUCT_LIBRARIES | RUNTIME_LIBRARIES | utf8)
    sanitizer = SANITIZER_LIBRARIES.get(mode)
    if mode and sanitizer is None:
        return fail("DC24H_SANITIZER must be address, undefined, thread, or empty")
    foreign = set().union(*(items for selected_mode, items in SANITIZER_LIBRARIES.items()
                            if selected_mode != mode))
    present_foreign = sorted(item for item in foreign if item in needed)
    if present_foreign:
        return fail(f"{name} links a sanitizer runtime that this build does not select: {', '.join(present_foreign)}")
    if sanitizer:
        present = needed & sanitizer
        if len(present) != 1:
            return fail(f"{name} must link exactly one sanitizer runtime from: {', '.join(sorted(sanitizer))}")
        allowed.update(present)
    unexpected = sorted(needed - allowed)
    if unexpected:
        return fail(f"{name} links an unexpected library: {', '.join(unexpected)}")
    return True


def check_webadmin():
    package = json.loads((ROOT / "webadmin/package.json").read_text(encoding="utf-8"))
    lock = json.loads((ROOT / "webadmin/package-lock.json").read_text(encoding="utf-8"))
    locked = lock.get("packages", {}).get("")
    if not isinstance(locked, dict):
        return fail("WebAdmin lockfile has no root package")
    ok = True
    for field in ("dependencies", "devDependencies"):
        declared = package.get(field, {})
        recorded = locked.get(field, {})
        if declared != recorded:
            ok = fail(f"WebAdmin {field} differ from the lockfile")
            continue
        for name, version in declared.items():
            if not isinstance(version, str) or EXACT_VERSION.fullmatch(version) is None:
                ok = fail(f"WebAdmin dependency is not an exact version: {name}")
    for name, meta in lock.get("packages", {}).items():
        if not name or not isinstance(meta, dict):
            continue
        resolved = meta.get("resolved")
        if resolved is None:
            continue
        if not str(resolved).startswith("https://registry.npmjs.org/"):
            ok = fail(f"WebAdmin package is not from the npm registry: {name}")
        integrity = str(meta.get("integrity", ""))
        if not integrity.startswith("sha512-"):
            ok = fail(f"WebAdmin package has no sha512 integrity: {name}")
    return ok


def registered_tests(build_dir):
    names = set()
    for path in Path(build_dir).rglob("CTestTestfile.cmake"):
        for line in path.read_text(encoding="utf-8").splitlines():
            match = TEST_NAME.match(line)
            if not match:
                continue
            name = match.group("bracket")
            if name is None:
                name = match.group("quoted")
            names.add(name)
    return names


def check_security_tests(build_dir):
    manifest = ROOT / "tests/security_tests.manifest"
    required = [
        line.strip() for line in manifest.read_text(encoding="utf-8").splitlines()
        if line.strip() and not line.startswith("#")]
    if len(required) != len(set(required)):
        return fail("Security test manifest contains a duplicate name")
    present = registered_tests(build_dir)
    missing = [name for name in required if name not in present]
    if missing:
        return fail("Security test is not registered: " + ", ".join(missing))
    return True


def main():
    if len(sys.argv) != 5:
        return fail("Usage: dependency_audit_tests.py BUILD_DIR DC24H DC24H_OWNER DC24H_ACCOUNT_ADMIN")
    build_dir = Path(sys.argv[1])
    binaries = [Path(argument) for argument in sys.argv[2:]]
    mode = os.environ.get("DC24H_SANITIZER", "")
    ok = check_security_tests(build_dir) and check_webadmin()
    for binary in binaries:
        if not binary.is_file():
            ok = fail(f"Hub binary is missing: {binary.name}")
            continue
        try:
            ok = check_binary(binary, mode) and ok
        except (OSError, RuntimeError) as error:
            ok = fail(str(error))
    if not ok:
        return False
    print("Alpha 0.9.17 dependency audit OK")
    return True


if __name__ == "__main__":
    sys.exit(0 if main() else 1)
