#!/usr/bin/env python3
"""Independent ZIP-byte and redacted-report verification for beta-0.9.

Read-only GitHub REST metadata is the trust root. Run only after *all* required
workflow attempts complete at the final head; this tool has no merge/deploy path.
"""

import argparse
import hashlib
import io
import json
import math
import os
from pathlib import Path
import re
import stat
import subprocess
import urllib.error
import urllib.parse
import urllib.request
import zipfile

import beta08_github_evidence as prior
import beta07_ubuntu_perf_matrix as perf
import beta09_os_provenance as host


BRANCH = "beta-0.9.x-stage-full"
PR = 243
MAX_ARCHIVE = 16 * 1024 * 1024
MAX_ENTRY = 256 * 1024
MAX_UNCOMPRESSED = 4 * 1024 * 1024
MAX_ENTRIES = 64
SHA = re.compile(r"[0-9a-f]{40}\Z")
ARTIFACT_FILE = re.compile(r"[a-z][a-z0-9_.-]{1,95}\.json\Z")
FORBIDDEN_FIELD = re.compile(r"(?:password|cookie|authorization|access_token|refresh_token|"
                             r"totp_secret|private_key|recovery_secret|raw_sql|raw_trace|"
                             r"actor_id|ip_address|credential_secret)\Z", re.I)

# Static, reviewable file inventory for the existing redacted upload folders.
# A new producer must be reviewed before its output is accepted, not silently
# swept in by a permissive `*.json` archive extractor.
COMMON = {
    "summary.json", "provenance.json", "beta07-supply-chain.json",
    "beta07-client-capabilities.json", "beta08-stage1-interoperability.json",
    "beta09-stage1-interoperability.json", "beta09-os-provenance.json",
    "disposable-db-cli-restore.json", "loopback-probes.json", "stage4-endurance.json",
    "stage4-https-endurance.json", "stage4-failure-rollback.json",
    "stage1-matched-recovery.json", "beta06-authenticated-https.json",
    "beta06-authenticated-gate.json", "beta06-restored-login.json",
    "beta06-previous-version-rollback.json", "beta06-integrated-chaos.json",
    "beta07-adc-adcs-windows.json", "beta07-bounded-faults.json",
    "beta07-dpkg-rollback.json", "beta07-transport-performance.json",
    "beta08-transport-evidence.json", "beta08-stage2-authentication.json",
    "timing-summary.json", "beta09-security-envelope.json",
    "beta09-systemd-node.json", "beta09-stage3-gate.json",
}
LEGACY_UNBOUND = {"loopback-probes.json", "disposable-db-cli-restore.json"}


def _file_policy(artifact_name):
    if artifact_name.startswith("cpp-"):
        return COMMON, {"summary.json", "provenance.json", "beta07-supply-chain.json",
                        "beta09-stage1-interoperability.json", "beta09-os-provenance.json"}
    if artifact_name.startswith("stage4-ubuntu-vm-"):
        return COMMON, {"beta07-bounded-faults.json", "beta08-transport-evidence.json",
                        "beta09-os-provenance.json", "beta09-security-envelope.json",
                        "beta09-systemd-node.json", "beta09-stage3-gate.json"}
    if artifact_name.startswith("beta07-npm-audit-"):
        return {"beta07-npm-audit.json"}, {"beta07-npm-audit.json"}
    if artifact_name.startswith("beta08-build-audit-"):
        return {"beta08-build-chain-audit.json"}, {"beta08-build-chain-audit.json"}
    if artifact_name.startswith("sanitizer-"):
        return {"summary.json"}, {"summary.json"}
    if artifact_name.startswith("tsan-evidence-"):
        return {"tsan-summary.json", "tsan-provenance.json"}, {"tsan-provenance.json"}
    if artifact_name.startswith("fuzz-evidence-"):
        return {"fuzz-smoke.json", "fuzz-fragments-smoke.json", "beta09-fuzz-envelope.json"}, {
            "fuzz-smoke.json", "fuzz-fragments-smoke.json", "beta09-fuzz-envelope.json"}
    raise ValueError("Unreviewed artifact family")


