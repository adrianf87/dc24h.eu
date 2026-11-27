"""Twilight Stage VI static qualification: positive and fail-closed negatives.

All fixtures are local file copies. These are NOT GitHub CI/RC acceptance tests
and do not start a service, run dpkg, create a tag or access real private data.
"""

import json
from pathlib import Path
import shutil
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
import twilight_stage6_preflight as stage6

SHA = "a" * 40
FILES = set(stage6.ACTIVE_DISPLAY) | {
    "src/api/server.cpp", "webadmin/src/lib/status.ts",
    "webadmin/server/remote-webauthn.ts",
    "webadmin/server/remote-gateway.ts",
    "config/dc24h-twilight.service.example",
    "config/apache2-webadmin-https.conf.example",
    "config/dc24h.sysusers.example",
    "webadmin/package.json", "webadmin/package-lock.json", "README.md",
} | {".github/workflows/" + name for name in stage6.WORKFLOWS}


def fixture(destination):
    for filename in FILES:
        target = destination / filename
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(ROOT / filename, target)
    return destination


def mutate(root, name, old, new):
    target = root / name
    content = target.read_text(encoding="utf8")
    if old not in content:
        raise AssertionError("Unmatched regression fixture marker: " + name)
    target.write_text(content.replace(old, new, 1), encoding="utf8")


class TwilightStageSixPreflightTests(unittest.TestCase):
    def test_exact_active_repository_structure_is_no_go_not_owner_approval(self):
        report = stage6.evaluate(ROOT, SHA, "ubuntu-22.04")
        self.assertEqual(report["status"], "NO-GO")
        self.assertEqual(report["source_sha"], SHA)
        self.assertEqual(report["active_display_files_checked"], len(stage6.ACTIVE_DISPLAY))
        for name in ("complete_final_sha_github_ci_verified",
                     "artifact_zips_independently_hashed",
                     "stage_v_documentation_accepted",
                     "owner_release_acceptance_recorded",
                     "release_tag_or_production_deployment_authorized"):
            self.assertIs(report[name], False)
        self.assertFalse("GO" == report["status"])

    def test_wrong_sha_os_or_unsupported_identity_refused(self):
        for sha, image in (("bad", "ubuntu-22.04"), ("0" * 40, "ubuntu-22.04"),
                           (SHA, "ubuntu-20.04"), (SHA, "ubuntu-24.04"),
                           (SHA, "ubuntu-26.04"), (SHA, "debian-13")):
            with self.subTest(sha=sha, image=image), self.assertRaises(ValueError):
                stage6.evaluate(ROOT, sha, image)

    def test_missing_current_brand_and_old_brand_are_rejected(self):
        for filename, marker, replacement in (
                ("src/main.cpp", "dc24h.eu-twilight", "old-server"),
                ("webadmin/index.html", "dc24h.eu-twilight", "old-name"),
                ("webadmin/src/App.tsx", "dc24h.eu-twilight", "dc24h.eu High Starter"),
                ("src/adc/user_list.cpp", "NIdc24h.eu-twilight", "NIdc24h.eu"),
                ("src/adc/user_list.cpp", "DEDirect Connect hub", "DEGuest login alpha"),
                ("webadmin/src/languages/pl/index.ts", "Dla dc24h.eu-twilight", "Dla dc24h.eu"),
                ("webadmin/src/languages/us/index.ts", "Built for dc24h.eu-twilight",
                 "Built for dc24h.eu")):
            with self.subTest(filename=filename), tempfile.TemporaryDirectory() as directory:
                root = fixture(Path(directory))
                mutate(root, filename, marker, replacement)
                if filename.endswith("App.tsx"):
                    with self.assertRaisesRegex(ValueError, "legacy user-facing"):
                        stage6.evaluate(root, SHA, "ubuntu-22.04")
                else:
                    with self.assertRaises(ValueError):
                        stage6.evaluate(root, SHA, "ubuntu-22.04")

    def test_strict_status_api_and_passkey_origin_cannot_be_relaxed(self):
        for filename, old, replacement in (
                ("src/api/server.cpp", "dc24h.eu-twilight", "dc24h.eu"),
                ("webadmin/src/lib/status.ts", 'server.name !== "dc24h.eu-twilight"',
                 'server.name !== "dc24h.eu"'),
                ("webadmin/server/remote-webauthn.ts", "expectedRPID: rpID", "expectedRPID: 'other'"),
                ("webadmin/server/remote-webauthn.ts", "expectedOrigin: origin",
                 "expectedOrigin: 'https://other.invalid'"),
                ("webadmin/server/remote-gateway.ts", "req.headers.origin !== options.origin",
                 "false")):
            with self.subTest(filename=filename, marker=old), tempfile.TemporaryDirectory() as directory:
                root = fixture(Path(directory))
                mutate(root, filename, old, replacement)
                with self.assertRaises(ValueError):
                    stage6.evaluate(root, SHA, "ubuntu-22.04")

    def test_operator_marker_service_user_and_repo_guard_cannot_be_removed(self):
        for filename, old, replacement in (
                ("config/dc24h-twilight.service.example", "Conflicts=dc24h.service", "# removed"),
                ("config/dc24h-twilight.service.example",
                 "AssertPathExists=/etc/dc24h/twilight-unit-migration-approved", "# removed"),
                ("config/dc24h.sysusers.example", "u dc24h -", "u other -"),
                ("config/apache2-webadmin-https.conf.example", "ServerName dc24h.eu",
                 "ServerName other.invalid"),
                (".github/workflows/build.yml",
                 "github.repository == 'adrianf87/dc24h.eu-twilight'", "github.repository == 'other/repo'"),
                ("README.md", "derived from dc24h.eu High Starter", "is the original project"),
                ("webadmin/package.json", "dc24h-twilight-webadmin", "old-package")):
            with self.subTest(filename=filename), tempfile.TemporaryDirectory() as directory:
                root = fixture(Path(directory))
                mutate(root, filename, old, replacement)
                with self.assertRaises(ValueError):
                    stage6.evaluate(root, SHA, "ubuntu-22.04")

    def test_report_is_bounded_redacted_and_has_no_automatic_go(self):
        with tempfile.TemporaryDirectory() as directory:
            root = fixture(Path(directory))
            report = stage6.evaluate(root, SHA, "ubuntu-22.04")
            serialized = json.dumps(report)
            self.assertLess(len(serialized), 4096)
            self.assertNotIn("password", serialized.lower())
            self.assertEqual(report["github_workflows_checked"], 5)
            self.assertFalse(report["release_tag_or_production_deployment_authorized"])


if __name__ == "__main__":
    unittest.main()
