"""Generate and verify Stage III evidence without storing raw test output."""

from contextlib import contextmanager
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import ssl
import subprocess
from types import SimpleNamespace
import xml.etree.ElementTree as ET

STATUSES = {"PASS", "FAIL", "BLOCKED", "NOT TESTED", "NOT APPLICABLE"}
PAYLOADS = {"results.json", "junit.xml", "summary.log"}
OS_MATRIX = ("ubuntu-22.04", "ubuntu-24.04", "ubuntu-26.04")
RUNNER_GROUPS = ("offline", "two-client", "tls-fixtures", "sandbox", "stability")
PUBLIC_GROUPS = ("ncdc", "two-client-public", "tls-public", "exposure")

CASES = {
    "offline": {
        "cmake.configure": "C++ and all required integration dependencies configure",
        "cmake.build": "All build targets compile",
        "binary.identity": "Record exact-source hub binary version and SHA-256",
        "ctest.inventory": "All configured CTest entries are represented, including security and protocol tests",
        "policy.regressions": "Offline endpoint, PTY, workflow and evidence regressions pass",
    },
    "two-client": {
        "guests.visibility": "Two guests see each other with distinct, unprivileged identities",
        "chat.public": "Public chat is delivered in both directions from the correct sender",
        "chat.private": "Private chat is delivered in both directions from the correct sender",
        "search.broadcast": "Broadcast search routes to the other guest",
        "search.direct": "Directed search routes to the specified guest",
        "search.result": "Search result routes to the initiating session with the matching token",
        "transfer.ctm": "ADCS CTM signaling reaches the expected guest",
        "transfer.rcm": "ADC RCM signaling reaches the expected guest",
        "guests.disconnect": "The other guest observes only the expected session quit",
        "guests.reconnect": "The guest can return with its prior identity and a refreshed SID",
        "guests.permissions": "Search and chat are denied to guests when disabled by policy",
    },
    "ncdc": {
        "ncdc.login": "Packaged ncdc negotiates ADCS and appears as a guest with an assigned SID",
        "ncdc.inbound": "Selected ncdc hub tab renders a fresh message sent by its independent observer",
        "ncdc.outbound": "The hub delivers ncdc public chat to an independent guest under ncdc's SID",
        "ncdc.search": "The observer receives ncdc search with ncdc's SID and the synthetic marker",
        "ncdc.reconnect": "Packaged ncdc disconnects and returns with a fresh SID and working chat",
        "ncdc.cleanup": "ncdc exits cleanly; PTY descriptors and isolated profile are released",
    },
    "tls-public": {
        "dns.a": "DNS A records exclusively match the approved IPv4",
        "dns.ptr.forward-confirmed": "Independent PTR matches the approved hostname and resolves back to the approved IPv4",
        "tls.identity": "SNI, trusted chain, exact DNS SAN, validity and Let's Encrypt issuer verify",
        "tls.fingerprint": "Independent TLS reads and OpenSSL agree on the public leaf SHA-256",
        "tls.v12": "TLS 1.2 negotiates successfully",
        "tls.v13": "TLS 1.3 negotiates successfully",
        "tls.v10.denied": "The public listener rejects a TLS 1.0-only ClientHello",
        "tls.v11.denied": "The public listener rejects a TLS 1.1-only ClientHello",
        "tls.plaintext.denied": "Plain ADC on the ADCS listener is rejected",
    },
    "tls-fixtures": {
        "tls.control": "An isolated TLS server presents a trusted certificate with the right SAN",
        "tls.untrusted": "TLS verification rejects an untrusted issuer",
        "tls.san": "TLS verification rejects the wrong DNS SAN",
        "tls.expired": "TLS verification rejects an expired certificate",
        "tls.future": "TLS verification rejects a not-yet-valid certificate",
        "keyp.correct": "Packaged ncdc accepts the independently calculated pin",
        "keyp.incorrect": "Packaged ncdc rejects an incorrect pin",
        "keyp.stale": "After rotation packaged ncdc rejects the previously verified pin",
        "keyp.rotation": "The trusted TLS stack accepts a valid replacement; ncdc accepts the new pin",
        "tls.pair.rollback": "A mismatched pair is refused; known-good backup and replacement both validate",
    },
    "exposure": {
        "tcp.1512": "TCP 1512 accepts verified ADCS/TLS connections",
        **{f"tcp.{port}": f"TCP {port} is unreachable from the independent runner"
           for port in (1511, 3000, 3306, 5173, 5432, 6379, 8000, 8080, 9090)},
    },
    "sandbox": {
        "ubuntu.sudo": "The runner's fixed tools can exercise a disposable systemd fixture",
        "systemd.verify": "systemd-analyze verifies the submitted service example without starting it",
    },
    "stability": {
        "stability.config": "Loopback ADCS-only configuration validates and excludes database and internal API",
        "stability.cycles": "20 guest cycles verify join, disconnect, unique SID and bounded descriptors",
        "stability.refusal": "10 bounded, intentional local TCP refusals are followed by successful logins",
        "stability.interruption": "Abort a local TLS transport during a partial message and reconnect cleanly",
        "stability.idle": "Server idle disconnect is reported and the same guest can log in again",
        "stability.observation": "Concurrent guests stay healthy for the bounded observation window",
        "stability.ncdc": "Exercise the same real ncdc PTY synchronization and cleanup against loopback",
    },
    "extended": {
        "extended.cycles": "20 paced public guest cycles complete with same-identity reconnects and fresh SIDs",
        "extended.observation": "Two public guests remain connected and responsive for 30 minutes",
    },
}

