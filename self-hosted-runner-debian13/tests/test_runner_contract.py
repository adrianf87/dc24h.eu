#!/usr/bin/env python3
"""Fail-closed tests for the manually dispatched Debian 13 runner policy."""

from pathlib import Path
import re
import unittest

from runner_preflight import evaluate


ROOT = Path(__file__).resolve().parents[2]
WORKFLOW = ROOT / ".github/workflows/debian13-supplemental.yml"
SUDO_WORKFLOW = ROOT / ".github/workflows/debian13-stage2-sudo.yml"


def good_report():
    return {
        "host": {"os_id": "debian", "os_major": "13", "pid1": "systemd"},
        "runner": {"name": "server-747585786", "unix_user": "github-runner",
                   "euid_is_root": False, "actor": "adrianf87", "ref": "refs/heads/main"},
        "services": {"dc24h.service": "inactive", "dc24h-twilight.service": "inactive"},
        "processes": {"dc24h": 0, "dc24h-twilight": 0},
        "tools": {"python3": True, "systemctl": True},
    }


class RunnerContractTests(unittest.TestCase):
    def test_good_inventory_is_not_production_authorization(self):
        result = evaluate(good_report())
        self.assertEqual(result["result"], "INVENTORY_CLEAR")
        self.assertEqual(result["findings"], [])
        for key in ("production_go", "migration_go", "public_listener_go"):
            self.assertIs(result[key], False)

    def test_wrong_runner_and_privileges_fail_closed(self):
        report = good_report()
        report["runner"].update(name="unexpected-runner", euid_is_root=True)
        self.assertIn("WRONG_RUNNER_NAME", evaluate(report)["findings"])
        self.assertIn("UNEXPECTED_RUNNER_PRIVILEGES", report["findings"])

    def test_wrong_ref_or_actor_fail_closed(self):
        report = good_report()
        report["runner"]["ref"] = "refs/pull/13/merge"
        report["runner"]["actor"] = "unreviewed-user"
        self.assertIn("UNAUTHORIZED_DISPATCH_CONTEXT", evaluate(report)["findings"])

    def test_wrong_distribution_or_missing_systemd_fail_closed(self):
        report = good_report()
        report["host"].update(os_id="ubuntu", pid1="unknown")
        issues = evaluate(report)["findings"]
        self.assertIn("DEBIAN_13_REQUIRED", issues)
        self.assertIn("SYSTEMD_PID1_REQUIRED", issues)

    def test_active_or_unknown_unit_fails_closed(self):
        for state in ("active", "activating", "unknown"):
            with self.subTest(state=state):
                report = good_report()
                report["services"]["dc24h.service"] = state
                self.assertEqual(evaluate(report)["result"], "BLOCKED")

    def test_invisible_or_running_processes_fail_closed(self):
        report = good_report()
        report["processes"] = None
        self.assertIn("PROCESS_VISIBILITY_UNKNOWN", evaluate(report)["findings"])
        report = good_report()
        report["processes"]["dc24h-twilight"] = 1
        self.assertIn("HUB_PROCESS_RUNNING:dc24h-twilight", evaluate(report)["findings"])

    def test_manual_only_workflow_and_exact_label(self):
        body = WORKFLOW.read_text(encoding="utf-8")
        self.assertIn("  workflow_dispatch:", body)
        self.assertNotIn("  pull_request:", body)
        self.assertNotIn("  push:", body)
        self.assertIn("twilight-debian13-stage2", body)
        self.assertIn("github.actor == 'adrianf87'", body)
        self.assertIn("github.ref == 'refs/heads/main'", body)
        self.assertIn("persist-credentials: false", body)
        self.assertIn("contents: read", body)
        self.assertNotRegex(body, re.compile(r"\bsudo\s+(?:-\S+\s+)*\w+"))

    def test_sudo_workflow_is_manual_main_only_and_privilege_scoped(self):
        body = SUDO_WORKFLOW.read_text(encoding="utf-8")
        self.assertIn("  workflow_dispatch:", body)
        self.assertNotIn("  pull_request:", body)
        self.assertNotIn("  push:", body)
        self.assertIn("github.actor == 'adrianf87'", body)
        self.assertIn("github.ref == 'refs/heads/main'", body)
        self.assertIn("twilight-debian13-stage2", body)
        self.assertIn("RUN_STAGE2_SUDO_ISOLATED", body)
        self.assertIn("persist-credentials: false", body)
        self.assertIn("contents: read", body)
        self.assertEqual(body.count("sudo -n "), 2)
        self.assertIn("sudo -n id -u", body)
        self.assertIn("sudo -n python3 scripts/public_stage2_preflight.py", body)
        self.assertNotRegex(body, re.compile(r"(?m)^\s*sudo\s+(?!-n\s)"))
        for forbidden in ("systemctl start", "systemctl stop", "systemctl enable", "apt install", "dpkg -i"):
            self.assertNotIn(forbidden, body)


if __name__ == "__main__":
    unittest.main()
