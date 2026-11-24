"""Offline smoke tests with synthetic emitters, never a claim of real fuzzing."""

import os
from pathlib import Path
import sys
import tempfile
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

    def test_private_seed_copy_and_bounded_stdout_stderr(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            corpus = root / "corpus"
            corpus.mkdir()
            (corpus / "seed").write_bytes(b"BSUP ADBASE ADC/1.0\n")
            fake = root / "fake-fuzzer"
            fake.write_text('#!/bin/sh\nprintf "#1024 DONE\\n" >&2\ntouch "$PWD/secret-crash"\n')
            os.chmod(fake, 0o700)
            observed, _ = gate.run_bounded(fake, corpus)
            self.assertEqual(observed, 1024)
            self.assertEqual(list(corpus.iterdir()), [corpus / "seed"])
            self.assertEqual(sorted(root.iterdir()), [corpus, fake])

    def test_unbounded_log_and_symlinked_seed_are_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            corpus = root / "corpus"
            corpus.mkdir()
            seed = corpus / "seed"
            seed.write_bytes(b"ADC\n")
            fake = root / "fake-fuzzer"
            fake.write_text('#!/bin/sh\nhead -c 1200000 /dev/zero >&2\nprintf "#1024 DONE\\n" >&2\n')
            os.chmod(fake, 0o700)
            with self.assertRaises(ValueError):
                gate.run_bounded(fake, corpus)
            (corpus / "alias").symlink_to(seed)
            with self.assertRaisesRegex(ValueError, "seed corpus"):
                gate.run_bounded(fake, corpus)


if __name__ == "__main__":
    unittest.main()
