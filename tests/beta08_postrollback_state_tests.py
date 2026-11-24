"""Socket-free, synthetic Node reopening regression; not a hosted VM claim."""

import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from stage1_matched_recovery_tests import make_gateway_fixture


ROOT = Path(__file__).resolve().parents[1]


@unittest.skipUnless(shutil.which("node") and (ROOT / "webadmin/node_modules/tsx").is_dir(),
                     "requires local Node test dependencies")
class PostRollbackNodeStateTests(unittest.TestCase):
    def execute(self, restored, *, expect_consumed=False):
        # This is a local synthetic unit test. The production-like dpkg gate
        # separately checks the actual GitHub runner and C++ package binaries.
        env = dict(os.environ, CI="true", GITHUB_ACTIONS="true",
                   RUNNER_ENVIRONMENT="github-hosted", RUNNER_OS="Linux")
        argv = ["node", "--import", "tsx", "tests/beta08-postrollback-state.ts", str(restored)]
        if expect_consumed:
            argv.append("--expect-consumed")
        return subprocess.run(argv, cwd=ROOT / "webadmin", env=env,
                              capture_output=True, timeout=15, check=False)

    def test_reopen_preserves_totp_replay_revocation_audit_and_rate_limits(self):
        with tempfile.TemporaryDirectory() as directory:
            restored = Path(directory) / "restored"
            restored.mkdir(mode=0o700)
            make_gateway_fixture(restored)
            accepted = self.execute(restored)
            self.assertEqual(accepted.returncode, 0, "Synthetic restored state should reopen")
            ledger = restored / "replay.jsonl"
            self.assertEqual(len(ledger.read_text().splitlines()), 2)
            # The same OTP ledger survives a simulated process restart.
            again = self.execute(restored, expect_consumed=True)
            self.assertEqual(again.returncode, 0, "Consumed OTP must be rejected by a fresh verifier")

    def test_unrevoked_passkey_is_not_misclassified_as_revoked(self):
        with tempfile.TemporaryDirectory() as directory:
            restored = Path(directory) / "restored"
            restored.mkdir(mode=0o700)
            make_gateway_fixture(restored)
            passkeys = restored / "passkeys.json"
            doc = json.loads(passkeys.read_text())
            doc["credentials"][0]["revokedAt"] = None
            passkeys.write_text(json.dumps(doc) + "\n")
            self.assertNotEqual(self.execute(restored).returncode, 0)


if __name__ == "__main__":
    unittest.main()