def _no_sensitive_fields(value, depth=0):
    if depth > 16:
        raise ValueError("Unbounded report nesting")
    if isinstance(value, dict):
        if len(value) > 4096:
            raise ValueError("Oversized report object")
        for key, child in value.items():
            if not isinstance(key, str) or FORBIDDEN_FIELD.fullmatch(key):
                raise ValueError("Unreviewed sensitive report field")
            _no_sensitive_fields(child, depth + 1)
    elif isinstance(value, list):
        if len(value) > 4096:
            raise ValueError("Oversized report collection")
        for child in value:
            _no_sensitive_fields(child, depth + 1)
    elif isinstance(value, str):
        if (len(value) > 8192 or "-----BEGIN PRIVATE" in value or
                "Authorization: Bearer " in value or "Set-Cookie:" in value):
            raise ValueError("Potential secret or raw trace in report")
    elif value is not None:
        if type(value) not in (int, float, bool) or (
                type(value) is float and not math.isfinite(value)):
            raise ValueError("Unsupported or nonfinite report value")


def _report_semantics(name, report):
    if name == "beta09-os-provenance.json":
        if (report.get("runner_environment") != "github-hosted" or
                report.get("fully_reproducible_os_build") is not False or
                report.get("apt_repository_snapshot_immutable") is not False or
                not isinstance(report.get("selected_public_apt_versions"), dict) or
                set(report["selected_public_apt_versions"]) != set(host.REQUIRED_APT) or
                not re.fullmatch(r"[0-9a-f]{64}", str(report.get("apt_full_installed_inventory_sha256"))) or
                not re.fullmatch(r"[0-9a-f]{64}", str(report.get("npm_lock_sha256")))):
            raise ValueError("Missing or exaggerated GitHub-hosted build/apt provenance")
    if name == "beta08-build-chain-audit.json":
        if (report.get("runtime_and_build_chain_audited") is not True or
                report.get("scope_enforced_by_invoking_npm") is not True or
                type(report.get("unreviewed_high_or_critical")) is not int or
                report["unreviewed_high_or_critical"] != 0 or
                report.get("dependency_exceptions") != [] or
                report.get("raw_advisories_exported") is not False):
            raise ValueError("Missing actual production+dev npm advisory coverage")
    if name == "beta09-security-envelope.json":
        profiles = report.get("profiles")
        if (not isinstance(profiles, dict) or set(profiles) != set(perf.POLICY) or
                report.get("within_profile_regression_envelope_passed") is not True or
                report.get("all_required_faults_bounded") is not True or
                report.get("across_protocol_capacity_equivalence") is not False or
                report.get("production_sla_or_capacity_approved") is not False):
            raise ValueError("Missing bounded three-transport CI envelope")
        for profile, policy in perf.POLICY.items():
            values = profiles[profile]
            if (not isinstance(values, dict) or values.get("windows") != 3 or
                    values.get("samples") != 3 * policy["requests"] or
                    values.get("failed_requests") != 0 or
                    values.get("ci_regression_envelope_passed") is not True):
                raise ValueError("Incomplete/nonfinite transport window evidence")
    if name == "beta09-stage3-gate.json":
        if (not all(report.get(field) is True for field in (
                "beta_0_9_9_old_installed_cpp_auth_browser_verified",
                "beta_0_9_10_private_dirfd_regressions_required_in_unit_job",
                "beta_0_9_11_hmac_six_component_recovery_verified",
                "beta_0_9_12_actual_systemd_cpp_and_node_ci_verified")) or
                report.get("production_deployment_or_rollback_proven") is not False):
            raise ValueError("Missing genuine old-binary recovery and authentication evidence")
    if name == "beta09-systemd-node.json":
        if (not all(report.get(field) is True for field in (
                "compiled_node_production_only_dependencies", "actual_systemd_start_restart_stop",
                "unprivileged_systemd_service", "loopback_only_listener",
                "private_owner_only_five_file_state", "hardened_systemd_unit_properties")) or
                report.get("production_service_changed") is not False):
            raise ValueError("Incomplete isolated Node systemd lifecycle evidence")
    if name == "beta09-fuzz-envelope.json":
        if (report.get("both_required_fuzz_checks_passed") is not True or
                report.get("distinct_fuzzer_profiles") != ["adc_parser", "adc_fragments"] or
                report.get("required_runs") != 2048 or
                type(report.get("observed_runs")) is not int or
                report["observed_runs"] < 2048 or report.get("sanitizer_findings") != 0 or
                report.get("raw_inputs_exported") is not False):
            raise ValueError("Incomplete or falsely green dual fuzz profiles")
    if name == "tsan-provenance.json":
        if (report.get("result") != "passed" or report.get("suite") != "beta08-tsan" or
                type(report.get("tests")) is not int or report["tests"] < 8):
            raise ValueError("TSAN evidence missing required new routing/concurrency cases")


