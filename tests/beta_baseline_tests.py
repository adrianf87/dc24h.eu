#!/usr/bin/env python3
"""Lock the beta-0.1 release string and the unchanged alpha-0.9.17 limits."""

from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[1]


def require(condition, message, errors):
    if not condition:
        errors.append(message)


def check(path, phrases, errors):
    text = path.read_text(encoding="utf-8")
    if "\ufffd" in text or text.startswith("\ufeff"):
        errors.append(f"{path.name}: replacement character or UTF-8 BOM")
    for phrase in phrases:
        require(phrase in text, f"{path.name}: missing {phrase!r}", errors)
    return text


def check_no_custom_workers(errors):
    needles = (
        "contabo",
        "hetzner",
        "747585786",
        "proxmox",
        "github-runner",
        "ci-controller",
        "custom-runner",
        "custom runner",
        "self-hosted",
        "własnego runnera",
        "własny runner",
        "własnym runner",
    )
    paths = [ROOT / "README.md", ROOT / "CHANGELOG.md"]
    paths.extend((ROOT / "doc").rglob("*.md"))
    paths.extend((ROOT / ".github" / "workflows").glob("*.yml"))
    for path in paths:
        folded = path.read_text(encoding="utf-8").casefold()
        relative = path.relative_to(ROOT)
        for needle in needles:
            require(needle.casefold() not in folded,
                    f"{relative}: custom worker term {needle!r}", errors)
    actionlint = (ROOT / ".github" / "actionlint.yaml").read_text(encoding="utf-8")
    require("server-747585786" not in actionlint,
            "actionlint.yaml: custom runner label remains", errors)
    require("ubuntu-26.04" in actionlint,
            "actionlint.yaml: public ubuntu-26.04 label is missing", errors)


def main():
    errors = []
    cmake = check(ROOT / "CMakeLists.txt", (
        "project(dc24h VERSION 0.1.0 LANGUAGES CXX)",
        'set(DC24H_VERSION "beta-0.1")',
        "add_test(NAME beta.baseline ",
    ), errors)
    require('set(DC24H_VERSION "alpha-' not in cmake, "CMakeLists.txt: alpha version string remains", errors)
    check(ROOT / "cmake/version.hpp.in", ('inline constexpr std::string_view version = "@DC24H_VERSION@";',), errors)
    workflow = check(ROOT / ".github/workflows/build.yml", (
        "os: [ubuntu-22.04, ubuntu-24.04, ubuntu-26.04]",
        "runs-on: ${{ matrix.os }}",
        'test "$(nproc)" -eq 4',
        "beta-0.1",
    ), errors)
    require(workflow.count("os: [ubuntu-22.04, ubuntu-24.04, ubuntu-26.04]") == 2,
            "build.yml: both jobs must list the three public images", errors)
    require(workflow.count("runs-on: ${{ matrix.os }}") == 2,
            "build.yml: both jobs must use the public image matrix", errors)
    require(workflow.count('test "$(nproc)" -eq 4') == 2,
            "build.yml: both jobs must require four logical CPUs", errors)
    require("server-747585786" not in workflow, "build.yml: custom runner is selected", errors)
    require("self-hosted" not in workflow, "build.yml: self-hosted runner is selected", errors)
    require("ubuntu-latest" not in workflow, "build.yml: ubuntu-latest is selected", errors)
    check(ROOT / "doc/us/BETA.md", (
        "# Beta baseline — beta-0.1",
        "The branch starts from main `423a958`.",
        "The branch name is `beta-0.1`.",
        "The hub, the API and WebAdmin report `beta-0.1`.",
        "Schema v9 is unchanged.",
        "`configuration.manage` and `hub.manage` stay unimplemented",
        "This is not a production release.",
        "No residual threat is closed.",
        "public GitHub-hosted runners `ubuntu-22.04`, `ubuntu-24.04` and `ubuntu-26.04`",
        "four logical CPUs and 16 GB",
    ), errors)
    check(ROOT / "doc/pl/BETA.md", (
        "# Linia beta — beta-0.1",
        "Gałąź powstaje z main `423a958`.",
        "Nazwa gałęzi to `beta-0.1`.",
        "Hub, API i WebAdmin zgłaszają `beta-0.1`.",
        "Schemat v9 pozostaje bez zmian.",
        "`configuration.manage` i `hub.manage` pozostają niezaimplementowane",
        "To nie jest wydanie produkcyjne.",
        "Żadne zagrożenie szczątkowe nie jest zamknięte.",
        "publicznych runnerach GitHub-hosted `ubuntu-22.04`, `ubuntu-24.04` i `ubuntu-26.04`",
        "cztery logiczne CPU i 16 GB",
    ), errors)
    check(ROOT / "include/dc24h/admin/access.hpp", (
        "inline constexpr std::array<RouteTemplate, 12> access_routes{{",
        "configuration.manage and hub.manage stay",
    ), errors)
    check(ROOT / "webadmin/src/lib/status.ts", (
        'export const WEBADMIN_VERSION = "beta-0.1";',
        '!/^(?:alpha-\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}|beta-\\d{1,3}\\.\\d{1,3}(?:\\.\\d{1,3})?)$/.test(server.version)',
    ), errors)
    check(ROOT / "src/security/roles.cpp", (
        "return p!=Permission::manage_configuration&&p!=Permission::manage_hub;",
    ), errors)
    check(ROOT / "tests/database_migrations_tests.cpp", (
        "require(migration_catalog().size() == 9 && entries[0].info.version == 1 && entries[1].info.version == 2);",
        'require(entries[8].info.version == 9 && entries[8].info.name == "audit_events");',
    ), errors)
    check_no_custom_workers(errors)
    if errors:
        print("\n".join(errors), file=sys.stderr)
        return 1
    print("Beta 0.1 baseline, version string and unchanged limits passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
