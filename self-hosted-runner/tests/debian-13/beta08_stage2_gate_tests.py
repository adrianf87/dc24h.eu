"""Stage II gate unit tests use artificial manifests, never hosted proof."""

import copy
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import beta08_stage2_gate as gate

SHA = "a" * 40
IMAGE = "ubuntu-24.04"


def fixture():
    restored = {"schema": 1, "source_sha": SHA, "os_image": IMAGE,
                "evidence": "fresh-mariadb-five-store-real-reauth",
                "production_recovery_claim": False,
                **{key: True for key in gate.RESTORE_FLAGS}}
    auth = {"schema": 1, "source_sha": SHA, "os_image": IMAGE,
            "ci_regression_gate_passed": True, "production_capacity_claim": False}
    xml = '<testsuites><testsuite tests="2">' + "".join(
        f'<testcase name="{name}" time="0.20" />' for name in gate.REQUIRED_BROWSER
    ) + "</testsuite></testsuites>"
    return restored, auth, xml


class Stage2GateTests(unittest.TestCase):
    def test_synthetic_positive_never_claims_physical_device_or_production(self):
        restored, auth, xml = fixture()
        result = gate.assess(restored, auth, xml, sha=SHA, image=IMAGE)
        self.assertTrue(result["ci_regression_gate_passed"])
        self.assertFalse(result["physical_authenticator_tested"])
        self.assertFalse(result["offline_catalog_authenticity_proven"])
        self.assertFalse(result["production_recovery_claim"])

    def test_wrong_head_missing_restore_flags_and_bad_auth_fail(self):
        for change in (
            lambda r, a: r.update(source_sha="b" * 40),
            lambda r, a: r.update(real_browser_webauthn_exercised=False),
            lambda r, a: r.update(revoked_passkey_persisted_after_restart=None),
            lambda r, a: a.update(ci_regression_gate_passed=False),
            lambda r, a: a.update(source_sha="b" * 40),
            lambda r, a: r.update(production_recovery_claim=True),
        ):
            with self.subTest(change=change):
                restored, auth, xml = fixture()
                change(restored, auth)
                with self.assertRaises(ValueError):
                    gate.assess(restored, auth, xml, sha=SHA, image=IMAGE)

    def test_missing_duplicate_skipped_failed_and_nan_browser_cases_fail(self):
        for changed in (
            lambda xml: xml.replace(gate.REQUIRED_BROWSER[0], "unrelated"),
            lambda xml: xml.replace("</testsuite>",
                f'<testcase name="{gate.REQUIRED_BROWSER[0]}" time="0.2" /></testsuite>'),
            lambda xml: xml.replace('time="0.20" />', 'time="0.20"><skipped /></testcase>', 1),
            lambda xml: xml.replace('time="0.20" />', 'time="0.20"><failure /></testcase>', 1),
            lambda xml: xml.replace('time="0.20" />', 'time="NaN" />', 1),
        ):
            with self.subTest(change=changed):
                restored, auth, xml = fixture()
                with self.assertRaises(ValueError):
                    gate.assess(restored, auth, changed(xml), sha=SHA, image=IMAGE)

    def test_unsupported_image_and_invalid_source_sha_fail(self):
        restored, auth, xml = fixture()
        for sha, image in (("b" * 39, IMAGE), (SHA, "debian-13")):
            with self.assertRaises(ValueError):
                gate.assess(copy.deepcopy(restored), copy.deepcopy(auth), xml, sha=sha, image=image)


if __name__ == "__main__":
    unittest.main()
