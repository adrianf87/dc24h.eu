#!/usr/bin/env python3
"""Lock prior beta-0.3 safety limits and current beta-0.9.20 metadata."""

from pathlib import Path
import re
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
    """The retired VPS runner must be absent from all executable CI paths."""
    retired = (
        "self-hosted-runner-debian13",
        ".github/workflows/debian13-supplemental.yml",
        ".github/workflows/debian13-stage2-sudo.yml",
        ".github/workflows/debian13-stage3-adcs-preflight.yml",
        ".github/workflows/debian13-stage3-adcs-activation.yml",
        "scripts/public_stage3_adcs_preflight.py",
        "scripts/public_stage3_adcs_activate.py",
        "scripts/public_stage3_qualification_host.py",
        "scripts/stage3_premerge_evidence.py",
    )
    for path in retired:
        require(not (ROOT / path).exists(), f"Retired runner code still exists: {path}", errors)

    for path in (ROOT / ".github" / "workflows").glob("*.yml"):
        content = path.read_text(encoding="utf-8")
        require(not re.search(r"runs-on:\s*(?:\[self-hosted|self-hosted)", content),
                f"{path.name}: self-hosted runner selected", errors)
        for prohibited in ("twilight-debian13-stage2", "github-runner",
                           "self-hosted-runner-debian13"):
            require(prohibited not in content,
                    f"{path.name}: retired worker reference {prohibited}", errors)

    for path in (ROOT / "README.md", *(ROOT / "doc").rglob("*.md")):
        content = path.read_text(encoding="utf-8")
        require("self-hosted-runner-debian13" not in content and
                "github-runner" not in content,
                f"{path.relative_to(ROOT)}: retired worker documentation", errors)

    workflow_dir = ROOT / ".github" / "workflows"
    automatic = (workflow_dir / "public-stage3-qualification.yml").read_text(encoding="utf-8")
    manual = (workflow_dir / "debian13-stage3-full-qualification.yml").read_text(encoding="utf-8")
    for workflow, entries in (
        (automatic, ("  pull_request:", "  push:", "os: [ubuntu-22.04, ubuntu-24.04, ubuntu-26.04]",
                     "runs-on: ${{ matrix.os }}", "sudo -n id -u", "group: [two-client, tls-fixtures, stability]",
                     "needs: [offline, sandbox, loopback]")),
        (manual, ("  workflow_dispatch:", "RUN_STAGE3_PUBLIC_EXTENDED_STABILITY",
                  "stage3.run run extended", "inputs.production_restarts_ack == false",
                  "bash .github/scripts/install-ci-deps.sh ncdc")),
    ):
        for required in (*entries, "contents: read",
                         "test \"$RUNNER_ENVIRONMENT\" = github-hosted"):
            require(required in workflow, f"Stage III workflow missing {required!r}", errors)
        for prohibited in ("systemctl restart", "sudo -n python3", "premerge-host:",
                           "RUN_STAGE3_PUBLIC_QUALIFICATION", "self-hosted", "ssh "):
            require(prohibited not in workflow, f"Stage III workflow attempts {prohibited!r}", errors)
    for public_group in ("tls-public", "exposure", "two-client-public", "stage3.run run ncdc"):
        require(public_group not in automatic, f"Automatic Stage III workflow includes {public_group!r}", errors)
    require(not (ROOT / ".github" / "actionlint.yaml").exists(),
            "Retired custom-runner actionlint shim remains", errors)


def check_ci_repository_allowlists(errors):
    old = "github.repository == 'adrianf87/dc24h.eu-high-starter'"
    twilight = "github.repository == 'adrianf87/dc24h.eu-twilight'"
    original = "github.repository == 'adrianf87/dc24h.eu'"
    policies = {
        "build.yml": (2, {old, twilight, original}),
        "beta07-codeql.yml": (1, {old, twilight}),
        "beta08-hardening.yml": (2, {old, twilight}),
        "beta09-stage4-storage.yml": (1, {old, twilight}),
        "stage4-ubuntu-vm-validation.yml": (1, {old, twilight}),
    }
    for name, (count, allowlist) in policies.items():
        content = (ROOT / ".github/workflows" / name).read_text(encoding="utf-8")
        guards = re.findall(r"(?m)^\s*if: (.*github\.repository.*)$", content)
        require(len(guards) == count, f"{name}: expected {count} protected jobs", errors)
        for guard in guards:
            clauses = guard.strip().split(" || ")
            require(len(clauses) == len(allowlist) and set(clauses) == allowlist,
                    f"{name}: foreign repo or altered exact repository allowlist", errors)
    require(sum(count for count, _ in policies.values()) == 7,
            "Unexpected repository guard inventory", errors)


