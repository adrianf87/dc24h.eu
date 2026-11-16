#!/usr/bin/env python3
"""Check release metadata and ordinary local Markdown links without Node or Git."""

import json
from pathlib import Path
import re
import sys
from urllib.parse import unquote, urlsplit


def check_project(root):
    errors = []
    cmake = (root / "CMakeLists.txt").read_text(encoding="utf-8")
    match = re.search(r"project\(dc24h VERSION (\d+\.\d+\.\d+) LANGUAGES CXX\)", cmake)
    if not match:
        return ["CMake project version is missing"]
    version = match.group(1)
    package = json.loads((root / "webadmin/package.json").read_text(encoding="utf-8"))
    lock = json.loads((root / "webadmin/package-lock.json").read_text(encoding="utf-8"))
    for name, actual in (
        ("package.json", package.get("version")),
        ("package-lock.json", lock.get("version")),
        ("package-lock.json root package", lock.get("packages", {}).get("", {}).get("version")),
    ):
        if actual != version:
            errors.append(f"{name}: expected {version}, got {actual!r}")
    frontend = (root / "webadmin/src/lib/status.ts").read_text(encoding="utf-8")
    if f'export const WEBADMIN_VERSION = "alpha-{version}";' not in frontend:
        errors.append("WEBADMIN_VERSION differs from CMake project version")
    # Keep black-box CLI/log/API and browser fixtures on the release version.
    # Otherwise metadata can pass while the real process tests expect an older build.
    for name in ("tests/process_tests.py", "tests/logging_process_tests.py", "tests/api_process_tests.py",
                 "webadmin/tests/fixtures.ts", "webadmin/tests/panel.spec.ts", "webadmin/tests/live-panel.mjs"):
        versions = set(re.findall(r"alpha-\d+\.\d+\.\d+", (root / name).read_text(encoding="utf-8")))
        if versions != {f"alpha-{version}"}:
            errors.append(f"{name}: release version fixtures differ from CMake project version")

    us = {p.relative_to(root / "doc/us") for p in (root / "doc/us").rglob("*.md")}
    pl = {p.relative_to(root / "doc/pl") for p in (root / "doc/pl").rglob("*.md")}
    if not us or us != pl:
        errors.append(f"Documentation language file mismatch: US-only={sorted(us - pl)}, PL-only={sorted(pl - us)}")
    current_documents = {root / "README.md", root / "doc/README.md",
                         root / "doc/us/README.md", root / "doc/pl/README.md"}
    documents = sorted(set((root / "doc").rglob("*.md")) | {root / "README.md", root / "CHANGELOG.md"})
    for path in documents:
        text = path.read_text(encoding="utf-8")
        name = path.relative_to(root)
        if "\ufffd" in text or text.startswith("\ufeff"):
            errors.append(f"{name}: replacement character or UTF-8 BOM")
        if path in current_documents:
            header = text.splitlines()[:6]
            if not any(f"alpha-{version}" in line for line in header):
                errors.append(f"{name}: current version missing from introduction")
        for target in re.findall(r"\[[^\]]*\]\(([^\s)]+)\)", text):
            url = urlsplit(target)
            if url.scheme or url.netloc or not url.path:
                continue
            local = (path.parent / unquote(url.path)).resolve()
            try:
                local.relative_to(root)
            except ValueError:
                errors.append(f"{name}: link leaves project: {target}")
                continue
            if not local.exists():
                errors.append(f"{name}: missing local link target: {target}")
    return errors


if __name__ == "__main__":
    try:
        problems = check_project(Path(__file__).resolve().parents[1])
    except (OSError, UnicodeError, ValueError) as error:
        print(f"Project metadata check failed: {error}", file=sys.stderr)
        sys.exit(1)
    if problems:
        print("\n".join(problems), file=sys.stderr)
        sys.exit(1)
    print("Release metadata, documentation UTF-8, language file parity and local Markdown targets passed")
