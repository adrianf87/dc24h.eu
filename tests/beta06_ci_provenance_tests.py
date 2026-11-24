"""Negative tests for beta-0.6.1 provenance and required-suite integrity."""

import sys
from pathlib import Path
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from beta06_ci_provenance import validate


SHA = "a" * 40


def report():
    return {"schema": 1, "suite": "cpp-ubuntu-24.04-Debug", "commit_sha": SHA,
            "state": "passed", "tests": 120, "failures": 0, "skipped": 0}


def run(value, *, sha=SHA, suite="cpp-ubuntu-24.04-Debug",
        image="ubuntu-24.04", environment="github-hosted", release="24.04"):
    return validate(value, sha=sha, suite=suite, image=image,
                    environment=environment, release=release)


class ProvenanceTests(unittest.TestCase):
    def test_valid_exact_sha_and_supported_image(self):
        result = run(report())
        self.assertEqual(result["source_sha"], SHA)
        self.assertFalse(result["production_attestation"])

    def test_wrong_head_cannot_reuse_old_evidence(self):
        with self.assertRaisesRegex(ValueError, "Wrong-head"):
            run(report(), sha="b" * 40)

    def test_missing_zero_or_malformed_sha(self):
        for sha in ("", "0" * 40, "a" * 39, "A" * 40, None):
            with self.subTest(sha=sha), self.assertRaises(ValueError):
                run(report(), sha=sha)

    def test_skipped_failed_and_not_run_are_never_green(self):
        for changes in ({"state": "failed"}, {"state": "not-run"},
                        {"failures": 1}, {"skipped": 1}, {"tests": 0}):
            item = report()
            item.update(changes)
            with self.subTest(changes=changes), self.assertRaises(ValueError):
                run(item)

    def test_unreviewed_and_fabricated_shapes_rejected(self):
        for changes in ({"tests": True}, {"schema": True},
                        {"skipped": False}, {"tests": -1}, {"state": "passed ",
                        "notes": "unknown"}):
            item = report()
            item.update(changes)
            with self.subTest(changes=changes), self.assertRaises(ValueError):
                run(item)
        item = report()
        del item["failures"]
        with self.assertRaises(ValueError):
            run(item)

    def test_unsupported_or_mismatched_runner_refused(self):
        for options in ({"image": "debian-13"}, {"image": "ubuntu-26.04"},
                        {"environment": "self-hosted"}, {"release": "22.04"}):
            with self.subTest(options=options), self.assertRaises(ValueError):
                run(report(), **options)

    def test_wrong_suite_and_invalid_suite(self):
        with self.assertRaises(ValueError):
            run(report(), suite="cpp-ubuntu-24.04-Release")
        with self.assertRaises(ValueError):
            run(report(), suite="../../leak")


if __name__ == "__main__":
    unittest.main()
