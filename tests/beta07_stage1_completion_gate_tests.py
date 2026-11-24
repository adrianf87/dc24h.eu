"""Fail-closed stage completion logic, deliberately using synthetic manifests."""

import copy
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import beta07_stage1_completion_gate as gate


SHA = "a" * 40


def artificial_complete_manifest():
    return {"schema": 1, "head_sha": SHA, "version": "beta-0.7.20",
            "patches": {f"0.7.{n}": True for n in range(1, 21)},
            "jobs": [{"kind": kind, "os_image": os_image, "configuration": configuration,
                      "head_sha": SHA, "status": "completed", "conclusion": "success", "run_id": 1}
                     for kind, os_image, configuration in sorted(gate.REQUIRED)],
            "artifacts": {name: True for name in (
                "ncdc_matrix", "real_passkey", "matched_six_store", "installed_dpkg_rollback",
                "three_transport_windows", "bounded_exhaustion", "npm_advisories",
                "action_lock_provenance", "risk_register", "pl_runbook")}}


class GateTests(unittest.TestCase):
    def test_synthetic_positive_never_authorizes_merge_or_deployment(self):
        result = gate.evaluate(artificial_complete_manifest(), SHA)
        self.assertEqual(result["accepted_patches"], 20)
        self.assertEqual(result["successful_required_jobs"], 17)
        self.assertFalse(result["stage2_complete"])
        self.assertFalse(result["five_post_stage_fix_passes_complete"])
        self.assertFalse(result["merge_authorized"])
        self.assertFalse(result["production_deployment_authorized"])

    def test_skipped_pending_wrong_sha_missing_patch_or_artifact_never_passes(self):
        for damage in (
            lambda d: d["jobs"][0].update(conclusion="skipped"),
            lambda d: d["jobs"][0].update(status="queued"),
            lambda d: d["jobs"][0].update(head_sha="b" * 40),
            lambda d: d["jobs"].pop(),
            lambda d: d["patches"].update({"0.7.19": False}),
            lambda d: d["artifacts"].update(real_passkey=False),
            lambda d: d.update(version="beta-0.6.16"),
            lambda d: d["jobs"].append(copy.deepcopy(d["jobs"][0])),
        ):
            with self.subTest(damage=damage):
                manifest = artificial_complete_manifest()
                damage(manifest)
                with self.assertRaises(ValueError):
                    gate.evaluate(manifest, SHA)


if __name__ == "__main__":
    unittest.main()
