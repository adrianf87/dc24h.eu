#!/usr/bin/env python3
"""Optional offline HMAC authenticity and anti-reuse gate for matched recovery.

The operator's independently pinned 256-bit key and replay ledger MUST live
outside the SQL/five-store bundle. An HMAC is a symmetric-key trust decision,
not a signature attributable to a separate individual or proof of quiescence.
Neither the key, private SQL nor five-store contents are logged.
"""

import argparse
import fcntl
import hashlib
import hmac
import json
import os
from pathlib import Path
import re
import stat
import sys

import stage1_recovery_bundle as pair
import stage4_private_recovery_drill as private


NAME = "recovery-auth.json"
DOMAIN = b"dc24h-beta08-offline-pair-v1\x00"
MAX_LEDGER = 1024 * 1024


def _key(path):
    secret = private.read_private(private.private_directory(Path(path).parent),
                                  Path(path).name, 64)
    if len(secret) != 32:
        secret[:] = b"\x00" * len(secret)
        raise ValueError("Offline recovery key must be 256 bits")
    return secret


def key_id(secret):
    return hashlib.sha256(secret).hexdigest()[:32]


def _pair_bytes(pair_directory):
    directory = private.private_directory(pair_directory)
    return private.read_private(directory, pair.NAME, 4096)


def sign(pair_directory, key_path, destination):
    """Create a *detached* tag without mutating the paired SQL/private sources."""
    private.private_directory(destination)
    private.separate_destination(pair_directory, destination)
    source = _pair_bytes(pair_directory)
    secret = _key(key_path)
    try:
        auth = {"schema": 1, "algorithm": "HMAC-SHA256",
                "key_id": key_id(secret),
                "pair_sha256": hashlib.sha256(source).hexdigest(),
                "mac": hmac.new(secret, DOMAIN + source, hashlib.sha256).hexdigest()}
        private.write_private(Path(destination), NAME,
                              (json.dumps(auth, sort_keys=True) + "\n").encode())
        private.sync_directory(Path(destination))
        return auth["key_id"]
    finally:
        secret[:] = b"\x00" * len(secret)
        source[:] = b"\x00" * len(source)


def initialize_ledger(path):
    """First-installation only; never reset a consumed recovery ledger."""
    private.private_directory(Path(path).parent)
    private.write_private(Path(path).parent, Path(path).name, b"")
    private.sync_directory(Path(path).parent)


def _check_and_optionally_consume(ledger_path, fingerprint, consume):
    with private.private_directory_fd(Path(ledger_path).parent) as directory_fd:
        fd = os.open(Path(ledger_path).name,
                     os.O_RDWR | os.O_APPEND | os.O_NOFOLLOW | os.O_NONBLOCK,
                     dir_fd=directory_fd)
        try:
            fcntl.flock(fd, fcntl.LOCK_EX)
            s = os.fstat(fd)
            if (not stat.S_ISREG(s.st_mode) or s.st_uid != os.geteuid() or
                    s.st_nlink != 1 or s.st_mode & 0o077 or s.st_size > MAX_LEDGER):
                raise ValueError("Unsafe authenticity anti-reuse ledger")
            os.lseek(fd, 0, os.SEEK_SET)
            raw = os.read(fd, MAX_LEDGER + 1)
            if len(raw) != s.st_size or not re.fullmatch(rb"(?:[0-9a-f]{64}\n)*", raw):
                raise ValueError("Invalid authenticity anti-reuse ledger")
            entries = raw.splitlines()
            if len(set(entries)) != len(entries):
                raise ValueError("Duplicate authenticity anti-reuse records")
            if fingerprint.encode("ascii") in entries:
                raise ValueError("Recovery pair already accepted; replay refused")
            current = os.stat(Path(ledger_path).name, dir_fd=directory_fd, follow_symlinks=False)
            if (current.st_dev, current.st_ino) != (s.st_dev, s.st_ino):
                raise ValueError("Anti-reuse ledger replaced during verification")
            if consume:
                line = (fingerprint + "\n").encode("ascii")
                if s.st_size + len(line) > MAX_LEDGER or os.write(fd, line) != len(line):
                    raise ValueError("Authenticity anti-reuse ledger exhausted")
                os.fsync(fd)
                current = os.stat(Path(ledger_path).name, dir_fd=directory_fd,
                                  follow_symlinks=False)
                if (current.st_dev, current.st_ino) != (s.st_dev, s.st_ino):
                    raise ValueError("Anti-reuse ledger replaced during append")
            return consume
        finally:
            os.close(fd)


