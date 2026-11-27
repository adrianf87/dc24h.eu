"""Negative-evidence regression: partial and cross-head Stage III claims fail."""

import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import beta09_stage3_gate as gate


class StageThreeEvidenceTests(unittest.TestCase):
    SHA = "a" * 40
    IMAGE = "ubuntu-24.04"

    def report(self, directory, name, fields):
        path = directory / name
        path.write_text(json.dumps({"schema": 1, "source_sha": self.SHA,
                                    "os_image": self.IMAGE, **fields}))
        return path

    def test_valid_exact_head_evidence(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            dpkg = self.report(root, "dpkg.json", {**dict.fromkeys(gate.DPKG_PROOFS, True),
                "production_package_or_rollback_claim": False})
            node = self.report(root, "node.json", {**dict.fromkeys(gate.NODE_PROOFS, True),
                "production_service_changed": False})
            result = gate.evaluate(dpkg, node, self.SHA, self.IMAGE)
            self.assertTrue(result["beta_0_9_12_actual_systemd_cpp_and_node_ci_verified"])
            self.assertFalse(result["production_deployment_or_rollback_proven"])
            with self.assertRaisesRegex(ValueError, "exact SHA"):
                gate.evaluate(dpkg, node, "b" * 39, self.IMAGE)
            with self.assertRaisesRegex(ValueError, "bound to"):
                gate.evaluate(dpkg, node, "b" * 40, self.IMAGE)

    def test_missing_historical_login_or_node_hardening_fails(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            dpkg = self.report(root, "dpkg.json", {**dict.fromkeys(gate.DPKG_PROOFS, True),
                "historical_maintenance_actual_password_totp_and_browser_passkey": False,
                "production_package_or_rollback_claim": False})
            node = self.report(root, "node.json", {**dict.fromkeys(gate.NODE_PROOFS, True),
                "production_service_changed": False})
            with self.assertRaisesRegex(ValueError, "required executed proof"):
                gate.evaluate(dpkg, node, self.SHA, self.IMAGE)
            adapter_missing = self.report(root, "adapter-missing.json", {
                **dict.fromkeys(gate.DPKG_PROOFS, True),
                "historical_maintenance_pinned_status_adapter_ci_only": False,
                "production_package_or_rollback_claim": False})
            with self.assertRaisesRegex(ValueError, "required executed proof"):
                gate.evaluate(adapter_missing, node, self.SHA, self.IMAGE)
            dpkg = self.report(root, "dpkg2.json", {**dict.fromkeys(gate.DPKG_PROOFS, True),
                "production_package_or_rollback_claim": False})
            node = self.report(root, "node2.json", {**dict.fromkeys(gate.NODE_PROOFS, True),
                "loopback_only_listener": False, "production_service_changed": False})
            with self.assertRaisesRegex(ValueError, "required executed proof"):
                gate.evaluate(dpkg, node, self.SHA, self.IMAGE)


if __name__ == "__main__":
    unittest.main()
