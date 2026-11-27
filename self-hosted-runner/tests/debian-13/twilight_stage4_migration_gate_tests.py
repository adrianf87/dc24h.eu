"""Negative and positive validation of Stage IV exact-head CI-only evidence."""

import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import beta07_ci_deb as deb
import beta06_previous_version_rollback as previous
import twilight_stage4_migration_gate as gate


SHA = "b" * 40
IMAGE = "ubuntu-24.04"


def valid_record():
    record = {"schema": 1, "source_sha": SHA, "previous_sha": previous.PREVIOUS_SHA,
              "os_image": IMAGE, "legacy_package": deb.LEGACY_PACKAGE,
              "twilight_package": deb.PACKAGE, "legacy_hub_sha256": "c" * 64,
              "twilight_hub_sha256": "d" * 64,
              "production_deployment_or_migration_claim": False}
    record.update({name: True for name in gate.PROOFS})
    return record


class TwilightMigrationGateTests(unittest.TestCase):
    def test_standalone_cli_resolves_pinned_historical_source(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            report, output = root / "input.json", root / "result.json"
            report.write_text(json.dumps(valid_record()), encoding="utf8")
            project = Path(__file__).resolve().parents[1]
            environment = dict(os.environ)
            environment.pop("PYTHONPATH", None)
            result = subprocess.run([
                sys.executable, "scripts/twilight_stage4_migration_gate.py",
                "--report", str(report), "--sha", SHA, "--os-image", IMAGE,
                "--output", str(output)], cwd=project, env=environment,
                stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=30)
            self.assertEqual(result.returncode, 0, result.stderr.decode(errors="replace"))
            self.assertEqual(json.loads(output.read_text(encoding="utf8"))["source_sha"], SHA)

    def test_complete_evidence_accepts_only_ci_scope(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "report.json"
            path.write_text(json.dumps(valid_record()))
            proof = gate.evaluate(path, SHA, IMAGE)
            self.assertTrue(proof["actual_old_new_old_package_and_unit_rehearsal_verified"])
            self.assertFalse(proof["production_upgrade_or_migration_authorized"])

    def test_each_required_proof_refuses_missing_and_false(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "report.json"
            for name in gate.PROOFS:
                for absent in (True, False):
                    record = valid_record()
                    if absent:
                        del record[name]
                    else:
                        record[name] = False
                    path.write_text(json.dumps(record))
                    with self.subTest(name=name, absent=absent), self.assertRaises(ValueError):
                        gate.evaluate(path, SHA, IMAGE)

    def test_wrong_source_os_package_production_or_hash_refused(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "report.json"
            for name, value in (("source_sha", "a" * 40), ("previous_sha", "f" * 40),
                                ("os_image", "ubuntu-20.04"),
                                ("legacy_package", "dc24h"),
                                ("twilight_package", "dc24h-twilight"),
                                ("production_deployment_or_migration_claim", True),
                                ("legacy_hub_sha256", "bad"),
                                ("twilight_hub_sha256", "c" * 64)):
                record = valid_record(); record[name] = value
                path.write_text(json.dumps(record))
                with self.subTest(field=name), self.assertRaises(ValueError):
                    gate.evaluate(path, SHA, IMAGE)
            for bad_sha, bad_os in (("bad", IMAGE), (SHA, "ubuntu-20.04")):
                path.write_text(json.dumps(valid_record()))
                with self.assertRaises(ValueError):
                    gate.evaluate(path, bad_sha, bad_os)


if __name__ == "__main__":
    unittest.main()
