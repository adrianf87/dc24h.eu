#!/usr/bin/env python3
"""Lock beta-0.1.4 editor metadata and established configuration safety limits."""

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
        'set(DC24H_VERSION "beta-0.1.4")',
        "add_test(NAME beta.baseline ",
    ), errors)
    require('set(DC24H_VERSION "alpha-' not in cmake, "CMakeLists.txt: alpha version string remains", errors)
    check(ROOT / "cmake/version.hpp.in", ('inline constexpr std::string_view version = "@DC24H_VERSION@";',), errors)
    workflow = check(ROOT / ".github/workflows/build.yml", (
        "os: [ubuntu-22.04, ubuntu-24.04, ubuntu-26.04]",
        "runs-on: ${{ matrix.os }}",
        'test "$(nproc)" -eq 4',
        "beta-0.1",
        "pull_request:",
        "name: CI (C++ and WebAdmin)",
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
        "# WebAdmin configuration editor — beta-0.1.4",
        "Branch `beta-0.1.4` starts from main `3f7f03d`.",
        "# Atomic configuration write — beta-0.1.3",
        "Branch `beta-0.1.3` starts from main `83b224c`.",
        "# Configuration review — beta-0.1.2",
        "The branch name is `beta-0.1.2`.",
        "# Management foundation — beta-0.1.1",
        "The branch name is `beta-0.1.1`.",
        "The hub, the API and WebAdmin report `beta-0.1.1`.",
        "main `075081f`",
        "The branch starts from main `423a958`.",
        "The branch name is `beta-0.1`.",
        "Schema v9 is unchanged.",
        "`configuration.manage` and `hub.manage` stay unimplemented",
        "This is not a production release.",
        "No residual threat is closed.",
        "public GitHub-hosted runners `ubuntu-22.04`, `ubuntu-24.04` and `ubuntu-26.04`",
        "four logical CPUs and 16 GB",
    ), errors)
    check(ROOT / "doc/pl/BETA.md", (
        "# Edytor konfiguracji WebAdmin — beta-0.1.4",
        "Gałąź `beta-0.1.4` powstaje z main `3f7f03d`.",
        "# Atomowy zapis konfiguracji — beta-0.1.3",
        "Gałąź `beta-0.1.3` powstaje z main `83b224c`.",
        "# Przegląd konfiguracji — beta-0.1.2",
        "Nazwa gałęzi to `beta-0.1.2`.",
        "# Fundament zarządzania — beta-0.1.1",
        "Nazwa gałęzi to `beta-0.1.1`.",
        "Hub, API i WebAdmin zgłaszają `beta-0.1.1`.",
        "main `075081f`",
        "Gałąź powstaje z main `423a958`.",
        "Nazwa gałęzi to `beta-0.1`.",
        "Schemat v9 pozostaje bez zmian.",
        "`configuration.manage` i `hub.manage` pozostają niezaimplementowane",
        "To nie jest wydanie produkcyjne.",
        "Żadne zagrożenie szczątkowe nie jest zamknięte.",
        "publicznych runnerach GitHub-hosted `ubuntu-22.04`, `ubuntu-24.04` i `ubuntu-26.04`",
        "cztery logiczne CPU i 16 GB",
    ), errors)
    check(ROOT / "src/api/accounts.cpp", (
        'schema\\":2',
        "configuration.read",
        "configuration.validate",
        "configuration.write",
        "configuration.restore",
        "hub.status",
        "hub.start",
        "hub.stop",
        "hub.restart",
        "not_implemented",
    ), errors)
    check(ROOT / "src/api/accounts.cpp", (
        '"/api/v1/management/configuration"',
        "configuration_review_request",
    ), errors)
    check(ROOT / "src/configuration/configuration.cpp", (
        "Review::read() const",
        "Review::validate_candidate",
        "HMAC(EVP_sha256()",
        "StaleReview()",
    ), errors)
    check(ROOT / "doc/us/CONFIGURATION_REVIEW.md", (
        "beta-0.1.2",
        "hub_active_available",
        "written",
    ), errors)
    check(ROOT / "doc/pl/CONFIGURATION_REVIEW.md", (
        "beta-0.1.2",
        "hub_active_available",
        "written",
    ), errors)
    check(ROOT / "doc/us/CONFIGURATION_WRITE.md", (
        "beta-0.1.3", "configuration.write", "outcome_unknown", ".dc24h.bak",
    ), errors)
    check(ROOT / "doc/pl/CONFIGURATION_WRITE.md", (
        "beta-0.1.3", "configuration.write", "outcome_unknown", ".dc24h.bak",
    ), errors)
    check(ROOT / "doc/us/CONFIGURATION_EDITOR.md", (
        "beta-0.1.4", "configuration.manage", "409 stale", "503 outcome_unknown", "Require local",
    ), errors)
    check(ROOT / "doc/pl/CONFIGURATION_EDITOR.md", (
        "beta-0.1.4", "configuration.manage", "409 stale", "503 outcome_unknown", "Require local",
    ), errors)
    check(ROOT / "config/apache2-webadmin-https-proxy.conf.example", (
        "management(?:/authorize|/configuration",
        "management/authorize",
    ), errors)
    check(ROOT / "webadmin/server/account-api.ts", (
        "/admin-api/v1/management/authorize",
        "not_implemented",
        "parseConfigurationReview", "parseConfigurationValidation", "parseConfigurationWrite",
    ), errors)
    check(ROOT / "webadmin/src/components/ConfigurationEditor.tsx", (
        "configurationChanges", "configEditConfirm", "configEditRestoreConfirm", "adminHeaders()",
    ), errors)
    check(ROOT / "include/dc24h/admin/access.hpp", (
        "inline constexpr std::array<RouteTemplate, 12> access_routes{{",
        "configuration.manage and hub.manage stay",
    ), errors)
    check(ROOT / "webadmin/src/lib/status.ts", (
        'export const WEBADMIN_VERSION = "beta-0.1.4";',
        '!/^(?:alpha-\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}|beta-\\d{1,3}\\.\\d{1,3}(?:\\.\\d{1,3})?)$/.test(server.version)',
    ), errors)
    check(ROOT / "src/security/roles.cpp", (
        "return p!=Permission::manage_configuration&&p!=Permission::manage_hub;",
    ), errors)
    check(ROOT / "tests/database_migrations_tests.cpp", (
        "require(migration_catalog().size() == 10 && entries[0].info.version == 1 && entries[1].info.version == 2);",
        'require(entries[8].info.version == 9 && entries[8].info.name == "audit_events");',
        'require(entries[9].info.version == 10 && entries[9].info.name == "configuration_audit");',
    ), errors)
    check_no_custom_workers(errors)
    if errors:
        print("\n".join(errors), file=sys.stderr)
        return 1
    print("Beta 0.1.4 editor, version and safety limits passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
