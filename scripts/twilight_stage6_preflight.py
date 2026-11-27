#!/usr/bin/env python3
"""Bounded, fail-closed Twilight source-identity preflight; NEVER a release GO.

This checks selected current user-facing/contract files against the approved
Twilight naming and compatibility policy on the exact checked-out source SHA.
It does NOT verify GitHub CI runs, independently downloaded artifact ZIPs,
production installation, physical passkeys or OWNER release authorization.
"""

import argparse
import json
from pathlib import Path
import re
import subprocess


REPO = "adrianf87/dc24h.eu-twilight"
OLD_REPO = "adrianf87/dc24h.eu-high-starter"
BRAND = "dc24h.eu-twilight"
IMAGES = ("ubuntu-22.04",)
SHA = re.compile(r"[0-9a-f]{40}\Z")
ACTIVE_DISPLAY = (
    "src/main.cpp",
    "src/adc/user_list.cpp",
    "languages/pl/messages.hpp",
    "languages/us/messages.hpp",
    "include/dc24h/chat/settings.hpp",
    "config/dc24h.ini.example",
    "webadmin/index.html",
    "webadmin/remote.html",
    "webadmin/src/App.tsx",
    "webadmin/src/remote/RemoteApp.tsx",
    "webadmin/src/languages/pl/index.ts",
    "webadmin/src/languages/us/index.ts",
)
WORKFLOWS = (
    "build.yml",
    "beta07-codeql.yml",
    "beta08-hardening.yml",
    "beta09-stage4-storage.yml",
    "stage4-ubuntu-vm-validation.yml",
)
EXPECTED_TWILIGHT_GUARDS = {
    "build.yml": 2,
    "beta07-codeql.yml": 1,
    "beta08-hardening.yml": 2,
    "beta09-stage4-storage.yml": 1,
    "stage4-ubuntu-vm-validation.yml": 1,
}
LEGACY_BRAND = re.compile(r"(?i)(?:dc24h\.eu[ -]high[ -]starter|\bhigh starter\b)")


def read(root, relative):
    base = Path(root).resolve(strict=True)
    target = (base / relative).resolve(strict=True)
    if not target.is_relative_to(base) or not target.is_file() or (
            not 0 < target.stat().st_size <= 1024 * 1024):
        raise ValueError("Missing, unbounded or external identity source")
    return target.read_text(encoding="utf8")


def require(text, phrases, area):
    if not all(phrase in text for phrase in phrases):
        raise ValueError("Twilight identity or security contract regressed: " + area)


