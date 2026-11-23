"""Prevent a dependency-provisioning regression from producing green empty CI."""

import importlib.util
from pathlib import Path
import unittest


SCRIPT = Path(__file__).resolve().parents[1] / "scripts/compatibility_fixture_gate.py"
spec = importlib.util.spec_from_file_location("compatibility_fixture_gate", SCRIPT)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class CompatibilityGateTests(unittest.TestCase):
    def test_complete_fixture_list(self):
        self.assertEqual(module.check_fixture_names(module.REQUIRED), len(module.REQUIRED))
        self.assertEqual(module.check_fixture_names(iter(module.REQUIRED)), len(module.REQUIRED))

    def test_missing_database_or_apache_fails(self):
        for missing in ("database.process", "apache2.https", "adcs.process"):
            with self.subTest(missing=missing), self.assertRaisesRegex(ValueError, missing):
                module.check_fixture_names(module.REQUIRED - {missing})


if __name__ == "__main__":
    unittest.main()
