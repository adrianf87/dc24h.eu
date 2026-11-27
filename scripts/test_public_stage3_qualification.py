"""Offline Stage III protocol, PTY, workflow and evidence regressions."""

import errno
import json
from pathlib import Path
import ssl
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock

import public_stage3_external_probe as base
import public_stage3_qualification_external as external
from stage3 import evidence, fixtures, public_checks
from stage3.run import public_guard, report
import stage3_premerge_gate as premerge


class FakePeer:
    def __init__(self, *chunks):
        self.chunks = list(chunks)
        self.sent = []

    def settimeout(self, _):
        pass

    def recv(self, _):
        return self.chunks.pop(0) if self.chunks else b""

    def sendall(self, value):
        self.sent.append(value)

    def close(self):
        pass


class ADCReaderTests(unittest.TestCase):
    def test_reader_retains_overshoot_and_ignores_unrelated_disconnects(self):
        peer = FakePeer(b"BMSG ABCD first\nIQUI JJJJ DIprivate\nBSCH ABCD ANsecond\n")
        reader = external.Reader(peer, b"ABCD")
        self.assertEqual(reader.until(lambda line: line.startswith(b"BMSG"), "chat"), b"BMSG ABCD first")
        self.assertEqual(reader.until(lambda line: line.startswith(b"BSCH"), "search"), b"BSCH ABCD ANsecond")
        self.assertEqual((reader.message_frames, reader.search_frames), (1, 1))

    def test_status_redacts_hub_text(self):
        reader = external.Reader(FakePeer(b"ISTA 200 Permission\\sdenied FCBINF XXprivate-token\n"))
        with self.assertRaisesRegex(evidence.Mismatch, "^ADC rejected update with ISTA 200$"):
            reader.until(lambda _: False, "update")

    def test_expected_status_is_observable(self):
        reader = external.Reader(FakePeer(b"ISTA 244 Login\\srequired\n"))
        self.assertTrue(reader.until(lambda line: line.startswith(b"ISTA 244 "), "denial"))

    def test_own_quit_is_fatal_without_disclosing_sid(self):
        reader = external.Reader(FakePeer(b"IQUI ABCD DIsecret\n"), b"ABCD")
        with self.assertRaisesRegex(evidence.Mismatch, "^Own ADC session was disconnected during chat$"):
            reader.until(lambda _: False, "chat")

    def test_login_returns_presences_before_and_after_identity(self):
        peer = FakePeer(
            b"ISUP ADBASE ADTIGR\nISID ABCD\nIINF CT32\nBINF EFGH NIexisting IDwrong\n"
            b"BINF ABCD NIprobe IDexpected\nBINF IJKL NIanother IDother\n")
        with mock.patch.object(base, "open_tls", return_value=peer):
            tls, sid, pending = base.connect_guest(None, b"probe", identity=(b"synthetic-pid", b"expected"))
        self.assertIs(tls, peer)
        self.assertEqual(sid, b"ABCD")
        self.assertEqual(pending, b"BINF EFGH NIexisting IDwrong\nBINF IJKL NIanother IDother\n")

    def test_guest_rejects_wrong_identity_disclosed_pid_or_privileged_type(self):
        for frame in (b"BINF ABCD NInotExpected IDexpected", b"BINF ABCD NIprobe IDanother",
                      b"BINF ABCD NIprobe IDexpected PDhidden", b"BINF ABCD NIprobe IDexpected CT4"):
            with self.subTest(frame=frame):
                self.assertFalse(base.guest_identity(frame, b"probe", b"expected"))

    def test_active_client_update_does_not_advertise_hub_address(self):
        one, two, replacement = (mock.Mock(sid=sid) for sid in (b"ABCD", b"EFGH", b"IJKL"))
        for peer in (one, two, replacement):
            peer.until.return_value = b"BINF " + peer.sid + b" NItest"
        with mock.patch.object(external, "guest") as guest_call, mock.patch.object(external, "see_identity"):
            guest_call.side_effect = [external.nullcontext(peer) for peer in (one, two, replacement)]
            external.check_two_client_protocol(None, None, {})
        frames = [call.args[0] for call in one.send.call_args_list]
        self.assertIn(b"BINF ABCD SUTCP4", frames)
        metadata = [frame for frame in frames if frame.startswith(b"BINF ")]
        self.assertFalse(any(b"I4" in base.inf_fields(frame) for frame in metadata))
        self.assertFalse(any(base.IPV4.encode() in frame for frame in metadata))
        self.assertTrue(any(frame.startswith(b"DCTM ABCD EFGH ") for frame in frames))