def evaluate(root, sha, os_image):
    if (not isinstance(sha, str) or not SHA.fullmatch(sha) or sha == "0" * 40 or
            os_image not in IMAGES):
        raise ValueError("Exact source SHA and GitHub-hosted Ubuntu image required")
    for name in ACTIVE_DISPLAY:
        content = read(root, name)
        if BRAND not in content or LEGACY_BRAND.search(content):
            raise ValueError("Unreviewed legacy user-facing branding: " + name)
    # The visible version/banner and HTML metadata/title must *both* retain
    # the reviewed identity; a single remaining mention is not sufficient.
    if (read(root, "src/main.cpp").count(BRAND) != 2 or
            read(root, "webadmin/index.html").count(BRAND) != 2):
        raise ValueError("Incomplete C++ operator or WebAdmin page rebranding")
    require(read(root, "src/adc/user_list.cpp"),
            ('"NIdc24h.eu-twilight"', '"DEDirect Connect hub"'),
            "ADC wire-visible hub identity")
    require(read(root, "webadmin/src/languages/pl/index.ts"),
            ('footer: "Dla dc24h.eu-twilight"',), "PL WebAdmin footer")
    require(read(root, "webadmin/src/languages/us/index.ts"),
            ('footer: "Built for dc24h.eu-twilight"',), "EN WebAdmin footer")

    api = read(root, "src/api/server.cpp")
    status = read(root, "webadmin/src/lib/status.ts")
    require(api, (BRAND, '\\"server\\":{\\"name\\":\\"' + BRAND), "C++ API server.name")
    if status.count('server.name !== "' + BRAND + '"') != 2:
        raise ValueError("Both WebAdmin v1 status adapters must reject the old name")
    if ('server.name !== "dc24h.eu"' in status or 'name: "dc24h.eu";' in status):
        raise ValueError("Legacy server.name must not be accepted")

    webauthn = read(root, "webadmin/server/remote-webauthn.ts")
    require(webauthn, ('rpName: "' + BRAND + '", rpID',
                       'parsed.hostname !== rpID',
                       'expectedOrigin: origin', 'expectedRPID: rpID'),
            "Passkey RP and Origin")
    if (webauthn.count("expectedOrigin: origin") != 2 or
            webauthn.count("expectedRPID: rpID") != 2 or
            read(root, "webadmin/server/remote-gateway.ts").count(
                "req.headers.origin !== options.origin") != 2):
        raise ValueError("An enrollment/login or remote gateway Origin/RP guard was removed")

    hub = read(root, "config/dc24h-twilight.service.example")
    require(hub, ("Conflicts=dc24h.service",
                  "AssertPathExists=/etc/dc24h/twilight-unit-migration-approved",
                  "User=dc24h", "Group=dc24h",
                  "WorkingDirectory=/var/lib/dc24h",
                  "ExecStart=/usr/local/bin/dc24h-twilight "),
            "Guarded systemd migration example")
    if "ExecStart=/usr/local/bin/dc24h " in hub:
        raise ValueError("Twilight unit executes legacy binary")
    if read(root, "config/apache2-webadmin-https.conf.example").count("ServerName dc24h.eu") != 2:
        raise ValueError("Preserved infrastructure hostname changed")
    require(read(root, "config/dc24h.sysusers.example"),
            ("u dc24h -",), "Preserved system user")

    package = json.loads(read(root, "webadmin/package.json"))
    lock = json.loads(read(root, "webadmin/package-lock.json"))
    if (package.get("name") != "dc24h-twilight-webadmin" or
            lock.get("name") != package["name"] or
            lock.get("packages", {}).get("", {}).get("name") != package["name"]):
        raise ValueError("Twilight npm manifest and lock name diverged")
    for workflow in WORKFLOWS:
        text = read(root, ".github/workflows/" + workflow)
        if text.count("github.repository == '" + REPO + "'") != EXPECTED_TWILIGHT_GUARDS[workflow]:
            raise ValueError("Hosted CI no longer permits only reviewed Twilight repo identity")
        guards = re.findall(r"github\.repository == '([^']+)'", text)
        if any(name not in (REPO, OLD_REPO, "adrianf87/dc24h.eu") for name in guards):
            raise ValueError("Unreviewed foreign repository activated in CI")

    readme = read(root, "README.md")
    require(readme, ("# " + BRAND, "derived from dc24h.eu High Starter",
                     "<summary>Beta</summary>", "<summary>Alpha</summary>",
                     "github.com/" + OLD_REPO),
            "Current branding plus historical source provenance")
    # Historical project names, stable protocol paths, service accounts,
    # MariaDB schema and public DNS are explicitly excluded from replacement.
    return {
        "schema": 1, "source_sha": sha, "os_image": os_image,
        "stage": "twilight-stage-vi-static-preflight",
        "active_display_files_checked": len(ACTIVE_DISPLAY),
        "github_workflows_checked": len(WORKFLOWS),
        "active_branding_and_api_name_strict": True,
        "passkey_origin_rpid_guards_present": True,
        "legacy_systemd_conflict_and_marker_present": True,
        "stable_dns_service_user_and_data_paths_preserved": True,
        "npm_and_repository_identity_checked": True,
        "historical_high_starter_provenance_retained": True,
        "complete_final_sha_github_ci_verified": False,
        "artifact_zips_independently_hashed": False,
        "stage_v_documentation_accepted": False,
        "owner_release_acceptance_recorded": False,
        "release_tag_or_production_deployment_authorized": False,
        "status": "NO-GO",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", required=True, type=Path)
    parser.add_argument("--sha", required=True)
    parser.add_argument("--os-image", required=True)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    actual = subprocess.check_output(
        ["git", "-C", str(args.root), "rev-parse", "HEAD"], timeout=10).decode().strip()
    if actual != args.sha:
        raise ValueError("Source tree does not match the exact GitHub checkout head")
    report = evaluate(args.root, args.sha, args.os_image)
    with args.output.open("x", encoding="utf8") as out:
        json.dump(report, out, sort_keys=True)
        out.write("\n")


if __name__ == "__main__":
    main()
