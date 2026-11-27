"""Synthetic transport and fault constraints, not production performance."""

import copy
import hashlib
import json
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import beta09_security_envelope as gate
import beta08_transport_evidence as transport
from beta07_ubuntu_perf_matrix_tests import data, SHA, OS


def fixture():
    adc, https = data()
    summary = gate.matrix.evaluate(SHA, OS, adc, https)
    locked = transport.evaluate(SHA, OS, json.dumps(adc).encode(), json.dumps(https).encode(),
                                summary, "13.3.0", "v22.14.0", "b" * 64)
    fault = {"schema": 1, "source_sha": SHA, "os_image": OS,
             "required_cases": list(gate.faults.REQUIRED),
             "case_seconds": {case: 1.0 for case in gate.faults.REQUIRED},
             "all_required_fault_cases_passed": True, "production_sla_claim": False}
    provenance = {"schema": 1, "source_sha": SHA, "os_image": OS,
                  "runner_environment": "github-hosted", "compiler_version": "13.3.0",
                  "node_version": "v22.14.0", "npm_lock_sha256": "b" * 64,
                  "selected_public_apt_versions": {name: "1.0" for name in gate.host.REQUIRED_APT},
                  "apt_full_installed_inventory_sha256": "c" * 64,
                  "apt_repository_snapshot_immutable": False,
                  "fully_reproducible_os_build": False}
    return adc, https, locked, fault, provenance


class BoundedEnvelopeTests(unittest.TestCase):
    def test_three_recomputed_workloads_are_separate_and_faults_bounded(self):
        evidence = gate.evaluate(SHA, OS, *fixture())
        self.assertEqual(evidence["windows_per_transport"], 3)
        self.assertEqual(evidence["bounded_fault_case_count"], 5)
        self.assertFalse(evidence["across_protocol_capacity_equivalence"])
        self.assertFalse(evidence["production_sla_or_capacity_approved"])

    def test_nonfinite_wrong_head_forged_percentile_or_missing_toolchain_fails(self):
        for damage in (
            lambda a: a[2]["profiles"]["adc"].update(max_observed_p99_ms=999),
            lambda a: a[0]["windows"]["adcs"][0].update(successful_requests_per_second=999),
            lambda a: a[0]["windows"]["adc"][0].update(p50_ms=float("nan")),
            lambda a: a[3]["case_seconds"].update({gate.faults.REQUIRED[0]: 241}),
            lambda a: a[3]["case_seconds"].update({gate.faults.REQUIRED[0]: float("nan")}),
            lambda a: a[4].update(source_sha="d" * 40),
            lambda a: a[4].update(compiler_version="14.0.0"),
            lambda a: a[4].update(npm_lock_sha256="d" * 64),
            lambda a: a[4].update(fully_reproducible_os_build=True),
        ):
            with self.subTest(damage=damage), self.assertRaises(ValueError):
                args = list(copy.deepcopy(fixture()))
                damage(args)
                gate.evaluate(SHA, OS, *args)


if __name__ == "__main__":
    unittest.main()
