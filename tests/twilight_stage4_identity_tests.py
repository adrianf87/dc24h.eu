#!/usr/bin/env python3
"""Source-only regressions for OWNER-approved technical rebranding (no host writes)."""

import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


def source(name):
    return (ROOT / name).read_text(encoding="utf-8")


def require_service_contract(text, legacy, executable, marker=False):
    if "Conflicts=" + legacy not in text:
        raise ValueError("Missing legacy unit conflict")
    if executable not in text:
        raise ValueError("Missing new executable")
    if marker and "\nAssertPathExists=/etc/dc24h/twilight-unit-migration-approved\n" not in text:
        raise ValueError("Missing operator approval marker")


class StageIVTechnicalIdentityTests(unittest.TestCase):
    def test_cmake_keeps_internal_targets_but_renames_all_output_files(self):
        cmake = source("CMakeLists.txt")
        for target, output in (
            ("dc24h", "dc24h-twilight"),
            ("dc24h-owner", "dc24h-twilight-owner"),
            ("dc24h-account-admin", "dc24h-twilight-account-admin"),
            ("dc24h-service-controller", "dc24h-twilight-service-controller"),
        ):
            self.assertIn("add_executable(" + target + " ", cmake)
            self.assertIn("set_target_properties(" + target + " PROPERTIES OUTPUT_NAME " + output + ")", cmake)
        self.assertIn("project(dc24h VERSION 0.9.20", cmake)
        self.assertNotIn("project(dc24h-twilight", cmake)

    def test_units_fail_closed_and_preserve_accounts_and_state_paths(self):
        hub = source("config/dc24h-twilight.service.example")
        require_service_contract(hub, "dc24h.service", "ExecStart=/usr/local/bin/dc24h-twilight ", True)
        for name in ("User=dc24h", "Group=dc24h", "WorkingDirectory=/var/lib/dc24h",
                     "StateDirectory=dc24h", "ConfigurationDirectory=dc24h"):
            self.assertIn(name, hub)
        controller = source("config/dc24h-twilight-control.service.example")
        require_service_contract(controller, "dc24h-control.service", "dc24h-twilight-service-controller")
        self.assertIn("User=dc24h-control", controller)
        self.assertIn("Group=dc24h-admin", controller)
        remote = source("config/dc24h-twilight-remote-gateway.service.example")
        self.assertIn("Conflicts=dc24h-remote-gateway.service", remote)
        self.assertIn("User=dc24h-maintenance", remote)
        self.assertIn("Environment=DC24H_REMOTE_RP_ID=localhost", remote)
        self.assertIn("WorkingDirectory=/opt/dc24h/webadmin", remote)
        for old in ("dc24h.service.example", "dc24h-control.service.example",
                    "dc24h-remote-gateway.service.example"):
            self.assertFalse((ROOT / "config" / old).exists())

    def test_systemd_control_contract_rejects_old_unit_identity(self):
        for filename in ("src/service_control/controller.cpp", "config/49-dc24h-control.rules.example",
                         "webadmin/src/lib/hub-service.ts"):
            content = source(filename)
            self.assertIn('"dc24h-twilight.service"', content)
            self.assertNotIn('"dc24h.service"', content)
        self.assertIn('unit: "dc24h.service"', source("webadmin/tests/hub-service.test.ts"))
        self.assertIn("assert.throws(() => parseHubServiceStatus(wrong))",
                      source("webadmin/tests/hub-service.test.ts"))
        self.assertIn("dc24h-twilight.service", source("src/api/accounts.cpp"))

    def test_npm_package_manifest_matches_lockfile(self):
        manifest = json.loads(source("webadmin/package.json"))
        lock = json.loads(source("webadmin/package-lock.json"))
        for name in (manifest["name"], lock["name"], lock["packages"][""]["name"]):
            self.assertEqual(name, "dc24h-twilight-webadmin")
        self.assertEqual(manifest["version"], lock["packages"][""]["version"])

    def test_ci_package_distinguishes_legacy_and_new_payload(self):
        fixture = source("scripts/beta07_ci_deb.py")
        self.assertIn('LEGACY_PACKAGE = "dc24h-high-starter-ci-fixture"', fixture)
        self.assertIn('PACKAGE = "dc24h-twilight-ci-fixture"', fixture)
        self.assertIn("opposite_name = PACKAGE if legacy else LEGACY_PACKAGE", fixture)
        self.assertIn("Conflicts: {opposite_name}", fixture)
        self.assertNotIn("Replaces: {opposite_name}", fixture)
        self.assertIn("LEGACY_UNIT", fixture)
        self.assertIn("ConditionPathExists=/__dc24h_ci_fixture_never_start__", fixture)
        workflow = source(".github/workflows/stage4-ubuntu-vm-validation.yml")
        self.assertIn("build/Stage4VM/dc24h-twilight", workflow)
        self.assertIn("stage4_ubuntu_vm_probe_tests.py", workflow)
        self.assertIn("runs-on: ubuntu-22.04", workflow)
        self.assertNotIn("self-hosted", workflow)

    def test_status_api_dns_origin_passkey_and_sysusers_are_stable(self):
        self.assertIn(r'\"name\":\"dc24h.eu-twilight\"', source("src/api/server.cpp"))
        self.assertEqual(source("webadmin/src/lib/status.ts").count(
            'server.name !== "dc24h.eu-twilight"'), 2)
        self.assertIn('rpName: "dc24h.eu-twilight", rpID',
                      source("webadmin/server/remote-webauthn.ts"))
        self.assertIn("ServerName dc24h.eu", source("config/apache2-webadmin-https.conf.example"))
        self.assertIn("u dc24h -", source("config/dc24h.sysusers.example"))

    def test_missing_conflict_or_operator_marker_is_rejected(self):
        hub = source("config/dc24h-twilight.service.example")
        with self.assertRaisesRegex(ValueError, "conflict"):
            require_service_contract(hub.replace("Conflicts=dc24h.service", ""), "dc24h.service",
                                     "ExecStart=/usr/local/bin/dc24h-twilight ", True)
        with self.assertRaisesRegex(ValueError, "approval"):
            require_service_contract(hub.replace("\nAssertPathExists=", "\n# AssertPathExists="),
                                     "dc24h.service", "ExecStart=/usr/local/bin/dc24h-twilight ", True)


if __name__ == "__main__":
    unittest.main()
