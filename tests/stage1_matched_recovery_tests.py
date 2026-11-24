"""Disposable, matched MariaDB + private gateway-state recovery on hosted Ubuntu.

All three MariaDB instances use fresh temporary datadirs and Unix sockets with
networking disabled. No host database, system service or real gateway is touched.
Only aggregate, non-sensitive evidence may leave this test.
"""

import hashlib
import json
import os
from pathlib import Path
import platform
import pwd
import re
import subprocess
import sys
import tempfile
import time
import unittest


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
import stage1_recovery_bundle as bundle
import stage4_private_recovery_drill as private

SOURCE_SHA = os.environ.get("SOURCE_SHA", "")
EXPECTED_ROW = "00FF61\towner\tactive\t2"
MAX_RECOVERY_SECONDS = 120.0  # Disposable Ubuntu CI smoke budget, not production SLA.


def command(args, *, stdin=None, stdout=subprocess.PIPE, timeout=45):
    result = subprocess.run(args, input=stdin, stdout=stdout, stderr=subprocess.DEVNULL,
                            timeout=timeout, check=False)
    if result.returncode:
        raise AssertionError("Isolated recovery command failed (details suppressed)")
    return result.stdout


class IsolatedDatabase:
    def __init__(self, root):
        self.root = root
        self.root.mkdir(mode=0o700)
        self.datadir = root / "data"
        self.tmp = root / "tmp"
        self.tmp.mkdir(mode=0o700)
        self.socket = root / "db.sock"
        self.proc = None
        self.user = pwd.getpwuid(os.geteuid()).pw_name

    def client(self, *more):
        return ["mariadb", "--no-defaults", "--skip-ssl", "--protocol=socket",
                f"--socket={self.socket}", "--user=root", *more]

    def initialize(self):
        command(["mariadb-install-db", "--no-defaults", f"--datadir={self.datadir}",
                 f"--tmpdir={self.tmp}", "--auth-root-authentication-method=normal",
                 "--skip-test-db", f"--user={self.user}"], timeout=60)

    def start(self):
        self.proc = subprocess.Popen([
            "mariadbd", "--no-defaults", f"--user={self.user}",
            f"--datadir={self.datadir}", f"--socket={self.socket}",
            f"--tmpdir={self.tmp}", f"--pid-file={self.root / 'db.pid'}",
            f"--log-error={self.root / 'db.log'}", "--skip-networking",
            "--skip-name-resolve", "--innodb-buffer-pool-size=32M",
            "--innodb-log-file-size=8M",
        ], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        deadline = time.monotonic() + 25
        while time.monotonic() < deadline:
            if self.proc.poll() is not None:
                raise AssertionError("Isolated MariaDB exited during startup")
            if self.socket.exists():
                ping = subprocess.run([
                    "mariadb-admin", "--no-defaults", "--protocol=socket",
                    f"--socket={self.socket}", "--user=root", "ping",
                ], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=4)
                if ping.returncode == 0:
                    return
            time.sleep(0.1)
        raise AssertionError("Isolated MariaDB did not become ready")

    def stop(self):
        if self.proc is not None:
            if self.proc.poll() is None:
                self.proc.terminate()
                try:
                    self.proc.wait(timeout=12)
                except subprocess.TimeoutExpired:
                    self.proc.kill()
                    self.proc.wait(timeout=5)
            self.proc = None

    def sql(self, query):
        return command(self.client("--batch", "--skip-column-names"),
                       stdin=query.encode()).decode().strip()

    def setup_schema(self):
        # Each disposable instance has a fresh, test-only credential. Let
        # MariaDB hash it with its configured authentication plugin; never
        # implement the legacy mysql_native_password SHA-1 hash in the test.
        fixture_value = os.urandom(24).hex()
        self.sql("CREATE DATABASE dc24h CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;"
                 f"CREATE USER 'dc24h'@'localhost' IDENTIFIED BY '{fixture_value}';"
                 "GRANT SELECT,INSERT,UPDATE,DELETE,CREATE,ALTER,DROP ON dc24h.* "
                 "TO 'dc24h'@'localhost';")
        password = self.root / "password"
        # Create owner-only from the outset; never expose credentials through
        # an intermediate world-readable file or truncate an existing path.
        fd = os.open(password, os.O_WRONLY | os.O_CREAT | os.O_EXCL |
                     os.O_NOFOLLOW | os.O_CLOEXEC, 0o600)
        with os.fdopen(fd, "w", encoding="ascii") as file:
            file.write(fixture_value + "\n")
        config = self.root / "hub.ini"
        config.write_text("[network]\nlisten=127.0.0.1:0\n[database]\nenabled=on\n"
                          f"socket={self.socket}\nuser=dc24h\npassword_file=password\n"
                          "timeout=2\n", encoding="utf-8")
        return config

    def dump(self, destination):
        # The dump is kept on disk with owner-only permissions; never capture
        # its SQL content in workflow output or an uploaded artifact.
        fd = os.open(destination, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600)
        try:
            with os.fdopen(fd, "wb") as file:
                command(["mariadb-dump", "--no-defaults", "--protocol=socket",
                         f"--socket={self.socket}", "--user=root", "--single-transaction",
                         "--quick", "--skip-lock-tables", "--hex-blob", "dc24h"],
                        stdout=file, timeout=45)
                file.flush()
                os.fsync(file.fileno())
        except BaseException:
            destination.unlink(missing_ok=True)
            raise

    def restore_sql(self, source):
        with source.open("rb") as file:
            result = subprocess.run(self.client("dc24h"), stdin=file,
                                    stdout=subprocess.DEVNULL,
                                    stderr=subprocess.DEVNULL, timeout=45)
        if result.returncode:
            raise AssertionError("Isolated MariaDB SQL import failed")


def make_gateway_fixture(root):
    private.private_directory(root)
    clock = 1763856000000
    actor = "7"
    passkey = {"schema": 1, "rpID": "localhost",
               "users": [{"actor": actor, "handle": "Q" * 32, "epoch": 2}],
               "credentials": [{"actor": actor, "id": "D" * 32,
                                "userHandle": "Q" * 32, "publicKey": "P" * 40,
                                "counter": 4, "deviceType": "singleDevice",
                                "backedUp": False, "transports": ["internal"],
                                "name": "Revoked CI fixture", "createdAt": clock - 1000,
                                "lastUsedAt": clock - 500, "revokedAt": clock}]}
    records = {
        "mfa.json": {"schema": 1, "accounts": {actor: "A" * 32}},
        "passkeys.json": passkey,
        "rate-limits.json": {"schema": 1, "buckets": [{
            "key": "0" * 64, "count": 3, "starts": clock}]},
    }
    for name, value in records.items():
        private.write_private(root, name, (json.dumps(value, sort_keys=True) + "\n").encode())
    private.write_private(root, "replay.jsonl",
                          (json.dumps({"actor": actor, "step": 1234}) + "\n").encode())
    at = "2026-11-23T00:00:00.000Z"
    event = "passkey_revoked"
    digest = hashlib.sha256(json.dumps([1, 1, private.ZERO, event, at],
                                      separators=(",", ":")).encode()).hexdigest()
    private.write_private(root, "audit.jsonl", (json.dumps({
        "schema": 1, "sequence": 1, "previous": private.ZERO,
        "event": event, "at": at, "digest": digest,
    }, separators=(",", ":")) + "\n").encode())


def verify_recovered(db, path, binary):
    actual = db.sql("SELECT HEX(c.verifier),a.role,a.status,a.revision "
                    "FROM dc24h.dc24h_accounts a JOIN dc24h.dc24h_account_credentials c "
                    "ON c.account_id=a.id WHERE a.id=7;")
    if actual != EXPECTED_ROW:
        raise AssertionError("Recovered credential and grant state mismatch")
    version = db.sql("SELECT MAX(version) FROM dc24h.dc24h_schema_migrations;")
    if version != "11":
        raise AssertionError("Recovered MariaDB migration version mismatch")
    config = db.root / "hub.ini"
    result = command([str(binary), "--config", str(config), "--database-status"])
    if b"version=11 latest=11 pending=0 dirty=no" not in result:
        raise AssertionError("C++ account authority does not accept recovered database")
    manifest = private.verified_manifest(path.parent / "snapshot")
    private.check_payloads(path, manifest, include_manifest=False)
    passkeys = json.loads(private.read_private(path, "passkeys.json", private.FILES["passkeys.json"]))
    if (passkeys["users"][0]["epoch"] != 2 or
            passkeys["credentials"][0]["revokedAt"] is None):
        raise AssertionError("Recovered passkey revocation/epoch not retained")
    replay = private.read_private(path, "replay.jsonl", private.FILES["replay.jsonl"])
    if len(replay.splitlines()) != 1:
        raise AssertionError("Recovered replay state mismatch")
    audit = private.read_private(path, "audit.jsonl", private.FILES["audit.jsonl"])
    if private.validate_audit(audit) != 1:
        raise AssertionError("Recovered audit-chain integrity mismatch")


class MatchedRecoveryTests(unittest.TestCase):
    def test_disposable_restore_outage_and_fresh_rollback(self):
        if os.environ.get("GITHUB_ACTIONS") == "true":
            self.assertEqual(os.environ.get("RUNNER_ENVIRONMENT"), "github-hosted")
            self.assertEqual(os.environ.get("RUNNER_OS"), "Linux")
        binary = Path(sys.argv[1]).resolve()
        self.assertTrue(binary.is_file())
        with tempfile.TemporaryDirectory(prefix="dc24h-matched-dr-") as directory:
            root = Path(directory)
            source, recovered, rollback = (IsolatedDatabase(root / name)
                                           for name in ("primary", "recovered", "rollback"))
            try:
                source.initialize()
                source.start()
                config = source.setup_schema()
                command([str(binary), "--config", str(config), "--migrate-database"])
                source.sql("INSERT INTO dc24h.dc24h_accounts "
                           "(id,name,role,status,created_at,updated_at,revision) "
                           "VALUES (7,'ci-recovery','owner','active',1,2,2);"
                           "INSERT INTO dc24h.dc24h_account_credentials "
                           "(account_id,verifier,created_at) VALUES (7,UNHEX('00FF61'),2);")
                gateway = root / "gateway"
                gateway.mkdir(mode=0o700)
                make_gateway_fixture(gateway)
                snapshot = root / "snapshot"
                private.snapshot(gateway, snapshot)
                dump = root / "checkpoint.sql"
                source.dump(dump)
                pair_dir = root / "pair"
                pair_dir.mkdir(mode=0o700)
                sha = SOURCE_SHA if re.fullmatch(r"[0-9a-f]{40}", SOURCE_SHA) else "0" * 40
                bundle.create_pair(dump, snapshot, pair_dir, sha)
                self.assertEqual(bundle.verified_pair(dump, snapshot, pair_dir / bundle.NAME)
                                 ["verified_files"], 6)
                # No new acknowledged writes after the stopped consistency point:
                # measured fixture RPO is zero lost committed writes, NOT a live SLA.
                outage_at = time.monotonic()
                source.stop()

                recovered.initialize()
                recovered.start()
                recovered.setup_schema()
                recovered.restore_sql(dump)
                restored_dir = root / "restored-state"
                private.restore_to_fresh_directory(snapshot, restored_dir)
                verify_recovered(recovered, restored_dir, binary)
                rto = time.monotonic() - outage_at
                self.assertLess(rto, MAX_RECOVERY_SECONDS)

                # Inject a post-recovery privilege mutation and a second outage.
                # The rollback MUST use a new datadir and a fresh state directory;
                # it must not overwrite a running database or any source snapshot.
                recovered.sql("UPDATE dc24h.dc24h_accounts SET role='registered' "
                              "WHERE id=7;")
                self.assertNotEqual(recovered.sql("SELECT role FROM dc24h.dc24h_accounts "
                                                  "WHERE id=7;"), "owner")
                rollback_at = time.monotonic()
                recovered.stop()
                rollback.initialize()
                rollback.start()
                rollback.setup_schema()
                rollback.restore_sql(dump)
                rolled_back_state = root / "rollback-state"
                private.restore_to_fresh_directory(snapshot, rolled_back_state)
                verify_recovered(rollback, rolled_back_state, binary)
                rollback_seconds = time.monotonic() - rollback_at
                self.assertLess(rollback_seconds, MAX_RECOVERY_SECONDS)

                destination = os.environ.get("DC24H_VM_REPORT_DIR")
                if destination:
                    report_dir = Path(destination)
                    self.assertTrue(report_dir.is_dir())
                    report = {"schema": 1, "evidence": "disposable-ubuntu-matched-recovery",
                              "source_sha": sha, "ubuntu": platform.freedesktop_os_release().get("PRETTY_NAME"),
                              "mariadb_schema": 11, "paired_private_files": 5,
                              "credential_bytes_revalidated": True,
                              "grants_and_revocation_state_checked": True,
                              "audit_and_replay_integrity_checked": True,
                              "matched_restore_completed": True, "fresh_rollback_completed": True,
                              "rpo_lost_acknowledged_writes": 0,
                              "rpo_seconds": 0, "rto_seconds": round(rto, 3),
                              "rollback_seconds": round(rollback_seconds, 3),
                              "ci_rto_limit_seconds": MAX_RECOVERY_SECONDS,
                              "gateway_runtime_login_retested": False,
                              "capacity_claim": False, "production_sla_claim": False}
                    with (report_dir / "stage1-matched-recovery.json").open("x") as file:
                        json.dump(report, file, indent=2, sort_keys=True)
                        file.write("\n")
                print("Disposable MariaDB + five-file state restore and fresh rollback passed; "
                      "no production recovery or live gateway-login claim")
            finally:
                for instance in (source, recovered, rollback):
                    instance.stop()


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("Usage: stage1_matched_recovery_tests.py BUILT_DC24H")
    unittest.main(argv=[sys.argv[0]])
