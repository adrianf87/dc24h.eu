#!/usr/bin/env python3
"""Negative and read-only guarantees for the Debian 13 Stage II inventory."""

import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "public_stage2_preflight", ROOT / "scripts/public_stage2_preflight.py")
preflight = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(preflight)


def good_report():
    return {
        "source": {"matches_expected": True},
        "host": {"os_id": "debian", "os_major": "13",
                 "systemd_pid1": True, "systemd_manager": True},
        "services": {name: {"active": "inactive"} for name in preflight.SERVICES},
        "processes": {name: 0 for name in preflight.HUB_PROCESSES},
        "paths": {name: {"type": "missing"} for name in preflight.STABLE_PATHS},
        "tools": {name: True for name in preflight.REQUIRED_TOOLS},
        "guards": {"operator_marker_exists": False,
                   "template_contract_valid": True},
    }


class PublicStage2PreflightTests(unittest.TestCase):
    def test_read_only_inventory_never_authorizes_activation(self):
        result = preflight.assess(good_report())
        self.assertEqual(result["preflight"], "INVENTORY_CLEAR")
        self.assertEqual(result["findings"], [])
        for key in ("production_go", "private_state_migration_go", "public_listener_go"):
            self.assertIs(result[key], False)

    def test_wrong_source_or_distribution_fail_closed(self):
        for changed, code in (
            ({"source": {"matches_expected": False}}, "SOURCE_SHA_MISMATCH"),
            ({"host": {"os_id": "ubuntu", "os_major": "24",
                       "systemd_pid1": True, "systemd_manager": True}}, "DEBIAN_13_REQUIRED"),
        ):
            with self.subTest(code=code):
                report = good_report()
                report.update(changed)
                self.assertIn(code, preflight.assess(report)["findings"])

    def test_unavailable_systemd_manager_is_blocking(self):
        report = good_report()
        report["host"]["systemd_manager"] = False
        self.assertIn("SYSTEMD_MANAGER_UNAVAILABLE", preflight.assess(report)["findings"])

    def test_live_old_or_new_service_blocks_inventory(self):
        for name in preflight.SERVICES:
            with self.subTest(service=name):
                report = good_report()
                report["services"][name]["active"] = "active"
                result = preflight.assess(report)
                self.assertIn("LIVE_HUB_SERVICE_PRESENT:" + name, result["findings"])
                self.assertEqual(result["preflight"], "BLOCKED")

    def test_unknown_service_or_existing_process_blocks_inventory(self):
        report = good_report()
        report["services"]["dc24h.service"]["active"] = "unknown"
        report["processes"]["dc24h-twilight"] = 1
        problems = preflight.assess(report)["findings"]
        self.assertIn("SERVICE_STATUS_UNKNOWN:dc24h.service", problems)
        self.assertIn("HUB_PROCESS_PRESENT_OR_UNVERIFIED:dc24h-twilight", problems)

    def test_missing_marker_is_safe_but_existing_marker_requires_review(self):
        report = good_report()
        self.assertEqual(preflight.assess(report)["preflight"], "INVENTORY_CLEAR")
        report = good_report()
        report["guards"]["operator_marker_exists"] = True
        self.assertIn("EXISTING_MARKER_REQUIRES_OWNER_REVIEW",
                      preflight.assess(report)["findings"])

    def test_wrong_unit_contract_and_missing_tools_fail_closed(self):
        report = good_report()
        report["guards"]["template_contract_valid"] = False
        report["tools"]["dpkg"] = False
        problems = preflight.assess(report)["findings"]
        self.assertIn("UNIT_OR_STATUS_CONTRACT_MISMATCH", problems)
        self.assertIn("REQUIRED_TOOL_MISSING", problems)

    def test_symlink_is_reported_without_following_private_target(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            private = root / "private"
            private.write_text("PRIVATE SECRET", encoding="utf-8")
            link = root / "link"
            link.symlink_to(private)
            meta = preflight.path_metadata(str(link))
            self.assertEqual(meta["type"], "symlink")
            self.assertNotIn("PRIVATE SECRET", json.dumps(meta))
        report = good_report()
        report["paths"]["/etc/dc24h"] = {"type": "symlink"}
        self.assertIn("SYMLINK_REQUIRES_REVIEW:/etc/dc24h",
                      preflight.assess(report)["findings"])

    def test_network_inventory_redacts_raw_addresses(self):
        fake = json.dumps([
            {"ifname": "lo", "addr_info": [{"family": "inet", "local": "127.0.0.1"}]},
            {"ifname": "ens3", "addr_info": [
                {"family": "inet", "local": "198.51.100.4"},
                {"family": "inet6", "local": "2001:db8::1"}]},
        ])
        with mock.patch.object(preflight, "command", return_value=fake):
            result = preflight.network_summary()
        self.assertEqual(result["nonloopback_ipv4"], 1)
        self.assertEqual(result["nonloopback_ipv6"], 1)
        self.assertNotIn("198.51.100.4", json.dumps(result))
        self.assertNotIn("2001:db8::1", json.dumps(result))
        self.assertIs(result["public_ipv4_identity_verified"], False)

    def test_shipped_service_and_webadmin_contract(self):
        self.assertTrue(preflight.template_contract(ROOT))


if __name__ == "__main__":
    unittest.main()
