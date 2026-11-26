"""Pure regression checks: neither network nor sudo/systemd is used."""

import argparse
import unittest
from unittest import mock

import public_stage3_qualification_external as external
import public_stage3_qualification_host as host
import stage3_premerge_gate as premerge
import stage3_premerge_evidence as evidence


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
        self.assertEqual(reader.message_frames, 1)
        self.assertEqual(reader.search_frames, 1)

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

    def test_ncdc_command_clears_stale_input_and_submits_return(self):
        with mock.patch.object(external.os, "write", side_effect=lambda _fd, payload: len(payload)) as write:
            external.send_ncdc(91, "/say stage3-ncdc-qualified")
        self.assertEqual(write.call_args.args,
                         (91, b"\x15/say stage3-ncdc-qualified\r"))
        self.assertEqual(external.NCDC_CHAT, b"stage3-ncdc-qualified")

    def test_ncdc_hub_tab_selection_uses_documented_alt_two(self):
        with mock.patch.object(external.os, "write") as write:
            external.select_ncdc_hub_tab(91)
        write.assert_called_once_with(91, b"\x1b2")

    def test_ncdc_terminal_recognizes_split_synthetic_markers_only(self):
        terminal = external.NcdcTerminal()
        terminal.observe(b"private-should-never-appear #stage3q")
        terminal.observe(b"ual \x1b[1;2Hstage3-ncdc-inb")
        terminal.observe(b"ound-ready\r\nprivate-secret")
        self.assertTrue(terminal.hub_tab_visible)
        self.assertTrue(terminal.inbound_chat_visible)
        self.assertIn("guest_to_ncdc_displayed=yes", terminal.flags())
        self.assertNotIn("private", terminal.flags())
        self.assertNotIn("secret", terminal.flags())
        self.assertLess(len(terminal._tail), len(external.NCDC_INBOUND))

    def test_ncdc_inbound_requires_actual_terminal_rendering(self):
        terminal = external.NcdcTerminal()
        process = mock.Mock()
        process.poll.return_value = None
        with mock.patch.object(external, "drain_ncdc_terminal",
                               side_effect=lambda _fd, probe: probe.observe(external.NCDC_INBOUND)):
            external.await_ncdc_screen(terminal, 91, process, timeout=1)
        self.assertTrue(terminal.inbound_chat_visible)

    def test_ncdc_inbound_timeout_reports_only_safe_flags(self):
        terminal = external.NcdcTerminal()
        terminal.observe(b"sensitive-other-user")
        process = mock.Mock()
        process.poll.return_value = None
        with mock.patch.object(external, "drain_ncdc_terminal", return_value=0), mock.patch.object(
                external.time, "monotonic", side_effect=[0, 0, 2]):
            with self.assertRaises(RuntimeError) as caught:
                external.await_ncdc_screen(terminal, 91, process, timeout=1)
        self.assertIn("guest_to_ncdc_displayed=no", str(caught.exception))
        self.assertNotIn("sensitive", str(caught.exception))

    def test_ncdc_outbound_requires_independent_sender_sid(self):
        sender = b"EFGH"
        self.assertTrue(external.is_ncdc_chat_frame(
            b"BMSG EFGH stage3-ncdc-qualified", sender))
        self.assertFalse(external.is_ncdc_chat_frame(
            b"BMSG ABCD stage3-ncdc-qualified", sender))
        self.assertFalse(external.is_ncdc_chat_frame(
            b"BMSG EFGH stage3-ncdc-qualified-tampered", sender))
        self.assertFalse(external.is_ncdc_chat_frame(
            b"BMSG EFGH other message", sender))

    def test_ncdc_observer_wait_drains_terminal_between_adc_checks(self):
        reader = mock.Mock()
        reader.until.side_effect = [RuntimeError("Timed out waiting for: ncdc public chat"),
                                    b"BMSG ABCD stage3-ncdc-qualified"]
        process = mock.Mock()
        process.poll.return_value = None
        with mock.patch.object(external, "drain_ncdc_terminal", side_effect=[1, 0]) as drain:
            frame = external.await_ncdc_frame(
                reader, 91, process, lambda line: b"stage3-ncdc-qualified" in line,
                "ncdc public chat", timeout=4)
        self.assertEqual(frame, b"BMSG ABCD stage3-ncdc-qualified")
        self.assertEqual(drain.call_count, 2)

    def test_ncdc_timeout_diagnostic_never_includes_terminal_contents(self):
        reader = mock.Mock()
        reader.until.side_effect = RuntimeError("Timed out waiting for: ncdc public chat")
        process = mock.Mock()
        process.poll.return_value = None
        with mock.patch.object(external, "drain_ncdc_terminal", return_value=1), mock.patch.object(
                external.time, "monotonic", side_effect=[0, 0, 0.1, 1.1]):
            with self.assertRaises(RuntimeError) as caught:
                external.await_ncdc_frame(reader, 91, process, lambda line: False,
                                          "ncdc public chat", timeout=1)
        self.assertIn("tty_output_seen=yes", str(caught.exception))
        self.assertIn("ncdc_alive=yes", str(caught.exception))
        self.assertNotIn("stage3-ncdc-qualified", str(caught.exception))

    def test_ncdc_search_observer_accepts_broadcast_and_feature_routes(self):
        self.assertTrue(external.is_ncdc_search_frame(
            b"BSCH ABCD ANstage3ncdcqualification"))
        self.assertTrue(external.is_ncdc_search_frame(
            b"FSCH ABCD +TCP4 ANstage3ncdcqualification"))
        self.assertFalse(external.is_ncdc_search_frame(
            b"DSCH ABCD EFGH ANstage3ncdcqualification"))
        self.assertFalse(external.is_ncdc_search_frame(
            b"BSCH ABCD ANunrelated"))

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