class PTYTests(unittest.TestCase):
    def test_ncdc_input_clears_line_and_submits_carriage_return(self):
        with mock.patch.object(external, "write_terminal") as write:
            external.send_ncdc(91, "test-message")
        write.assert_called_once_with(91, b"\x15test-message\r")

    def test_ncdc_tab_selection_uses_documented_alt_two(self):
        with mock.patch.object(external, "write_terminal") as write:
            external.select_ncdc_hub_tab(91)
        write.assert_called_once_with(91, b"\x1b2")

    def test_partial_and_interrupted_pty_writes(self):
        with mock.patch.object(external.select, "select", return_value=([], [91], [])), mock.patch.object(
                external.os, "write", side_effect=[2, BlockingIOError, 2]) as write:
            external.write_terminal(91, b"abcd")
        self.assertEqual([call.args[1] for call in write.call_args_list], [b"abcd", b"cd", b"cd"])

    def test_pty_write_timeout_is_explicit(self):
        with mock.patch.object(external.select, "select", return_value=([], [], [])):
            with self.assertRaisesRegex(evidence.Mismatch, "did not become writable"):
                external.write_terminal(91, b"input")

    def test_split_marker_requires_a_fresh_observation(self):
        terminal = external.NcdcTerminal()
        terminal.observe(b"actual-marker")
        terminal.watch(b"actual-marker")
        terminal.observe(b"private-actual-")
        self.assertFalse(terminal.expected[b"actual-marker"])
        terminal.observe(b"marker-private")
        self.assertTrue(terminal.expected[b"actual-marker"])
        self.assertLess(len(terminal._tail), len(b"actual-marker"))

    def test_screen_must_render_expected_marker_with_process_alive(self):
        terminal = external.NcdcTerminal()
        process = mock.Mock()
        process.poll.return_value = None
        terminal.watch(b"synthetic-marker")
        with mock.patch.object(external, "drain_ncdc_terminal", side_effect=[
            None, terminal.observe(b"private-unrelated")
        ]), mock.patch.object(external.time, "monotonic", side_effect=[0, 0, 0.25, 2]):
            with self.assertRaisesRegex(evidence.Mismatch, "^ncdc terminal update timed out; output activity=True$"):
                external.await_ncdc_screen(terminal, 91, process, b"synthetic-marker", timeout=1)
        process.poll.return_value = 2
        with self.assertRaisesRegex(evidence.Mismatch, "ncdc exited"):
            external.await_ncdc_screen(terminal, 91, process, b"synthetic-marker", timeout=1)

    def test_drain_detects_terminal_eof(self):
        with mock.patch.object(external.select, "select", return_value=([91], [], [])), mock.patch.object(
                external.os, "read", return_value=b""):
            with self.assertRaisesRegex(evidence.Mismatch, "terminal output ended"):
                external.drain_ncdc_terminal(91, external.NcdcTerminal())

    def test_independent_frame_poll_drains_pty_between_adc_waits(self):
        terminal = external.NcdcTerminal()
        reader, process = mock.Mock(), mock.Mock()
        process.poll.return_value = None
        reader.until.side_effect = [external.FrameTimeout, b"BMSG ABCD synthetic"]
        with mock.patch.object(external, "drain_ncdc_terminal") as drain:
            frame = external.await_ncdc_frame(reader, 91, process, lambda _: True, "chat", 1, terminal)
        self.assertEqual(frame, b"BMSG ABCD synthetic")
        self.assertEqual(drain.call_count, 2)

    def test_observer_timeout_excludes_terminal_content(self):
        terminal = external.NcdcTerminal()
        terminal.observe(b"private-other-user")
        reader, process = mock.Mock(message_frames=1, search_frames=1), mock.Mock()
        process.poll.return_value = None
        reader.until.side_effect = external.FrameTimeout
        with mock.patch.object(external, "drain_ncdc_terminal"), mock.patch.object(
                external.time, "monotonic", side_effect=[0, 0, 0.1, 1.1]):
            with self.assertRaisesRegex(evidence.Mismatch, "output activity=True") as caught:
                external.await_ncdc_frame(reader, 91, process, lambda _: False, "chat", 1, terminal)
        self.assertNotIn("private-other-user", str(caught.exception))

    def test_attribution_requires_exact_sid_and_marker(self):
        frame = b"BMSG ABCD synthetic"
        self.assertTrue(external.attributed(frame, b"BMSG", b"synthetic", frame))
        self.assertFalse(external.attributed(b"BMSG ABCD synthetic-extra", b"BMSG", b"synthetic", frame))
        with self.assertRaises(evidence.Mismatch):
            external.attributed(b"BMSG EFGH synthetic", b"BMSG", b"synthetic", frame)
        for sender, marker, matched in ((b"ABCD", b"synthetic", True), (b"EFGH", b"synthetic", False),
                                        (b"ABCD", b"synthetic-extra", False)):
            self.assertEqual(external.is_ncdc_search_frame(b"FSCH " + sender + b" +TCP4 AN" + marker,
                                                          b"ABCD", b"synthetic"), matched)

    def test_ncdc_pin_is_derived_from_verified_der(self):
        context = mock.MagicMock()
        peer = mock.MagicMock()
        peer.getpeercert.return_value = b"sample public DER"
        with mock.patch.object(base, "open_tls") as connect:
            connect.return_value.__enter__.return_value = peer
            url = external.fingerprint_url(context)
        self.assertEqual(url, f"adcs://{base.FQDN}:1512/?kp=SHA256/{external.keyprint(b'sample public DER')}")
        connect.assert_called_once_with(context, (base.IPV4, base.ADCS_PORT), base.FQDN)