def inspect_archive(zipped, artifact, sha):
    if (not isinstance(zipped, bytes) or not 0 < len(zipped) <= MAX_ARCHIVE or
            not isinstance(artifact, dict) or not SHA.fullmatch(sha) or
            artifact.get("size_in_bytes") != len(zipped) or
            not isinstance(artifact.get("digest"), str) or
            hashlib.sha256(zipped).hexdigest() != artifact["digest"].removeprefix("sha256:")):
        raise ValueError("Missing ZIP bytes or GitHub artifact SHA-256 mismatch")
    allowed, required = _file_policy(artifact.get("name", ""))
    with zipfile.ZipFile(io.BytesIO(zipped), "r") as archive:
        files = archive.infolist()
        if not 1 <= len(files) <= MAX_ENTRIES:
            raise ValueError("Missing or excessive archive entries")
        seen = set()
        total = 0
        unbound = 0
        for info in files:
            name = info.filename
            if (not ARTIFACT_FILE.fullmatch(name) or name not in allowed or name in seen or
                    info.is_dir() or info.flag_bits & 1 or
                    stat.S_IFMT(info.external_attr >> 16) == stat.S_IFLNK or
                    not 0 < info.file_size <= MAX_ENTRY or
                    info.compress_size <= 0 or info.file_size > info.compress_size * 100):
                raise ValueError("Unreviewed path, symlink, ZIP bomb or raw report")
            seen.add(name)
            total += info.file_size
            if total > MAX_UNCOMPRESSED:
                raise ValueError("Excessive decompressed report bytes")
            raw = archive.read(info, pwd=None)
            if len(raw) != info.file_size:
                raise ValueError("Corrupt ZIP report length")
            report = json.loads(raw.decode("utf-8"), parse_constant=lambda x: (_ for _ in ()).throw(ValueError(x)))
            if not isinstance(report, dict) or report.get("schema") != 1:
                raise ValueError("Malformed redacted report")
            identities = [report[key] for key in ("source_sha", "commit_sha", "head_sha") if key in report]
            if not identities:
                if name not in LEGACY_UNBOUND:
                    raise ValueError("Redacted report lacks exact-head identity")
                unbound += 1
            elif any(identity != sha for identity in identities):
                raise ValueError("Wrong source head inside genuine ZIP bytes")
            _no_sensitive_fields(report)
            _report_semantics(name, report)
        if not required.issubset(seen):
            raise ValueError("Missing mandatory redacted report from GitHub archive")
        return {"reports": len(seen), "uncompressed_bytes": total,
                "legacy_unbound_reports": unbound}


def _verify_pr(get, sha):
    pr = get(f"/pulls/{PR}")
    if (not isinstance(pr, dict) or pr.get("state") != "open" or
            not isinstance(pr.get("head"), dict) or
            pr["head"].get("ref") != BRANCH or pr["head"].get("sha") != sha or
            pr["head"].get("repo", {}).get("full_name") != prior.REPO or
            not isinstance(pr.get("base"), dict) or
            pr["base"].get("ref") != "main" or
            pr["base"].get("repo", {}).get("full_name") != prior.REPO):
        raise ValueError("Pull request does not point to exact current source HEAD")


