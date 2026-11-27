"""Offline structural checks for self-hosted runner files, no VPS access."""
from __future__ import annotations

import pathlib
import subprocess
import unittest


RUNNER_ROOT = pathlib.Path(__file__).resolve().parents[1]


class RunnerLayoutTests(unittest.TestCase):
    def test_dated_runner_documents_present(self) -> None:
        dated = RUNNER_ROOT / "27112026"
        required = (
            "README.md",
            "SYSTEM.md",
            "RUNNER.md",
            "INSTALLED_PACKAGES.md",
            "REMOVED_PACKAGES.md",
            "SECURITY.md",
            "CI_WORKFLOWS.md",
            "CHANGELOG.md",
            "inventory/README.md",
        )
        for relative in required:
            with self.subTest(path=relative):
                self.assertTrue((dated / relative).is_file(), relative)

    def test_collector_bash_syntax(self) -> None:
        collector = RUNNER_ROOT / "scripts" / "collect-inventory.sh"
        self.assertTrue(collector.is_file())
        result = subprocess.run(
            ["bash", "-n", str(collector)],
            capture_output=True,
            text=True,
            check=False,
        )
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_manual_runner_entrypoint_bash_syntax(self) -> None:
        script = RUNNER_ROOT / "scripts" / "run-tests.sh"
        self.assertTrue(script.is_file())
        result = subprocess.run(
            ["bash", "-n", str(script)],
            capture_output=True,
            text=True,
            check=False,
        )
        self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == "__main__":
    unittest.main()
