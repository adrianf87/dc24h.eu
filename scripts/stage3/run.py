"""Entrypoint for each Stage III job and the exact-head evidence gate."""

import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
import xml.etree.ElementTree as ET

from public_stage3_qualification_external import check_real_ncdc, check_two_client_protocol, public_context, require
import stage3_premerge_gate as premerge
from stage3 import fixtures, public_checks
from stage3.evidence import (
    CASES, MANUAL, OS_MATRIX, PUBLIC_GROUPS, RUNNER_GROUPS, Blocked, Mismatch,
    Suite, checkout_sha, command_version, digest, invalidate_credentials, now, verify_bundle, write_manifest,
)

REPOSITORY = "adrianf87/dc24h.eu-twilight"
BRANCH = "test/stage3-public-adcs-final-qualification"
ARTIFACT_PREFIX = "stage3"
WORKFLOWS = {
    "pr": "Stage III public ADCS qualification",
    "extended": "Stage III OWNER extended public ADCS stability",
}


def execute(*args, **kwargs):
    return subprocess.run(args, capture_output=True, check=True, **kwargs)


def offline(suite, build, private, parallel, *, local=False):
    with suite.case("cmake.configure"):
        execute("cmake", "-S", ".", "-B", str(build), "-DCMAKE_BUILD_TYPE=Release",
                "-DBUILD_TESTING=ON", "-DDC24H_REQUIRE_INTEGRATION_TESTS=ON", timeout=100)
    with suite.case("cmake.build"):
        execute("cmake", "--build", str(build), "--parallel", str(parallel), timeout=5400)
    binary = build / "dc24h-twilight"
    with suite.case("binary.identity") as case:
        sha256 = digest(binary)
        version = command_version(str(binary), "--version")
        require(version != "unavailable", "Hub binary did not return a version")
        require(checkout_sha() == suite.source_sha, "Checked-out source identity changed")
        case.observed = version + "; binary SHA-256 " + sha256
        if not local:
            binary_dir = suite.output.parent / "binary"
            binary_dir.mkdir(parents=True, exist_ok=True)
            (binary_dir / "binary.json").write_text(json.dumps({
                "source_sha": suite.source_sha, "sha256": sha256, "version": version,
                "image": suite.image, "run_id": suite.context["run_id"],
                "run_attempt": suite.context["run_attempt"],
            }) + "\n")
            from shutil import copyfile
            copyfile(binary, binary_dir / "dc24h-twilight")

    with suite.case("ctest.inventory") as case:
        listed = json.loads(execute("ctest", "--test-dir", str(build), "--show-only=json-v1",
                                    timeout=30).stdout)
        names = {test["name"] for test in listed["tests"]}
        require(len(names) >= 250 and {"systemd.sandbox", "adc.ncdc", "adcs.ncdc"} <= names,
                "Required CTest inventory was incomplete")
        raw_junit = private / "ctest.raw.xml"
        start = now()
        command = ["ctest", "--test-dir", str(build), "--parallel", str(parallel),
                   "--output-junit", str(raw_junit)]
        if local:
            # Privileged manager fixtures belong to the ephemeral Ubuntu job.
            command += ["-E", r"^systemd\.sandbox$"]
        process = subprocess.run(command, capture_output=True, timeout=2100)
        end = now()
        rows = {}
        if raw_junit.exists():
            for item in ET.parse(raw_junit).iter("testcase"):
                name = item.attrib["name"]
                require(name in names and name not in rows, "CTest reported an unexpected or duplicate case")
                rows[name] = item
        for name in sorted(names):
            result = rows.get(name)
            if result is None:
                status, reason = "NOT TESTED", "CTest result was absent (check local restrictions and prerequisites)"
            elif result.find("failure") is not None or result.find("error") is not None:
                status, reason = "FAIL", "CTest returned failure"
            elif (result.find("skipped") is not None or result.get("status") in {"notrun", "disabled"} or
                  re.search(r"\bskipped=[1-9][0-9]*\b", result.findtext("system-out", ""))):
                status, reason = "NOT TESTED", "CTest or a required subprocess reported a skip"
            elif result.get("status") == "run":
                elapsed = float(result.get("time", 0))
                status, reason = "PASS", f"CTest completed successfully in {elapsed:.3f}s"
            else:
                status, reason = "FAIL", "CTest reported an unknown case status"
            suite.record("ctest." + name, status, reason, start=start, end=end,
                         expected="Configured CTest regression completes without a failure or skip")
        require(process.returncode == 0 or any(row["result"] == "FAIL" for row in suite.records),
                "CTest exited unsuccessfully without actionable per-case results")
        case.observed = f"{len(rows)} results for {len(names)} configured tests; timestamps bracket the CTest invocation"
    with suite.case("policy.regressions"):
        execute("python3", "-m", "unittest", "discover", "-s", "scripts", "-p", "test_public_stage3*.py", timeout=100)