CASES["two-client-public"] = {key: value for key, value in CASES["two-client"].items()
                            if key != "guests.permissions"}

MANUAL = {
    "owner.certificate": "Certificate renewal plan, staging validation and non-disruptive rollback evidence",
    "owner.acceptance": "OWNER reviews completed exact-head CI and issues separate Stage III acceptance and merge decisions",
}


def now():
    return datetime.now(timezone.utc).isoformat(timespec="milliseconds")


def digest(path):
    result = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            result.update(chunk)
    return result.hexdigest()


def command_version(*command):
    try:
        result = subprocess.run(command, capture_output=True, text=True, check=True, timeout=10)
        return (result.stdout or result.stderr).splitlines()[0][:220]
    except (OSError, subprocess.SubprocessError, IndexError):
        return "unavailable"


def checkout_sha():
    return subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()


def invalidate_credentials():
    # Candidate children inherit the tools they need, without typical credential variables.
    for key in tuple(os.environ):
        if any(part in key.upper() for part in ("TOKEN", "PASSWORD", "SECRET", "SSH_AUTH_SOCK")):
            del os.environ[key]


class Blocked(RuntimeError):
    """A required observation could not be made."""


class Mismatch(AssertionError):
    """A mismatch with a controlled, publishable diagnostic."""


class Suite:
    def __init__(self, group, output, source_sha, image, *, verify_checkout=True):
        self.group = group
        self.output = Path(output)
        self.source_sha = source_sha
        self.image = image
        self.records = []
        self.started = now()
        self.tools = {"python": platform.python_version(), "openssl": ssl.OPENSSL_VERSION}
        for name, command in {
            "ncdc": ("ncdc", "--version"), "cmake": ("cmake", "--version"),
            "c++": ("c++", "--version"), "systemd": ("systemd-analyze", "--version"),
        }.items():
            self.tools[name] = command_version(*command)
        self.context = {
            "source_sha": source_sha, "runner_image": image, "runner_os": platform.system(),
            "os_release": platform.freedesktop_os_release().get("PRETTY_NAME", "unknown"),
            "workflow": os.environ.get("GITHUB_WORKFLOW", "local"),
            "run_id": os.environ.get("GITHUB_RUN_ID", "local"),
            "run_attempt": os.environ.get("GITHUB_RUN_ATTEMPT", "local"),
            "tools": self.tools,
        }
        if verify_checkout:
            if checkout_sha() != source_sha:
                raise ValueError("Expected source SHA does not match checked-out HEAD")
            if subprocess.run(["git", "diff", "--quiet", "HEAD", "--"], timeout=10).returncode:
                raise ValueError("Tracked working-tree files differ from the exact source checkout")
        if not re.fullmatch(r"[0-9a-f]{40}", source_sha):
            raise ValueError("Expected exact 40-character source SHA")

    def record(self, identifier, result, observed, *, expected=None, start=None, end=None):
        if any(row["id"] == identifier for row in self.records):
            raise ValueError("Duplicate case " + identifier)
        self.records.append({
            **self.context, "id": identifier, "category": self.group, "started_at": start or now(),
            "ended_at": end or now(), "expected": expected or CASES[self.group][identifier],
            "observed": observed, "result": result, "reason": None if result == "PASS" else observed,
            "artifact": "summary.log#" + identifier,
        })

    @contextmanager
    def case(self, identifier):
        start = now()
        case = SimpleNamespace(observed="All assertions for this case passed")
        try:
            yield case
        except Blocked as error:
            self.record(identifier, "BLOCKED", str(error), start=start)
            raise
        except Exception as error:
            # Arbitrary process output, network text, local paths and secrets stay private.
            observed = str(error) if isinstance(error, Mismatch) else type(error).__name__
            self.record(identifier, "FAIL", observed, start=start)
            raise
        else:
            self.record(identifier, "PASS", case.observed, start=start)

    def __enter__(self):
        return self

    def __exit__(self, error_type, _error, _traceback):
        if error_type and (not self.records or self.records[-1]["result"] == "PASS"):
            self.record("suite.error", "FAIL", error_type.__name__, expected="Suite lifecycle completes")
        seen = {row["id"] for row in self.records}
        for identifier in sorted(CASES[self.group].keys() - seen):
            self.record(identifier, "BLOCKED", "An earlier prerequisite or the suite lifecycle failed")
        self.write()
        # Keep the traceback and exception text out of public Actions logs.
        return error_type is not None and issubclass(error_type, Exception)

    @property
    def passed(self):
        return bool(self.records) and all(row["result"] == "PASS" for row in self.records)

    def write(self):
        self.output.mkdir(parents=True, exist_ok=True)
        data = {"schema": 1, "group": self.group, **self.context, "started_at": self.started,
                "ended_at": now(), "results": self.records}
        (self.output / "results.json").write_text(json.dumps(data, indent=2) + "\n")
        (self.output / "summary.log").write_text("".join(
            f'{row["result"]} {row["id"]} | {row["observed"]}\n' for row in self.records))
        root = ET.Element("testsuite", name=self.group, tests=str(len(self.records)),
                          failures=str(sum(row["result"] == "FAIL" for row in self.records)),
                          skipped=str(sum(row["result"] not in {"PASS", "FAIL"} for row in self.records)))
        for row in self.records:
            test = ET.SubElement(root, "testcase", name=row["id"], classname=self.group)
            if row["result"] == "FAIL":
                ET.SubElement(test, "failure", message=row["observed"])
            elif row["result"] != "PASS":
                ET.SubElement(test, "skipped", message=row["result"] + " | " + row["observed"])
        ET.ElementTree(root).write(self.output / "junit.xml", encoding="utf-8", xml_declaration=True)
        write_manifest(self.output, PAYLOADS)


