"""No live network: verify raw transport/summary identity and non-equivalence."""

import copy
import json
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import beta07_ubuntu_perf_matrix as old
import beta08_transport_evidence as gate
from beta07_ubuntu_perf_matrix_tests import data, SHA, OS


class TransportEvidenceTests(unittest.TestCase):
    def prepare(self):
        adc, https = data()
        return json.dumps(adc).encode(), json.dumps(https).encode(), old.evaluate(SHA, OS, adc, https)

    def test_recomputed_samples_and_toolchain_identity(self):
        adc, https, summary = self.prepare()
        result = gate.evaluate(SHA, OS, adc, https, summary, "13.3.0", "v22.14.0", "b" * 64)
        self.assertEqual(result["profiles"], summary["profiles"])
        self.assertFalse(result["cross_protocol_throughput_equivalent"])
        self.assertFalse(result["production_sla_or_capacity_claim"])
        self.assertEqual(len(result["adc_window_input_sha256"]), 64)

    def test_fabricated_metrics_or_incomplete_build_identity_fail(self):
        adc, https, summary = self.prepare()
        bad = copy.deepcopy(summary)
        bad["profiles"]["adc"]["min_success_rps"] += 1
        for args in ((adc, https, bad, "13.3.0", "v22.14.0", "b" * 64),
                     (adc, https, summary, "unknown compiler", "v22.14.0", "b" * 64),
                     (adc, https, summary, "13.3.0", "v22.14.0", "wrong"),
                     (adc, https.replace(b'"failure": 0', b'"failure": 1'), summary,
                      "13.3.0", "v22.14.0", "b" * 64)):
            with self.subTest(args=args[3:]), self.assertRaises((ValueError, TypeError)):
                gate.evaluate(SHA, OS, *args)


if __name__ == "__main__":
    unittest.main()