def verify(dump, snapshot, pair_directory, expected_sha, auth_dir, key_path,
           expected_key_id, ledger_path, *, consume=False):
    if not re.fullmatch(r"[0-9a-f]{32}", expected_key_id):
        raise ValueError("Independent offline trust anchor ID required")
    evidence = pair.verified_pair(dump, snapshot, Path(pair_directory) / pair.NAME, expected_sha)
    auth = json.loads(private.read_private(private.private_directory(auth_dir), NAME, 4096))
    if (not isinstance(auth, dict) or set(auth) != {"schema", "algorithm", "key_id", "pair_sha256", "mac"} or
            type(auth["schema"]) is not int or auth["schema"] != 1 or
            auth["algorithm"] != "HMAC-SHA256" or
            any(not isinstance(auth[key], str) or not pair.HASH.fullmatch(auth[key])
                for key in ("pair_sha256", "mac")) or
            not isinstance(auth["key_id"], str) or not re.fullmatch(r"[0-9a-f]{32}", auth["key_id"])):
        raise ValueError("Invalid authenticated recovery format")
    if auth["pair_sha256"] != evidence["pair_hash"]:
        raise ValueError("Authenticated pair digest mismatch")
    secret = _key(key_path)
    source = _pair_bytes(pair_directory)
    try:
        fingerprint = key_id(secret)
        if (fingerprint != expected_key_id or auth["key_id"] != expected_key_id or
                not hmac.compare_digest(hmac.new(secret, DOMAIN + source, hashlib.sha256).hexdigest(), auth["mac"])):
            raise ValueError("Unknown-key or invalid authenticated recovery pair")
    finally:
        secret[:] = b"\x00" * len(secret)
        source[:] = b"\x00" * len(source)
    _check_and_optionally_consume(ledger_path, evidence["pair_hash"], consume)
    return {"schema": 1, "source_sha": evidence["commit_sha"],
            "verified_components": evidence["verified_files"],
            "authenticated": True, "key_id": expected_key_id,
            "anti_reuse_recorded": consume, "restore_performed": False}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("sign", "init-ledger", "verify", "accept"))
    parser.add_argument("--mariadb-dump", type=Path)
    parser.add_argument("--private-snapshot", type=Path)
    parser.add_argument("--pair-directory", type=Path)
    parser.add_argument("--auth-directory", type=Path)
    parser.add_argument("--trusted-key", type=Path)
    parser.add_argument("--expected-key-id")
    parser.add_argument("--commit-sha")
    parser.add_argument("--ledger", type=Path)
    parser.add_argument("--ack-isolated", action="store_true")
    parser.add_argument("--ack-stopped", action="store_true")
    args = parser.parse_args(argv)
    if not args.ack_isolated or (args.mode in ("sign", "accept", "init-ledger") and not args.ack_stopped):
        parser.error("Explicit offline disposable/stopped-writer acknowledgment required")
    try:
        if args.mode == "init-ledger":
            if not args.ledger: raise ValueError("Ledger required")
            initialize_ledger(args.ledger)
            result = {"schema": 1, "anti_reuse_ledger_initialized": True}
        elif args.mode == "sign":
            if not all((args.pair_directory, args.auth_directory, args.trusted_key)):
                raise ValueError("Detached authentication inputs required")
            result = {"schema": 1, "key_id": sign(args.pair_directory, args.trusted_key, args.auth_directory)}
        else:
            if not all((args.mariadb_dump, args.private_snapshot, args.pair_directory,
                        args.auth_directory, args.trusted_key, args.expected_key_id,
                        args.commit_sha, args.ledger)):
                raise ValueError("Full authenticated recovery inputs required")
            result = verify(args.mariadb_dump, args.private_snapshot, args.pair_directory,
                            args.commit_sha, args.auth_directory, args.trusted_key,
                            args.expected_key_id, args.ledger, consume=args.mode == "accept")
    except (OSError, ValueError, TypeError, KeyError, UnicodeError, json.JSONDecodeError):
        print("Authenticated offline recovery refused; inspect owner-only sources privately.", file=sys.stderr)
        return 1
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