def sandbox(suite, sudo_result):
    with suite.case("ubuntu.sudo"):
        if sudo_result != "success":
            raise Blocked("Fixed Ubuntu sudo/systemd fixture failed or was not run")
    with suite.case("systemd.verify"):
        with tempfile.TemporaryDirectory(prefix="stage3-unit-") as work:
            unit = Path(work) / "dc24h-twilight.service"
            text = Path("config/dc24h-twilight.service.example").read_text()
            require(text.count("ExecStart=/usr/local/bin/dc24h-twilight ") == 1,
                    "Service example ExecStart was unexpected")
            unit.write_text(text.replace("ExecStart=/usr/local/bin/dc24h-twilight ", "ExecStart=/bin/true ", 1))
            verified = execute("systemd-analyze", "verify", str(unit), timeout=20)
            require(b"Failed" not in verified.stderr, "systemd-analyze reported failed unit validation")


def verified_binary(suite, directory):
    binary = directory / "dc24h-twilight"
    manifest = json.loads((directory / "binary.json").read_text())
    require(manifest["source_sha"] == suite.source_sha and manifest["image"] == suite.image
            and manifest["run_id"] == suite.context["run_id"]
            and manifest["run_attempt"] == suite.context["run_attempt"] and digest(binary) == manifest["sha256"],
            "Downloaded hub binary did not match exact job and SHA-256")
    binary.chmod(0o700)
    suite.tools["hub"] = manifest["version"] + "; SHA-256 " + manifest["sha256"]
    return str(binary.resolve())


def public_guard(group):
    env = os.environ
    if not (env.get("GITHUB_ACTIONS") == "true" and env.get("RUNNER_ENVIRONMENT") == "github-hosted"
            and env.get("GITHUB_REPOSITORY") == REPOSITORY):
        raise Blocked("Public qualification requires an independent GitHub-hosted runner")
    if env.get("GITHUB_EVENT_NAME") == "workflow_dispatch" and group != "extended":
        try:
            premerge.guard(env)
        except RuntimeError:
            raise Blocked("OWNER external snapshot guard failed") from None
        if checkout_sha() != env.get("GITHUB_SHA"):
            raise Blocked("OWNER snapshot checkout did not match the exact source SHA")
    elif env.get("GITHUB_EVENT_NAME") == "workflow_dispatch":
        if not (env.get("GITHUB_ACTOR") == env.get("GITHUB_TRIGGERING_ACTOR") == "adrianf87" and
                env.get("GITHUB_REF") in ("refs/heads/" + BRANCH, "refs/heads/main") and
                env.get("OWNER_CONFIRMATION") == "RUN_STAGE3_PUBLIC_EXTENDED_STABILITY" and
                env.get("PRODUCTION_RESTARTS_ACK") == "false" and
                env.get("PREMERGE_PHASE") == "none" and
                checkout_sha() == env.get("PREMERGE_CANDIDATE_SHA") == env.get("GITHUB_SHA")):
            raise Blocked("OWNER, ref, confirmation token or exact source SHA guard failed")
    elif group == "extended" or env.get("GITHUB_EVENT_NAME") != "pull_request" or env.get("GITHUB_HEAD_REF") != BRANCH:
        raise Blocked("Public qualification requires the designated candidate branch PR")


def run_group(args):
    invalidate_credentials()
    suite = Suite(args.group, args.output, args.sha, args.image)
    with suite, tempfile.TemporaryDirectory(prefix="stage3-private-") as private:
        work = Path(private)
        if args.group in PUBLIC_GROUPS or args.group == "extended":
            public_guard(args.group)
            if args.group == "tls-public":
                public_checks.public_tls(suite)
            elif args.group == "exposure":
                public_checks.exposure(suite)
            elif args.group == "ncdc":
                check_real_ncdc(suite, public_context(), {})
            elif args.group == "two-client-public":
                check_two_client_protocol(suite, public_context(), {})
            else:
                public_checks.extended(suite)
        elif args.group == "offline":
            offline(suite, args.build.resolve(), work, args.parallel, local=args.image == "local")
        elif args.group == "sandbox":
            sandbox(suite, args.sudo_result)
        else:
            binary = str(args.binary.resolve()) if args.image == "local" else verified_binary(suite, args.binary)
            function = {"two-client": fixtures.two_clients, "tls-fixtures": fixtures.tls_fixtures,
                        "stability": fixtures.stability}[args.group]
            function(suite, binary, work)
    print((suite.output / "summary.log").read_text(), end="")
    return 0 if suite.passed else 1


def artifact_name(group, image, attempt):
    return f"{ARTIFACT_PREFIX}-{group}-{image}-{attempt}"


