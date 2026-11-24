"""Synthetic two-profile fuzz report contract, not a real sanitizer result."""

import copy
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import beta09_fuzz_envelope as gate
import beta08_fuzz_smoke as old


SHA = "a" * 40


def fixture():
    return {name: {"schema": 1, "source_sha": SHA, "os_image": "ubuntu-24.04",
                   "fuzzer_profile": name, "compiler": "clang-libfuzzer",
                   "requested_runs": old.RUNS, "observed_runs": old.RUNS,
                   "seed": old.SEED, "input_max_bytes": 4096, "rss_limit_mib": 512,
                   "timeout_per_input_seconds": 5, "elapsed_seconds": 10.5,
                   "bounded_diagnostic_bytes": old.MAX_LOG_BYTES,
                   "ephemeral_corpus_and_crash_artifacts": True, "sanitizer_findings": 0,
                   "all_required_fuzz_checks_passed": True, "raw_traces_uploaded": False,
                   "production_security_certified": False}
            for name in gate.PROFILES}


class DualFuzzTests(unittest.TestCase):
    def test_two_real_profiles_are_required_and_no_production_claim(self):
        result = gate.evaluate(SHA, fixture())
        self.assertEqual(result["required_runs"], 2048)
        self.assertFalse(result["independent_production_security_certification"])

    def test_wrong_head_skip_partial_or_unbounded_report_fails(self):
        for damage in (
            lambda x: x.pop("adc_fragments"),
            lambda x: x["adc_fragments"].update(source_sha="b" * 40),
            lambda x: x["adc_fragments"].update(fuzzer_profile="adc_parser"),
            lambda x: x["adc_fragments"].update(observed_runs=1023),
            lambda x: x["adc_parser"].update(sanitizer_findings=1),
            lambda x: x["adc_parser"].update(elapsed_seconds=float("nan")),
            lambda x: x["adc_parser"].update(bounded_diagnostic_bytes=0),
            lambda x: x["adc_parser"].update(raw_traces_uploaded=True),
        ):
            with self.subTest(damage=damage), self.assertRaises(ValueError):
                records = copy.deepcopy(fixture())
                damage(records)
                gate.evaluate(SHA, records)


if __name__ == "__main__":
    unittest.main()
