"""Pure regression checks: neither network nor sudo/systemd is used."""

import argparse
import unittest
from unittest import mock

import public_stage3_qualification_external as external
import public_stage3_qualification_host as host


class FakePeer:
    def __init__(self, frames):
        self.frames = list(frames)
        self.timeout = None

    def settimeout(self, value):
        self.timeout = value

    def recv(self, _size):
        return self.frames.pop(0) if self.frames else b""


class QualificationPolicyTests(unittest.TestCase):
    def test_frame_reader_preserves_overshoot(self):
        peer = FakePeer([b"BMSG ABCD first\nBSCH ABCD ANsecond\n"])
        reader = external.Reader(peer)
        self.assertEqual(reader.until(lambda frame: frame.startswith(b"BMSG"),
                                      "msg"), b"BMSG ABCD first")
        self.assertEqual(reader.until(lambda frame: frame.startswith(b"BSCH"),
                                      "search"), b"BSCH ABCD ANsecond")

    def test_frame_reader_rejects_hub_denial(self):
        reader = external.Reader(FakePeer([b"ISTA 200 Permission\\sdenied FCBINF XXprivate-token\n"]))
        with self.assertRaises(RuntimeError) as caught:
            reader.until(lambda frame: False, "TCP-active signal")
        self.assertIn("TCP-active signal", str(caught.exception))
        self.assertIn("status=200", str(caught.exception))
        self.assertIn("FC=BINF", str(caught.exception))
        self.assertNotIn("private-token", str(caught.exception))
        self.assertNotIn("Permission", str(caught.exception))

    def test_frame_reader_quit_does_not_expose_remote_sid(self):
        reader = external.Reader(FakePeer([b"IQUI ABCD DIsecret\n"]))
        with self.assertRaises(RuntimeError) as caught:
            reader.until(lambda frame: False, "direct search result")
        self.assertIn("direct search result", str(caught.exception))
        self.assertIn("IQUI", str(caught.exception))
        self.assertNotIn("ABCD", str(caught.exception))
        self.assertNotIn("secret", str(caught.exception))

    def test_active_client_update_does_not_spoof_hub_address(self):
        first, second = mock.Mock(), mock.Mock()
        with mock.patch.object(external.ssl, "create_default_context"), mock.patch.object(
                external.base, "connect_guest", side_effect=[(first, b"ABCD"), (second, b"EFGH")]
        ), mock.patch.object(external, "Reader") as reader:
            reader.return_value.until.return_value = b"ok"
            external.check_two_client_protocol()
        frames = [call.args[0] for call in first.sendall.call_args_list]
        self.assertIn(b"BINF ABCD SUTCP4\n", frames)
        self.assertFalse(any(b"I4" in frame for frame in frames))
        self.assertFalse(any(external.base.IPV4.encode() in frame for frame in frames))
        self.assertTrue(any(frame.startswith(b"DCTM ABCD EFGH ") for frame in frames))
        self.assertTrue(any(call.args[0].startswith(b"DRCM EFGH ABCD ")
                            for call in second.sendall.call_args_list))

    def test_negative_prelogin_expected_denial_is_observable(self):
        reader = external.Reader(FakePeer([b"ISTA 244 Login\\srequired\n"]))
        self.assertTrue(reader.until(lambda frame: frame.startswith(b"ISTA 244 "),
                                     "prelogin").startswith(b"ISTA 244 "))

    def test_forbidden_ports_are_explicit_and_bounded(self):
        self.assertIn(1511, external.FORBIDDEN_TCP)
        self.assertNotIn(1512, external.FORBIDDEN_TCP)
        for known_allowed in (22, 80, 443):
            self.assertNotIn(known_allowed, external.FORBIDDEN_TCP)
        self.assertLessEqual(len(external.FORBIDDEN_TCP), 12)

    def test_public_listener_detection_fails_closed(self):
        peer = mock.Mock()
        with mock.patch.object(external.socket, "create_connection", return_value=peer):
            with self.assertRaisesRegex(RuntimeError, "publicly accessible"):
                external.check_forbidden_ports()
        peer.close.assert_called_once()

    def test_public_listener_inventory_excludes_loopback(self):
        sample = ("LISTEN 0 128 0.0.0.0:22 0.0.0.0:*\n"
                  "LISTEN 0 128 62.171.156.32:1512 0.0.0.0:*\n"
                  "LISTEN 0 128 127.0.0.1:3306 0.0.0.0:*\n"
                  "LISTEN 0 128 [::]:443 [::]:*\n")
        self.assertEqual(host.public_listener_ports(sample), {22, 1512, 443})

    def test_public_listener_inventory_flags_unapproved(self):
        fake = mock.Mock(stdout="LISTEN 0 128 0.0.0.0:1512 0.0.0.0:*\n"
                        "LISTEN 0 128 0.0.0.0:3306 0.0.0.0:*\n")
        with mock.patch.object(host, "run", return_value=fake):
            with self.assertRaisesRegex(RuntimeError, "3306"):
                host.check_host_exposure()

    def test_missing_owner_consent_blocks_host(self):
        args = argparse.Namespace(confirmation="WRONG", actor="adrianf87",
                                  ref="refs/heads/main", runner="server-747585786",
                                  runner_user="github-runner", expected_sha="a" * 40)
        with mock.patch.object(host.os, "geteuid", return_value=0):
            with self.assertRaisesRegex(RuntimeError, "guard failed"):
                host.guard(args)

    def test_no_actual_letsencrypt_renewal_is_invoked(self):
        # Reviewable allowlist of subprocess calls to the unit only.
        with mock.patch.object(host, "run") as execute, mock.patch.object(
                host, "probe_until_ready"):
            host.restart_and_probe("TEST")
        self.assertEqual(execute.call_args_list[0].args[:3],
                         ("systemctl", "restart", host.UNIT))

    def test_ncdc_url_pin_matches_sha256_der(self):
        fake = mock.MagicMock()
        fake.getpeercert.return_value = b"public DER sample"
        context = mock.MagicMock()
        context.wrap_socket.return_value.__enter__.return_value = fake
        with mock.patch.object(external.socket, "create_connection"):
            url = external.fingerprint_url(context)
        self.assertTrue(url.startswith("adcs://server-747585786.dc24h.eu:1512/?kp=SHA256/"))
        self.assertNotIn("51.255.80.233", url)


if __name__ == "__main__":
    unittest.main()