def check_ci_apt_source_policy(errors):
    """Runner-supplied third-party APT outages cannot disable Ubuntu package tests."""
    script = check(ROOT / ".github/scripts/install-ci-deps.sh", (
        'GITHUB_ACTIONS:-',
        'RUNNER_ENVIRONMENT:-',
        '/etc/apt/sources.list.d/ubuntu.sources',
        '/etc/apt/sources.list',
        'Dir::Etc::sourcelist=',
        'Dir::Etc::sourceparts=-',
        'apt-get "${apt_network_options[@]}" update',
        'apt-get "${apt_network_options[@]}" install -y --no-install-recommends',
    ), errors)
    for unsafe in ('--allow-unauthenticated', 'Acquire::AllowInsecureRepositories',
                   'APT::Get::AllowUnauthenticated', '|| true'):
        require(unsafe not in script, f"APT policy weakens failure or signature checks: {unsafe}", errors)


def main():
    errors = []
    cmake = check(ROOT / "CMakeLists.txt", (
        "project(dc24h VERSION 0.9.20 LANGUAGES CXX)",
        'set(DC24H_VERSION "beta-0.9.20")',
        'add_executable(dc24h-service-controller',
        'service.controller.process',
        "add_test(NAME beta.baseline ",
    ), errors)
    require('set(DC24H_VERSION "alpha-' not in cmake, "CMakeLists.txt: alpha version string remains", errors)
    check(ROOT / "cmake/version.hpp.in", ('inline constexpr std::string_view version = "@DC24H_VERSION@";',), errors)
    workflow = check(ROOT / ".github/workflows/build.yml", (
        "runs-on: ${{ matrix.os }}",
        "os: [ubuntu-22.04, ubuntu-24.04, ubuntu-26.04]",
        'sanitizer: [address, undefined]',
        'npm audit --omit=dev --audit-level=high',
        "feature/stage4-webadmin-dedicated-service",
        "pull_request:",
        "name: CI (C++ and WebAdmin)",
    ), errors)
    require(workflow.count("runs-on: ${{ matrix.os }}") == 2 and
            workflow.count("runs-on: ubuntu-22.04") == 1,
            "build.yml: application and sanitizers must test all three Ubuntu versions", errors)
    require('test "$(nproc)" -eq 4' not in workflow,
            "build.yml: fixed runner CPU count is not portable", errors)
    check_ci_repository_allowlists(errors)
    check_ci_apt_source_policy(errors)
    require("needs: [application, sanitizers]" in workflow,
            "build.yml: sanitizer failures must block the required CI result", errors)
    require("debian_compat:" not in workflow and "container:" not in workflow,
            "build.yml: Debian container tests are disabled by owner instruction", errors)
    require("server-747585786" not in workflow, "build.yml: custom runner is selected", errors)
    require("self-hosted" not in workflow, "build.yml: self-hosted runner is selected", errors)
    require("DC24H_REQUIRE_SYSTEMD_HOST: '1'" in workflow,
            "build.yml: real systemd manager must be required for Ubuntu integration", errors)
    systemd_test = (ROOT / "tests/systemd_sandbox_tests.py").read_text(encoding="utf-8")
    require('os.environ.get("DC24H_REQUIRE_SYSTEMD_HOST") == "1"' in systemd_test and
            '"--require-systemd-host"' in systemd_test and 'if REQUIRE_SYSTEMD_HOST:' in systemd_test,
            "systemd.sandbox: missing fail-closed manager enforcement", errors)
    require("ubuntu-latest" not in workflow, "build.yml: ubuntu-latest is selected", errors)
    check(ROOT / "README.md", (
        "# dc24h.eu-twilight",
        "Current code version: beta-0.9.20",
        "High Starter v0.9.20-rc.1 pre-release",
        "No Twilight release or production GO",
        "beta-0.3.11–beta-0.3.12 Stage IV",
        "beta-0.3.8–beta-0.3.10 Stage III",
        "beta-0.3.4–beta-0.3.7 Stage II",
        "beta-0.2.7–beta-0.2.12 Consolidated hardening",
        "beta-0.2.6 Status projection privacy",
        "beta-0.2.5 Apache2/HTTPS/loopback boundary",
        "beta-0.2.4 Sessions and CSRF",
        "beta-0.2.3 Authentication and revalidation",
        "beta-0.2.2 HTTP request hardening",
        "beta-0.2.1 Maintenance baseline",
        "beta-0.2 Remote WebAdmin",
        "beta-0.1.8 Development baseline",
        "<summary>Alpha 0.9.x (17 releases)</summary>",
    ), errors)
    check(ROOT / "doc/README.md", ("Current Twilight code:", "beta-0.9.20",
                                   "No Twilight release or production GO"), errors)
    check(ROOT / "doc/us/BETA.md", (
        "# Combined hardening release — beta-0.2.7 to beta-0.2.12",
        "## beta-0.2.9 — private API response hardening",
        "## beta-0.2.12 — final verification boundary",
        "# Minimal status projection and API failures — beta-0.2.6",
        "# Apache2, HTTPS and loopback boundary — beta-0.2.5",
        "# Session lifetime, rotation and CSRF — beta-0.2.4",
        "# Authentication and live authorization — beta-0.2.3",
        "# HTTP request allowlist hardening — beta-0.2.2",
        "`beta-0.2-code`",
        "# Maintenance baseline — beta-0.2.1",
        "Branch `beta-0.2.1` starts from protected `main`",
        "# Remote WebAdmin read-only gateway — beta-0.2",
        "# Development baseline — beta-0.1.8",
        "Branch `beta-0.1.8` starts from protected main `cebcd12`.",
        "# Hub service controls in WebAdmin — beta-0.1.7",
        "# Read-only service controller — beta-0.1.5",
        "Branch `beta-0.1.5` starts from main `e8d00ae`.",
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
        "# Zbiorcze wydanie poprawek — beta-0.2.7 do beta-0.2.12",
        "## beta-0.2.9 — odpowiedzi prywatnego API",
        "## beta-0.2.12 — końcowa granica weryfikacji",
        "# Minimalizacja statusu i błędy API — beta-0.2.6",
        "# Apache2, HTTPS i granica loopback — beta-0.2.5",
        "# Czas życia sesji, rotacja i CSRF — beta-0.2.4",
        "# Uwierzytelnianie i bieżąca autoryzacja — beta-0.2.3",
        "# Uszczelnienie walidacji HTTP — beta-0.2.2",
        "`beta-0.2-code`",
        "# Wersja bazowa utrzymania — beta-0.2.1",
        "Gałąź `beta-0.2.1` powstaje z chronionego `main`",
        "# Lokalna brama WebAdmin tylko do odczytu — beta-0.2",
        "# Wersja bazowa rozwoju — beta-0.1.8",
        "Gałąź `beta-0.1.8` powstaje z chronionego main `cebcd12`.",
        "# Sterowanie usługą huba w WebAdmin — beta-0.1.7",
        "# Kontroler usługi tylko do odczytu — beta-0.1.5",
        "Gałąź `beta-0.1.5` powstaje z main `e8d00ae`.",
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
    check(ROOT / "src/service_control/controller.cpp", (
        "SO_PEERCRED", "STATUS\\n", "systemctl", "dc24h-twilight.service", "execve", "O_NOFOLLOW",
        '"START\\n"', '"STOP\\n"', '"RESTART\\n"', 'dc24h-maintenance',
    ), errors)
    check(ROOT / "src/api/accounts.cpp", (
        '"/api/v1/management/hub/start"', '"/api/v1/management/hub/stop"',
        '"/api/v1/management/hub/restart"', '"/api/v1/management/hub/status"', 'confirm', 'begin_service', 'finish_service',
        'SO_PEERCRED', 'dc24h-maintenance',
    ), errors)
    check(ROOT / "doc/us/SERVICE_OPERATIONS.md", (
        'beta-0.1.6', 'Owner', 'started', 'outcome_unknown', 'Polkit',
    ), errors)
    check(ROOT / "doc/pl/SERVICE_OPERATIONS.md", (
        'beta-0.1.6', 'Owner', 'started', 'outcome_unknown', 'Polkit',
    ), errors)
    check(ROOT / "doc/us/SERVICE_CONTROLLER.md", (
        "beta-0.1.5", "SO_PEERCRED", "systemd_unavailable", "beta-0.1.6",
    ), errors)
    check(ROOT / "doc/pl/SERVICE_CONTROLLER.md", (
        "beta-0.1.5", "SO_PEERCRED", "systemd_unavailable", "beta-0.1.6",
    ), errors)
    check(ROOT / "config/apache2-webadmin-https-proxy.conf.example", (
        "management(?:/authorize|/configuration",
        "management/authorize",
    ), errors)
    check(ROOT / "webadmin/server/account-api.ts", (
        "/admin-api/v1/management/authorize",
        "not_implemented",
        "parseConfigurationReview", "parseConfigurationValidation", "parseConfigurationWrite",
        "parseHubServiceStatus", "parseHubServiceResult",
    ), errors)
    check(ROOT / "webadmin/src/components/ConfigurationEditor.tsx", (
        "configurationChanges", "configEditConfirm", "configEditRestoreConfirm", "adminHeaders()",
    ), errors)
    check(ROOT / "include/dc24h/admin/access.hpp", (
        "inline constexpr std::array<RouteTemplate, 12> access_routes{{",
        "Owner-only fixed hub status/action",
    ), errors)
    check(ROOT / "webadmin/src/lib/status.ts", (
        'export const WEBADMIN_VERSION = "beta-0.9.20";',
        '!/^(?:alpha-\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}|beta-\\d{1,3}\\.\\d{1,3}(?:\\.\\d{1,3})?)$/.test(server.version)',
    ), errors)
    check(ROOT / "src/security/roles.cpp", (
        "return p!=Permission::manage_configuration;",
    ), errors)
    check(ROOT / "webadmin/src/components/HubService.tsx", (
        "hubServiceConfirm", "hubServiceUnknown", "hubServiceVerifying", "serviceConfirmation", "adminHeaders()",
    ), errors)
    check(ROOT / "webadmin/src/lib/hub-service.ts", (
        "parseHubServiceStatus", "parseHubServiceResult", '"dc24h-twilight.service"',
    ), errors)
    check(ROOT / "webadmin/server/json-content-type.ts", (
        "isJsonContentType", "application\\/json", "charset", "utf-8",
    ), errors)
    for adapter in ("local-api.ts", "account-api.ts"):
        check(ROOT / "webadmin/server" / adapter, (
            "isJsonContentType(response.headers[\"content-type\"])",
            "assembled", "scrub", 'response.on("close",',
        ), errors)
    check(ROOT / "doc/us/HUB_WEBADMIN.md", ("beta-0.1.7", "Owner", "outcome_unknown", "Apache"), errors)
    check(ROOT / "doc/pl/HUB_WEBADMIN.md", ("beta-0.1.7", "Owner", "outcome_unknown", "Apache"), errors)
    check(ROOT / "doc/us/REMOTE_GATEWAY.md", ("beta-0.2", "local-only", "MFA", "CSRF", "administrator"), errors)
    check(ROOT / "doc/pl/REMOTE_GATEWAY.md", ("beta-0.2", "lokalny", "MFA", "CSRF", "administrator"), errors)
    check(ROOT / "doc/us/REMOTE_PILOT.md", ("beta-0.3.10", "127.0.0.1:8443", "ssh -N -T", "--ca-file"), errors)
    check(ROOT / "doc/pl/REMOTE_PILOT.md", ("beta-0.3.10", "127.0.0.1:8443", "ssh -N -T", "--ca-file"), errors)
    check(ROOT / "doc/us/RELEASE_BETA_0.3.md", ("beta-0.3.12", "CI", "MFA", "rollback"), errors)
    check(ROOT / "doc/pl/RELEASE_BETA_0.3.md", ("beta-0.3.12", "CI", "MFA", "wycofanie"), errors)
    check(ROOT / "scripts/remote_pilot_check.py", ("ssl.create_default_context", '"localhost", 8443', "CERT_REQUIRED"), errors)
    check(ROOT / "config/apache2-webadmin-remote.conf.example", ("SSLHonorCipherOrder On", "SSLCipherSuite", "Listen 127.0.0.1:8443"), errors)
    check(ROOT / "CMakeLists.txt", ("remote.pilot.preflight",), errors)
    check(ROOT / "webadmin/server/remote-main.ts", ("loadTotpVerifier", "mfaVerify: mfa.verify", "127.0.0.1"), errors)
    check(ROOT / "webadmin/server/remote-mfa.ts", ("timingSafeEqual", "fsyncSync", "openPrivateLeaf", "lastUsed"), errors)
    check(ROOT / "webadmin/server/remote-private-path.ts", ("O_NOFOLLOW", "openPrivateLeaf"), errors)
    check(ROOT / "webadmin/src/remote/RemoteApp.tsx", ("s.mfa === true", 'name="otp"'), errors)
    check(ROOT / "tests/database_migrations_tests.cpp", (
        "require(migration_catalog().size() == 11 && entries[0].info.version == 1 && entries[1].info.version == 2);",
        'require(entries[8].info.version == 9 && entries[8].info.name == "audit_events");',
        'require(entries[9].info.version == 10 && entries[9].info.name == "configuration_audit");',
        'require(entries[10].info.version == 11 && entries[10].info.name == "hub_service_audit");',
    ), errors)
    check_no_custom_workers(errors)
    if errors:
        print("\n".join(errors), file=sys.stderr)
        return 1
    print("Beta 0.3 safety baseline and beta 0.9.20 candidate metadata verified")
    return 0


if __name__ == "__main__":
    sys.exit(main())