class PolicyTests(unittest.TestCase):
    def test_forward_confirmed_ptr_requires_exact_reverse_and_forward_pair(self):
        expected = [(2, 1, 6, "", (base.IPV4, 0))]
        with mock.patch.object(public_checks.socket, "gethostbyaddr",
                               return_value=(base.FQDN + ".", [], [base.IPV4])), mock.patch.object(
                               public_checks.socket, "getaddrinfo", return_value=expected):
            self.assertIn("Forward-confirmed", public_checks.forward_confirmed_ptr())

        with mock.patch.object(public_checks.socket, "gethostbyaddr",
                               return_value=("unapproved.example", [], [base.IPV4])):
            with self.assertRaisesRegex(evidence.Mismatch, "PTR does not match"):
                public_checks.forward_confirmed_ptr()

        with mock.patch.object(public_checks.socket, "gethostbyaddr",
                               return_value=(base.FQDN, [], [base.IPV4])), mock.patch.object(
                               public_checks.socket, "getaddrinfo",
                               return_value=[(2, 1, 6, "", ("203.0.113.99", 0))]):
            with self.assertRaisesRegex(evidence.Mismatch, "resolve exclusively"):
                public_checks.forward_confirmed_ptr()

        with mock.patch.object(public_checks.socket, "gethostbyaddr",
                               side_effect=OSError("private DNS resolver failure")):
            with self.assertRaisesRegex(evidence.Blocked, "PTR lookup is unavailable") as failure:
                public_checks.forward_confirmed_ptr()
            self.assertNotIn("private DNS", str(failure.exception))

    def test_bounded_ports_distinguish_refusal_timeout_and_network_error(self):
        self.assertEqual(external.FORBIDDEN_TCP, (1511, 3000, 3306, 5173, 5432, 6379, 8000, 8080, 9090))
        for error, expected in ((ConnectionRefusedError, "actively refused"), (TimeoutError, "timed out")):
            with mock.patch.object(public_checks.socket, "create_connection", side_effect=error):
                self.assertIn(expected, public_checks.port_observation(1511))
        with mock.patch.object(public_checks.socket, "create_connection",
                               side_effect=OSError(errno.ENETUNREACH, "unreachable")):
            with self.assertRaises(evidence.Blocked):
                public_checks.port_observation(1511)
        peer = mock.MagicMock()
        with mock.patch.object(public_checks.socket, "create_connection", return_value=peer):
            with self.assertRaisesRegex(evidence.Mismatch, "publicly accessible"):
                public_checks.port_observation(1511)
        peer.__exit__.assert_called_once()

    def setUp(self):
        self.sha = "a" * 40
        self.owner_env = {
            "GITHUB_ACTIONS": "true", "GITHUB_EVENT_NAME": "workflow_dispatch",
            "GITHUB_REPOSITORY": premerge.REPOSITORY, "GITHUB_ACTOR": "adrianf87",
            "GITHUB_TRIGGERING_ACTOR": "adrianf87", "GITHUB_REF": "refs/heads/a-future-candidate",
            "RUNNER_ENVIRONMENT": "github-hosted", "PRODUCTION_RESTARTS_ACK": "false",
            "GITHUB_SHA": self.sha, "PREMERGE_CANDIDATE_SHA": self.sha,
        }

    def test_external_snapshots_require_owner_ref_exact_sha_and_mode(self):
        env = dict(self.owner_env, OWNER_CONFIRMATION=premerge.CONFIRMATION, PREMERGE_PHASE="before")
        self.assertEqual(premerge.guard(env), "before")
        self.assertEqual(premerge.guard(dict(env, PREMERGE_PHASE="after")), "after")
        for changed in ({"RUNNER_ENVIRONMENT": "other"}, {"PREMERGE_PHASE": "full"},
                        {"PRODUCTION_RESTARTS_ACK": "true"}, {"GITHUB_TRIGGERING_ACTOR": "other"},
                        {"GITHUB_REF": "refs/tags/unreviewed"}, {"PREMERGE_CANDIDATE_SHA": "b" * 40}):
            with self.subTest(changed=changed), self.assertRaises(RuntimeError):
                premerge.guard(dict(env, **changed))

    def test_extended_requires_exact_token_sha_owner_and_no_host_operations(self):
        env = dict(self.owner_env, OWNER_CONFIRMATION="RUN_STAGE3_PUBLIC_EXTENDED_STABILITY",
                   PREMERGE_PHASE="none")
        with mock.patch("stage3.run.checkout_sha", return_value=self.sha):
            with mock.patch.dict("os.environ", env, clear=True):
                public_guard("extended")
            for changed in ({"GITHUB_ACTOR": "other"}, {"GITHUB_TRIGGERING_ACTOR": "other"},
                            {"PREMERGE_PHASE": "after"}, {"PREMERGE_CANDIDATE_SHA": "b" * 40},
                            {"OWNER_CONFIRMATION": premerge.CONFIRMATION}, {"PRODUCTION_RESTARTS_ACK": "true"},
                            {"GITHUB_EVENT_NAME": "pull_request"}, {"GITHUB_REF": "refs/tags/release"}):
                with self.subTest(changed=changed), mock.patch.dict("os.environ", dict(env, **changed), clear=True):
                    with self.assertRaises(evidence.Blocked):
                        public_guard("extended")

    def test_public_checks_require_explicit_owner_dispatch(self):
        env = dict(self.owner_env, OWNER_CONFIRMATION=premerge.CONFIRMATION, PREMERGE_PHASE="before",
                   GITHUB_REF="refs/heads/main", GITHUB_REF_PROTECTED="true")
        with mock.patch("stage3.run.checkout_sha", return_value=self.sha):
            with mock.patch.dict("os.environ", env, clear=True):
                public_guard("tls-public")
            for event in ("push", "pull_request"):
                with self.subTest(event=event), mock.patch.dict(
                    "os.environ", dict(env, GITHUB_EVENT_NAME=event), clear=True
                ):
                    with self.assertRaisesRegex(evidence.Blocked, "explicit OWNER dispatch"):
                        public_guard("tls-public")


