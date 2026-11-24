"""Offline, non-deploying beta-0.5.12 release-candidate invariants."""

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
VERSION = "beta-0.5.12"
BRANCH = "beta-0.4.13-0.4.16-stage4"


def source(path):
    return (ROOT / path).read_text(encoding="utf-8")


class StageFourReleaseGate(unittest.TestCase):
    def test_code_and_readme_versions_agree(self):
        for path, expected in (
            ("CMakeLists.txt", f'set(DC24H_VERSION "{VERSION}")'),
            ("webadmin/src/lib/status.ts", f'WEBADMIN_VERSION = "{VERSION}"'),
            ("webadmin/tests/fixtures.ts", f'version: "{VERSION}"'),
            ("README.md", f"Current development version: {VERSION}"),
            ("webadmin/README.md", VERSION),
            ("doc/README.md", VERSION),
            ("doc/us/README.md", VERSION),
            ("doc/pl/README.md", VERSION),
        ):
            self.assertIn(expected, source(path), path)

    def test_required_ci_and_stage4_regressions_are_registered(self):
        cmake = source("CMakeLists.txt")
        for check in (
            "beta.04.stage4.perf.command",
            "beta.04.stage4.recovery",
            "beta.04.stage4.release",
            "beta.release.gate",
        ):
            self.assertIn(f"add_test(NAME {check} ", cmake)
        workflow = source(".github/workflows/build.yml")
        for required in (
            BRANCH, "ubuntu-22.04", "ubuntu-24.04", "ubuntu-26.04",
            "sanitizer: [address, undefined]", "needs: [cpp, webadmin, sanitizers]",
        ):
            self.assertIn(required, workflow)

    def test_private_read_only_boundary_and_schema_unchanged(self):
        self.assertEqual(len(list((ROOT / "database/migrations").glob("*.sql"))), 11)
        gateway = source("webadmin/server/remote-main.ts")
        self.assertIn('app.listen(port, "127.0.0.1"', gateway)
        self.assertIn("Public remote gateway is disabled", gateway)
        apache = source("config/apache2-webadmin-remote.conf.example")
        self.assertIn("Listen 127.0.0.1:8443", apache)
        self.assertIn("Require local", apache)
        self.assertNotIn("0.0.0.0:8443", apache)
        self.assertNotIn("/remote/v1/", source("config/apache2-webadmin-https.conf.example"))

    def test_recovery_and_load_tools_require_explicit_isolation(self):
        perf = source("scripts/stage4_perf_probe.py")
        recovery = source("scripts/stage4_private_recovery_drill.py")
        self.assertIn("--ack-isolated", perf)
        self.assertIn("--ack-isolated", recovery)
        self.assertIn("--ack-stopped", recovery)
        self.assertIn("127.0.0.1", perf)
        self.assertIn("localhost", perf)
        self.assertIn("O_NOFOLLOW", recovery)
        for name in ("stage4_perf_probe_tests.py", "stage4_private_recovery_tests.py"):
            self.assertTrue((ROOT / "tests" / name).is_file())

    def test_release_docs_require_real_evidence_and_manual_decision(self):
        english = source("doc/us/BETA_0.4_STAGE_IV.md")
        polish = source("doc/pl/BETA_0.4_STAGE_IV.md")
        for token in (
            BRANCH, "NOT MEASURED", "PENDING", "NO-GO", "independent",
            "Debian 13", "MariaDB", "RPO/RTO", "GitHub Actions",
        ):
            self.assertIn(token, english, token)
        for token in ("NOT MEASURED", "PENDING", "NO-GO", "Debian 13", "MariaDB"):
            self.assertIn(token, polish, token)
        for doc in (
            "doc/us/BETA_0.4_STAGE_IV_PERFORMANCE.md",
            "doc/us/BETA_0.4_STAGE_IV_RECOVERY.md",
        ):
            self.assertIn("NOT MEASURED", source(doc), doc)


if __name__ == "__main__":
    unittest.main()
