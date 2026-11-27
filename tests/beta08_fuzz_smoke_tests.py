"""Offline smoke tests with synthetic emitters, never a claim of real fuzzing."""

import os
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import beta08_fuzz_smoke as gate


class FuzzSmokeTests(unittest.TestCase):
    def test_only_exact_head_github_hosted_ubuntu_2204_is_accepted(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            binary = root / "fuzzer"
            binary.touch()
            corpus = root / "corpus"
            corpus.mkdir()
            output = root / "summary.json"
            sha = "a" * 40
            args = ["--root", str(root), "--binary", str(binary),
                    "--corpus", str(corpus), "--sha", sha,
                    "--output", str(output)]
            hosted = {"GITHUB_ACTIONS": "true", "RUNNER_ENVIRONMENT": "github-hosted",
                      "RUNNER_OS": "Linux"}
            with (patch.dict(os.environ, hosted),
                  patch.object(gate.subprocess, "check_output", return_value=sha.encode()),
                  patch.object(gate, "run_bounded", return_value=(gate.RUNS, 1.0)) as run):
                with patch.object(gate.platform, "freedesktop_os_release",
                                  return_value={"ID": "ubuntu", "VERSION_ID": "22.04"}):
                    gate.main(args)
                self.assertEqual(json.loads(output.read_text())["os_image"], "ubuntu-22.04")
                self.assertEqual(run.call_count, 1)
                for environment, host in (
                    ({**hosted, "RUNNER_ENVIRONMENT": "self-hosted"},
                     {"ID": "ubuntu", "VERSION_ID": "22.04"}),
                    (hosted, {"ID": "ubuntu", "VERSION_ID": "24.04"}),
                    (hosted, {"ID": "debian", "VERSION_ID": "13"}),
                ):
                    with (self.subTest(environment=environment, host=host),
                          patch.dict(os.environ, environment),
                          patch.object(gate.platform, "freedesktop_os_release", return_value=host),
                          self.assertRaises(SystemExit)):
                        gate.main(args)
                self.assertEqual(run.call_count, 1,
                                 "unapproved hosts must never invoke the fuzz executable")

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
