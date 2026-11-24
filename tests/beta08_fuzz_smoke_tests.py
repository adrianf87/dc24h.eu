"""Offline validation of bounded fuzz outcomes; never executes a live fuzzer."""

from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import beta08_fuzz_smoke as gate


class FuzzSmokeTests(unittest.TestCase):
    def test_completed_bounded_run(self):
        self.assertEqual(gate.classify(0, b"#1024 DONE cov: 100", 4.2), 1024)

    def test_missing_done_error_sanitizer_and_timeout_refused(self):
        for code, log, elapsed in ((1, b"#1024 DONE", 4),
                                   (0, b"#100 pulse", 4),
                                   (0, b"#1024 DONE", 181),
                                   (0, b"ERROR: AddressSanitizer\n#1024 DONE", 4),
                                   (0, b"#1024 DONE", 0)):
            with self.subTest(code=code, log=log), self.assertRaises(ValueError):
                gate.classify(code, log, elapsed)


if __name__ == "__main__":
    unittest.main()