def report(args):
    if checkout_sha() != args.sha:
        raise ValueError("Report generator was checked out at the wrong source SHA")
    modes = ([(group, image) for group in RUNNER_GROUPS for image in OS_MATRIX] +
             [(group, "ubuntu-24.04") for group in PUBLIC_GROUPS]) if args.mode == "pr" else [
                 ("extended", "ubuntu-24.04")]
    rows = []
    issues = []
    binary_identity = {}
    for group, image in modes:
        artifact = artifact_name(group, image, args.attempt)
        try:
            verified = verify_bundle(args.input / artifact, group, image, args.sha, args.run_id, args.attempt)
        except (OSError, ValueError, TypeError, KeyError, ET.ParseError) as error:
            issues.append(f"{artifact}: evidence unavailable or invalid ({type(error).__name__})")
            time_reported = now()
            rows.extend({
                "id": case, "category": group, "runner_image": image, "runner_os": "Linux",
                "workflow": WORKFLOWS[args.mode], "run_id": args.run_id, "run_attempt": args.attempt,
                "source_sha": args.sha, "started_at": time_reported, "ended_at": time_reported,
                "tools": {"status": "unavailable"}, "os_release": "unavailable", "expected": description,
                "result": "BLOCKED", "observed": "Evidence unavailable or invalid",
                "reason": "Missing or invalid case bundle", "artifact": "qualification.json",
            } for case, description in CASES[group].items())
        else:
            rows.extend(verified)
            if any(row["workflow"] != WORKFLOWS[args.mode] for row in verified):
                issues.append(f"{artifact}: workflow identity mismatch")
            if group == "offline":
                identity = next(row for row in verified if row["id"] == "binary.identity")
                binary_identity[image] = re.search(r"SHA-256 ([a-f0-9]{64})", identity["observed"])
            if group in {"two-client", "stability", "tls-fixtures"} and all(row["result"] == "PASS" for row in verified):
                expected = binary_identity.get(image)
                actual = verified[0]["tools"].get("hub", "")
                if not expected or not actual.endswith("SHA-256 " + expected[1]):
                    issues.append(f"{artifact}: exercised binary does not match offline build evidence")
    unresolved = [row for row in rows if row["result"] != "PASS"]
    output = args.output
    output.mkdir(parents=True, exist_ok=True)
    url = f"https://github.com/{REPOSITORY}/actions/runs/{args.run_id}/attempts/{args.attempt}"
    title = [
        "# Stage III qualification evidence",
        "",
        "**STAGE III — NOT READY FOR FINAL ACCEPTANCE**",
        "",
        f"Candidate source commit `{args.sha}`. [GitHub Actions run]({url}).",
        "",
        f"Automated bundle gate **{'FAIL' if unresolved or issues else 'PASS'}** for this workflow's scope.",
        "",
        "## Automated test matrix",
        "",
        "| Runner | Category | Check | Result | Observed |",
        "| --- | --- | --- | --- | --- |",
    ]
    for row in rows:
        observed = row["observed"].replace("|", "/").replace("\n", " ")
        title.append(f'| {row["runner_image"]} | {row["category"]} | {row["id"]} | {row["result"]} | {observed} |')
    title += ["", "## Unresolved automated checks", ""]
    title.extend("- " + item for item in issues)
    title.extend(f'- {row["runner_image"]} / {row["category"]} / {row["id"]} — {row["result"]}'
                 for row in unresolved)
    if args.mode == "pr":
        title.append("- Extended OWNER workflow evidence is recorded in a separate run.")
    if not (unresolved or issues) and args.mode == "extended":
        title.append("All extended automated results passed.")
    title += ["", "## Manual OWNER evidence", "", "| Check | Result |", "| --- | --- |"]
    title.extend(f"| {description} | NOT TESTED |" for description in MANUAL.values())
    (output / "qualification.md").write_text("\n".join(title) + "\n")
    (output / "qualification.json").write_text(json.dumps({
        "source_sha": args.sha, "mode": args.mode, "run_id": args.run_id, "run_attempt": args.attempt,
        "statuses": rows, "invalid_bundles": issues, "manual": {case: "NOT TESTED" for case in MANUAL},
        "result": "FAIL" if unresolved or issues else "PASS",
    }, indent=2) + "\n")
    write_manifest(output, {"qualification.md", "qualification.json"})
    return 1 if unresolved or issues else 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(required=True)
    job = commands.add_parser("run")
    job.add_argument("group", choices=tuple(CASES))
    job.add_argument("--sha", required=True)
    job.add_argument("--image", choices=(*OS_MATRIX, "local"), required=True)
    job.add_argument("--output", type=Path, required=True)
    job.add_argument("--build", type=Path, default=Path("build/stage3"))
    job.add_argument("--parallel", type=int, default=2)
    job.add_argument("--binary", type=Path, default=Path("build/stage3/dc24h-twilight"))
    job.add_argument("--sudo-result", default="unavailable")
    job.set_defaults(action=run_group)
    gate = commands.add_parser("report")
    gate.add_argument("--mode", choices=tuple(WORKFLOWS), required=True)
    gate.add_argument("--sha", required=True)
    gate.add_argument("--run-id", required=True)
    gate.add_argument("--attempt", required=True)
    gate.add_argument("--input", type=Path, required=True)
    gate.add_argument("--output", type=Path, required=True)
    gate.set_defaults(action=report)
    args = parser.parse_args()
    if not re.fullmatch(r"[0-9a-f]{40}", args.sha):
        parser.error("--sha must be a complete commit SHA")
    return args.action(args)


if __name__ == "__main__":
    raise SystemExit(main())