def write_manifest(directory, files):
    (directory / "SHA256SUMS").write_text("".join(
        f"{digest(directory / name)}  {name}\n" for name in sorted(files)))


def verify_bundle(directory, group, image, sha, run_id, run_attempt=None):
    if {path.name for path in directory.iterdir()} != PAYLOADS | {"SHA256SUMS"}:
        raise ValueError("Missing or unexpected artifact files")
    manifest = "".join(f"{digest(directory / name)}  {name}\n" for name in sorted(PAYLOADS))
    if (directory / "SHA256SUMS").read_text() != manifest:
        raise ValueError("Artifact SHA-256 manifest mismatch")
    data = json.loads((directory / "results.json").read_text())
    identity = {"source_sha": sha, "run_id": run_id, "runner_image": image, "runner_os": "Linux"}
    if run_attempt is not None:
        identity["run_attempt"] = run_attempt
    if data.get("schema") != 1 or data.get("group") != group or any(
            data.get(key) != value for key, value in identity.items()):
        raise ValueError("Artifact source, suite or runner identity mismatch")
    rows = data["results"]
    identifiers = [row["id"] for row in rows]
    verify_rows(rows, group, identity, data)
    verify_junit(directory / "junit.xml", rows)
    expected_log = "".join(f'{row["result"]} {row["id"]} | {row["observed"]}\n' for row in rows)
    if (directory / "summary.log").read_text() != expected_log:
        raise ValueError("Log and JSON results disagree")
    if len(identifiers) != len(set(identifiers)) or not CASES[group].keys() <= set(identifiers):
        raise ValueError("Missing or duplicate qualification results")
    if group == "offline" and all(row["result"] == "PASS" for row in rows):
        inventory = next(row["observed"] for row in rows if row["id"] == "ctest.inventory")
        counts = re.match(r"(\d+) results for (\d+) configured tests;", inventory)
        ctests = {identifier for identifier in identifiers if identifier.startswith("ctest.")} - {"ctest.inventory"}
        if (not counts or int(counts[1]) != int(counts[2]) or len(ctests) < 250 or
                int(counts[1]) != len(ctests) or not {"ctest.adc.ncdc", "ctest.adcs.ncdc", "ctest.systemd.sandbox"} <= ctests):
            raise ValueError("Offline evidence did not include the required CTest inventory")
    return rows


def verify_rows(rows, group, identity, data):
    for row in rows:
        if row["result"] not in STATUSES or row["category"] != group or any(
                row.get(key) != value for key, value in identity.items()):
            raise ValueError("Invalid result status or source identity")
        if not isinstance(row["tools"], dict) or row["tools"] != data["tools"]:
            raise ValueError("Missing or inconsistent tool versions")
        if any(not isinstance(row.get(key), str) or not row[key] for key in
               ("workflow", "run_attempt", "os_release", "expected", "observed", "artifact")):
            raise ValueError("Missing required evidence fields")
        if row["workflow"] != data["workflow"] or row["artifact"] != "summary.log#" + row["id"]:
            raise ValueError("Invalid workflow or artifact reference")
        begin, end = (datetime.fromisoformat(row[key]) for key in ("started_at", "ended_at"))
        if begin.utcoffset() is None or end.utcoffset() is None or begin > end:
            raise ValueError("Invalid result timestamps")
        if (row["reason"] is None) != (row["result"] == "PASS"):
            raise ValueError("Invalid failure reason")


def verify_junit(path, rows):
    expected = {}
    for row in rows:
        status = row["result"]
        expected[row["id"]] = "failure" if status == "FAIL" else "skipped" if status != "PASS" else None
    tests = list(ET.parse(path).getroot())
    actual = {test.attrib["name"]: test[0].tag if len(test) else None for test in tests}
    if len(tests) != len(expected) or actual != expected:
        raise ValueError("JUnit and JSON results disagree")