def evaluate(get, download, sha, mode="pr"):
    if not isinstance(sha, str) or not SHA.fullmatch(sha) or mode not in ("pr", "main"):
        raise ValueError("Invalid final source SHA or review mode")
    branch, event = (BRANCH, "pull_request") if mode == "pr" else ("main", "push")
    if mode == "pr":
        _verify_pr(get, sha)
    count, size, reports, unbound = 0, 0, 0, 0

    def check(artifact, _workflow):
        nonlocal count, size, reports, unbound
        data = download(artifact["id"])
        checked = inspect_archive(data, artifact, sha)
        count += 1
        size += len(data)
        reports += checked["reports"]
        unbound += checked["legacy_unbound_reports"]

    metadata = prior.evaluate(get, sha, branch=branch, event=event, artifact_callback=check)
    if mode == "pr":
        _verify_pr(get, sha)
    return {"schema": 1, "source_sha": sha, "mode": mode, "branch": branch,
            "github_api_metadata_verified": True,
            "downloaded_artifact_count": count, "verified_zip_bytes": size,
            "redacted_reports_parsed": reports, "legacy_reports_without_embedded_sha": unbound,
            "artifact_sha256_independently_verified": True,
            "raw_credentials_exported": False, "owner_release_approval": False,
            "merge_authorized": False, "production_deployment_authorized": False,
            "workflow_run_ids": {name: {"id": item["run_id"], "attempt": item["attempt"]}
                                 for name, item in metadata["verified_workflows"].items()}}


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, request, fp, code, message, headers, newurl):
        return None


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--sha", required=True)
    parser.add_argument("--mode", choices=("pr", "main"), default="pr")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(argv)
    token = os.environ.get("GITHUB_TOKEN", "")
    if not token or len(token) > 2048:
        parser.exit(1, "Read-only GitHub token required; NO-GO.\n")
    try:
        actual = subprocess.check_output(["git", "-C", str(args.root), "rev-parse", "HEAD"],
                                         timeout=10).decode("ascii").strip()
        if actual != args.sha:
            raise ValueError("Wrong checkout")
        base = "https://api.github.com/repos/" + prior.REPO
        headers = {"Authorization": "Bearer " + token,
                   "Accept": "application/vnd.github+json",
                   "X-GitHub-Api-Version": "2022-11-28",
                   "User-Agent": "dc24h-beta09-artifact-verifier"}

        def get(path):
            with urllib.request.urlopen(urllib.request.Request(base + path, headers=headers),
                                        timeout=15) as response:
                data = response.read(2 * 1024 * 1024 + 1)
                if response.status != 200 or len(data) > 2 * 1024 * 1024:
                    raise ValueError("Unbounded or missing GitHub API metadata")
                return json.loads(data)

        def download(artifact_id):
            if type(artifact_id) is not int or artifact_id <= 0:
                raise ValueError("Invalid GitHub artifact ID")
            opener = urllib.request.build_opener(NoRedirect)
            try:
                opener.open(urllib.request.Request(
                    base + f"/actions/artifacts/{artifact_id}/zip", headers=headers), timeout=15)
                raise ValueError("Artifact URL returned no expected signed download redirect")
            except urllib.error.HTTPError as error:
                if error.code not in (302, 303, 307):
                    raise
                location = error.headers.get("Location", "")
            parsed = urllib.parse.urlparse(location)
            host = parsed.hostname or ""
            if (parsed.scheme != "https" or parsed.username is not None or
                    not (host.endswith(".blob.core.windows.net") or
                         host.endswith(".amazonaws.com") or
                         host.endswith(".githubusercontent.com"))):
                raise ValueError("Untrusted artifact redirect host")
            # Never forward the GitHub Bearer credential to object storage.
            with urllib.request.urlopen(urllib.request.Request(
                    location, headers={"User-Agent": "dc24h-beta09-artifact-verifier"}),
                    timeout=30) as response:
                data = response.read(MAX_ARCHIVE + 1)
                if len(data) > MAX_ARCHIVE:
                    raise ValueError("Artifact archive is oversized")
                return data

        report = evaluate(get, download, args.sha, args.mode)
        with args.output.open("x", encoding="utf8") as stream:
            json.dump(report, stream, sort_keys=True)
            stream.write("\n")
    except (OSError, ValueError, TypeError, KeyError, zipfile.BadZipFile,
            UnicodeError, json.JSONDecodeError, subprocess.SubprocessError,
            urllib.error.URLError):
        parser.exit(1, "Independent final-head artifact verification incomplete; NO-GO.\n")
    print("GitHub artifact ZIP bytes and allowlisted reports verified; Owner approval still required")


if __name__ == "__main__":
    raise SystemExit(main())
