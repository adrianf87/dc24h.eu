"""Unit checks for offline CodeQL results qualification."""

import json
import tempfile
import unittest
from pathlib import Path

from scripts.beta07_codeql_sarif_gate import evaluate


class TestCodeQLGate(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def sarif(self, language, *, security=None, level="warning"):
        result = {"ruleId": "x", "level": level}
        rule = {"id": "x", "properties": {}}
        if security is not None:
            rule["properties"]["security-severity"] = security
        name = f"{language}.sarif"
        path = self.root / name
        path.write_text(json.dumps({"version": "2.1.0", "runs": [
            {"tool": {"driver": {"name": "CodeQL", "rules": [rule]}}, "results": [result]}
        ]}), encoding="utf8")
        return path

    def test_clean_bilingual_scan(self):
        results = evaluate([self.sarif("cpp"), self.sarif("javascript-typescript")])
        self.assertEqual(results["findings_total"], 2)

    def test_actual_codeql_javascript_filename_is_typescript_analyzer(self):
        results = evaluate([self.sarif("cpp"), self.sarif("javascript")])
        self.assertEqual(results["languages"], ["cpp", "javascript-typescript"])

    def test_unrecognized_analyzer_filename_does_not_satisfy_language_gate(self):
        with self.assertRaisesRegex(ValueError, "Incomplete"):
            evaluate([self.sarif("cpp"), self.sarif("untrusted")])

    def test_workflow_keeps_cmake_generated_ts_out_of_source_scan(self):
        workflow = (Path(__file__).resolve().parents[1] /
                    ".github/workflows/beta07-codeql.yml").read_text()
        self.assertIn('cmake -S . -B "$RUNNER_TEMP/beta07-codeql-build"', workflow)
        self.assertNotIn("-B build/CodeQL", workflow)

    def test_high_security_rejected(self):
        with self.assertRaisesRegex(ValueError, "high_security=1"):
            evaluate([self.sarif("cpp", security="8.1"), self.sarif("javascript-typescript")])

    def test_error_rejected(self):
        with self.assertRaisesRegex(ValueError, "errors=1"):
            evaluate([self.sarif("cpp"), self.sarif("javascript-typescript", level="error")])

    def test_missing_language_rejected(self):
        with self.assertRaisesRegex(ValueError, "Incomplete"):
            evaluate([self.sarif("cpp")])

    def test_malformed_nonfinite_or_out_of_range_scores_cannot_bypass_policy(self):
        for score in ("nan", "inf", "unknown", "-1", "11", True):
            with self.subTest(score=score):
                with self.assertRaisesRegex(ValueError, "Invalid CodeQL security severity"):
                    evaluate([self.sarif("cpp", security=score), self.sarif("javascript-typescript")])


if __name__ == "__main__":
    unittest.main()