class CertificateVerificationTests(unittest.TestCase):
    def test_foreign_ca_fixture_has_its_own_issuer_and_valid_leaf(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            trusted = fixtures.authority(root / "trusted").parent / "ca.pem"
            issuer = fixtures.authority(root / "other")
            foreign = fixtures.issue(root, issuer, "foreign")
            with fixtures.certificate_server(foreign) as settings:
                observed = fixtures.verify_certificate_rejection(fixtures.loopback_context(trusted), settings, 20)
                self.assertEqual(observed, "OpenSSL verification code 20 (local issuer not found)")
            with fixtures.certificate_server(foreign) as settings:
                with base.open_tls(fixtures.loopback_context(issuer.parent / "ca.pem"), **settings):
                    pass

    def test_certificate_diagnostics_use_only_the_code_and_controlled_category(self):
        for code, category in (
            (9, "certificate not yet valid"), (10, "certificate expired"),
            (20, "local issuer not found"), (62, "DNS hostname mismatch"),
        ):
            with self.subTest(code=code):
                error = ssl.SSLCertVerificationError("sensitive certificate detail")
                error.verify_code = code
                with mock.patch.object(base, "open_tls", side_effect=error):
                    self.assertEqual(fixtures.verify_certificate_rejection(None, {}, code),
                                     f"OpenSSL verification code {code} ({category})")

    def test_wrong_reason_generic_error_and_accepted_leaf_fail(self):
        error = ssl.SSLCertVerificationError("sensitive certificate detail")
        error.verify_code = 30
        with mock.patch.object(base, "open_tls", side_effect=error):
            with self.assertRaisesRegex(evidence.Mismatch, "code 30 .*identifier mismatch.*Expected 20") as failure:
                fixtures.verify_certificate_rejection(None, {}, 20)
            self.assertNotIn("sensitive certificate detail", str(failure.exception))
        with mock.patch.object(base, "open_tls", side_effect=ssl.SSLError("generic handshake error")):
            with self.assertRaises(ssl.SSLError):
                fixtures.verify_certificate_rejection(None, {}, 20)
        with mock.patch.object(base, "open_tls"):
            with self.assertRaisesRegex(evidence.Mismatch, "accepted a deliberately invalid certificate"):
                fixtures.verify_certificate_rejection(None, {}, 20)


class EvidenceTests(unittest.TestCase):
    def test_automatic_ci_evidence_records_public_checks_out_of_scope(self):
        with tempfile.TemporaryDirectory() as work:
            directory = Path(work)
            sha = "a" * 40
            expected_groups = {(group, image) for group in evidence.RUNNER_GROUPS for image in evidence.OS_MATRIX}
            for mode in ("pr", "main"):
                with self.subTest(mode=mode), mock.patch("stage3.run.checkout_sha", return_value=sha):
                    args = SimpleNamespace(mode=mode, sha=sha, run_id="42", attempt="1",
                                           input=directory / "missing", output=directory / mode)
                    self.assertEqual(report(args), 1)
                    result = json.loads((args.output / "qualification.json").read_text())
                    self.assertEqual(result["public_checks_out_of_scope"],
                                     dict.fromkeys(evidence.PUBLIC_GROUPS, "OUT OF SCOPE BY OWNER"))
                    self.assertEqual({(row["category"], row["runner_image"]) for row in result["statuses"]},
                                     expected_groups)
                    self.assertEqual({row["result"] for row in result["statuses"]}, {"BLOCKED"})

    def test_failure_blocks_missing_rows_and_artifacts_reject_tampering(self):
        with tempfile.TemporaryDirectory() as work:
            directory = Path(work)
            suite = evidence.Suite("sandbox", directory, "a" * 40, "ubuntu-22.04", verify_checkout=False)
            with suite:
                with suite.case("ubuntu.sudo"):
                    raise RuntimeError("untrusted raw secret")
            rows = evidence.verify_bundle(directory, "sandbox", "ubuntu-22.04", "a" * 40, suite.context["run_id"])
            self.assertEqual([row["result"] for row in rows], ["FAIL", "BLOCKED"])
            self.assertNotIn("untrusted raw secret", (directory / "results.json").read_text())
            with self.assertRaisesRegex(ValueError, "identity mismatch"):
                evidence.verify_bundle(directory, "sandbox", "ubuntu-22.04", "b" * 40, suite.context["run_id"])
            (directory / "summary.log").write_text("substituted")
            with self.assertRaisesRegex(ValueError, "SHA-256 manifest mismatch"):
                evidence.verify_bundle(directory, "sandbox", "ubuntu-22.04", "a" * 40, suite.context["run_id"])

    def test_qualification_workflows_use_only_approved_hosted_ubuntu(self):
        workflows = Path(__file__).resolve().parent.parent / ".github/workflows"
        for name in ("debian13-stage3-full-qualification.yml", "public-stage3-qualification.yml"):
            document = (workflows / name).read_text()
            self.assertNotRegex(document, r"(?m)^\s*runs-on: (?!ubuntu-22\.04$|\$\{\{ matrix\.os \}\}$)")
            if name == "public-stage3-qualification.yml":
                self.assertIn("os: [ubuntu-22.04, ubuntu-24.04, ubuntu-26.04]", document)
            self.assertNotIn("self-hosted", document)
            self.assertNotIn("sudo -n python", document)
            self.assertNotIn("sudo -n bash", document)
            self.assertNotIn("systemctl restart", document)
            self.assertNotIn("persist-credentials: true", document)
            self.assertIn("if-no-files-found: error", document)


if __name__ == "__main__":
    unittest.main()