class PremergeExternalGateTests(unittest.TestCase):
    def setUp(self):
        self.head_sha = "a" * 40
        self.env = {
            "GITHUB_EVENT_NAME": "workflow_dispatch",
            "GITHUB_REPOSITORY": premerge.REPOSITORY,
            "GITHUB_ACTOR": "adrianf87",
            "GITHUB_REF": premerge.BRANCH,
            "RUNNER_ENVIRONMENT": "github-hosted",
            "OWNER_CONFIRMATION": premerge.CONFIRMATION,
            "PRODUCTION_RESTARTS_ACK": "false",
            "PREMERGE_PHASE": "before",
            "PREMERGE_CANDIDATE_SHA": self.head_sha,
            "GITHUB_SHA": self.head_sha,
        }

    def test_before_and_after_are_both_explicitly_allowed(self):
        self.assertEqual(premerge.guard(self.env), "before")
        self.env["PREMERGE_PHASE"] = "after"
        self.assertEqual(premerge.guard(self.env), "after")

    def test_candidate_sha_must_be_exact_commit(self):
        for supplied in ("", "a" * 39, "g" * 40, "b" * 40):
            with self.subTest(supplied=supplied):
                with self.assertRaises(RuntimeError):
                    premerge.guard(dict(self.env, PREMERGE_CANDIDATE_SHA=supplied))

    def test_premerge_never_accepts_self_hosted_or_restart_ack(self):
        for override in ({"RUNNER_ENVIRONMENT": "self-hosted"},
                         {"PRODUCTION_RESTARTS_ACK": "true"}):
            with self.subTest(override=override):
                with self.assertRaises(RuntimeError):
                    premerge.guard(dict(self.env, **override))

    def test_premerge_requires_owner_repo_branch_manual_ack(self):
        for override in ({"GITHUB_ACTOR": "someone-else"},
                         {"GITHUB_REPOSITORY": "fork/dc24h.eu-twilight"},
                         {"GITHUB_REF": "refs/heads/main"},
                         {"GITHUB_EVENT_NAME": "pull_request"},
                         {"OWNER_CONFIRMATION": "RUN_STAGE3_PUBLIC_QUALIFICATION"},
                         {"PREMERGE_PHASE": "none"}):
            with self.subTest(override=override):
                with self.assertRaises(RuntimeError):
                    premerge.guard(dict(self.env, **override))


class PremergeEvidenceTests(unittest.TestCase):
    def setUp(self):
        self.candidate_sha = "a" * 40
        self.main_sha = "b" * 40
        self.before = {"source_sha": self.candidate_sha, "phase": "before",
                       "scope": evidence.EXTERNAL_SCOPE, "result": "PASS"}
        self.host = {"candidate_sha": self.candidate_sha,
                     "source_sha": self.main_sha, "scope": evidence.HOST_SCOPE,
                     "result": "PASS"}
        self.after = {"source_sha": self.candidate_sha, "phase": "after",
                      "scope": evidence.EXTERNAL_SCOPE, "result": "PASS"}

    def check(self):
        evidence.validate(self.before, self.host, self.after,
                          self.candidate_sha, self.main_sha)

    def test_valid_three_phase_evidence(self):
        self.check()

    def test_missing_failure_or_wrong_phase_rejected(self):
        for field, value in (("result", "FAIL"), ("phase", "before"),
                             ("source_sha", "c" * 40)):
            with self.subTest(field=field):
                old = self.after[field]
                self.after[field] = value
                with self.assertRaises(RuntimeError):
                    self.check()
                self.after[field] = old

    def test_host_must_be_from_exact_trusted_main_and_candidate(self):
        for field in ("source_sha", "candidate_sha", "scope"):
            with self.subTest(field=field):
                old = self.host[field]
                self.host[field] = "wrong"
                with self.assertRaises(RuntimeError):
                    self.check()
                self.host[field] = old


if __name__ == "__main__":
    unittest.main()
