#!/usr/bin/env python3
"""No-root contract tests for the OWNER-gated public ADCS installer."""

from argparse import Namespace
from pathlib import Path
from tempfile import TemporaryDirectory
from unittest import TestCase, main
from unittest.mock import MagicMock, patch

import public_stage3_adcs_activate as deploy


class StageIIIActivationTests(TestCase):
    def args(self, action, confirmation):
        return Namespace(
            action=action, confirmation=confirmation,
            actor="adrianf87", ref="refs/heads/main", runner="server-747585786",
            runner_user="github-runner", expected_sha="a" * 40,
            provider_1512_confirmed=True, tls_renewal_plan_confirmed=True,
            binary=Path("/nonexistent"), deployed_sha=None,
        )

    def test_explicit_public_tls_only_and_private_database(self):
        config = deploy.guest_config()
        self.assertIn("listen = none\n", config)
        self.assertIn("adcs_listen = 62.171.156.32:1512", config)
        self.assertNotIn("adcs_listen = 0.0.0.0", config)
        for section in ("database", "authentication", "api"):
            self.assertIn(f"[{section}]\nenabled = off", config)

    def test_systemd_separate_unit_with_unprivileged_identity(self):
        unit = deploy.service_unit(Path("/opt/dc24h-stage3/a/bin/dc24h-twilight"))
        self.assertIn("User=dc24h-stage3", unit)
        self.assertIn("NoNewPrivileges=yes", unit)
        self.assertIn("ProtectSystem=strict", unit)
        self.assertIn("AssertPathExists=/etc/dc24h-stage3/.owner-activation", unit)
        self.assertNotIn("ExecStart=/usr/local", unit)

    @patch.object(deploy.readiness, "matching_source_sha", return_value=True)
    @patch.object(deploy.readiness, "debian_13", return_value=True)
    @patch.object(deploy.os, "geteuid", return_value=0)
    def test_owner_gate_accepts_exact_action_token(self, _uid, _debian, _sha):
        deploy.require_identity(self.args("activate", deploy.ACTIVATE_TOKEN))

    @patch.object(deploy.readiness, "matching_source_sha", return_value=True)
    @patch.object(deploy.readiness, "debian_13", return_value=True)
    @patch.object(deploy.os, "geteuid", return_value=0)
    def test_owner_gate_rejects_wrong_token(self, _uid, _debian, _sha):
        with self.assertRaises(RuntimeError):
            deploy.require_identity(self.args("activate", deploy.ROLLBACK_TOKEN))

    @patch.object(deploy.readiness, "matching_source_sha", return_value=True)
    @patch.object(deploy.readiness, "debian_13", return_value=True)
    @patch.object(deploy.os, "geteuid", return_value=0)
    def test_owner_gate_rejects_other_runner_and_branch(self, _uid, _debian, _sha):
        args = self.args("activate", deploy.ACTIVATE_TOKEN)
        args.runner = "wrong"
        with self.assertRaises(RuntimeError):
            deploy.require_identity(args)
        args.runner = "server-747585786"
        args.ref = "refs/heads/feature"
        with self.assertRaises(RuntimeError):
            deploy.require_identity(args)

    def test_activation_requires_no_existing_owned_paths(self):
        with patch.object(deploy, "UNIT_PATH", Path("/proc/self")):
            with self.assertRaises(RuntimeError):
                deploy.assert_new_install_targets()

    def test_rollback_preserves_unowned_files_and_removes_owned_targets(self):
        with TemporaryDirectory() as folder:
            root = Path(folder)
            prefix = root / "opt" / "dc24h-stage3"
            release = prefix / ("a" * 12)
            (release / "bin").mkdir(parents=True)
            (release / "bin" / "dc24h-twilight").write_bytes(b"binary")
            config_dir = root / "etc" / "dc24h-stage3"
            tls_dir = config_dir / "tls"
            tls_dir.mkdir(parents=True)
            config = config_dir / "dc24h.ini"
            cert = tls_dir / "fullchain.pem"
            key = tls_dir / "privkey.pem"
            marker = config_dir / ".owner-activation"
            for path in (config, cert, key, marker):
                path.write_text("owned")
            unit = root / "dc24h-stage3-adcs.service"
            unit.write_text("owned")
            manifest = prefix / "manifest.json"
            manifest.write_text('{"schema": 1, "source_sha": "' + 'a' * 40 +
                                '", "unit": "dc24h-stage3-adcs.service",' +
                                '"fqdn": "server-747585786.dc24h.eu",' +
                                '"ipv4": "62.171.156.32", "port": 1512,' +
                                '"created_user": false}')
            foreign = root / "keep-user-data.txt"
            foreign.write_text("preserve")
            with patch.multiple(deploy, PREFIX=prefix, MANIFEST=manifest, UNIT_PATH=unit,
                                CONFIG_DIR=config_dir, TLS_DIR=tls_dir, CONFIG=config,
                                CERT=cert, KEY=key, MARKER=marker), \
                    patch.object(deploy, "run"), \
                    patch.object(deploy.readiness, "command", return_value=MagicMock(returncode=3)), \
                    patch.object(deploy.readiness, "port_available", return_value=True):
                deploy.rollback(self.args("rollback", deploy.ROLLBACK_TOKEN))
            self.assertFalse(manifest.exists())
            self.assertFalse(unit.exists())
            self.assertFalse(key.exists())
            self.assertEqual(foreign.read_text(), "preserve")


if __name__ == "__main__":
    main()
