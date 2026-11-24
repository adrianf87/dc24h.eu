#!/usr/bin/env python3
"""Bind a disposable MariaDB dump and the five private gateway stores.

Does not dump, import, stop or start a database/service; no secret contents or
absolute paths enter its manifest/stdout. Only use in an approved offline pilot.
"""

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import stat

import stage4_private_recovery_drill as private


NAME = "recovery-pair.json"
MAX_DUMP = 20 * 1024 * 1024 * 1024
SHA = re.compile(r"[0-9a-f]{40}\Z")
HASH = re.compile(r"[0-9a-f]{64}\Z")


def private_hash(path, maximum):
    file = Path(path)
    private.private_directory(file.parent)
    fd = os.open(file, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
    try:
        before = os.fstat(fd)
        if (not stat.S_ISREG(before.st_mode) or before.st_uid != os.geteuid() or
            before.st_nlink != 1 or before.st_mode & 0o077 or
            not 0 < before.st_size <= maximum):
            raise ValueError("Unsafe recovery input file")
        digest = hashlib.sha256()
        total = 0
        while True:
            data = os.read(fd, 1024 * 1024)
            if not data:
                break
            total += len(data)
            if total > maximum:
                raise ValueError("Recovery input grew beyond limit")
            digest.update(data)
        after = os.fstat(fd)
        if (total != before.st_size or after.st_size != before.st_size or
            after.st_mtime_ns != before.st_mtime_ns or after.st_ctime_ns != before.st_ctime_ns):
            raise ValueError("Recovery input changed during hashing")
        return {"size": total, "sha256": digest.hexdigest()}
    finally:
        os.close(fd)


def verified_pair(dump, snapshot, pair_file, expected_commit_sha=None):
    pair_path = Path(pair_file)
    raw = private.read_private(private.private_directory(pair_path.parent), pair_path.name, 4096)
    data = json.loads(raw)
    if (set(data) != {"schema", "created_utc", "commit_sha", "mariadb_dump", "private_manifest"} or
        type(data["schema"]) is not int or data["schema"] != 1 or
        not isinstance(data["commit_sha"], str) or not SHA.fullmatch(data["commit_sha"]) or
        not private.canonical_utc_timestamp(data["created_utc"])):
        raise ValueError("Malformed paired recovery manifest")
    if expected_commit_sha is not None and data["commit_sha"] != expected_commit_sha:
        raise ValueError("Recovery pair belongs to a different source commit")
    for key in ("mariadb_dump", "private_manifest"):
        record = data[key]
        if (not isinstance(record, dict) or set(record) != {"size", "sha256"} or
            type(record["size"]) is not int or record["size"] < 1 or
            not isinstance(record["sha256"], str) or not HASH.fullmatch(record["sha256"])):
            raise ValueError("Invalid paired hash metadata")
    private.verify(str(snapshot))
    if private_hash(dump, MAX_DUMP) != data["mariadb_dump"] or (
        private_hash(Path(snapshot) / private.MANIFEST, 4096) != data["private_manifest"]
    ):
        raise ValueError("MariaDB/private-state recovery pair mismatch")
    return {"schema": 1, "verified_files": len(private.FILES) + 1,
            "commit_sha": data["commit_sha"], "pair_hash": hashlib.sha256(raw).hexdigest(),
            "mariadb_restored": False, "rpo_rto_measured": False}


def create_pair(dump, snapshot, destination, commit_sha):
    if not SHA.fullmatch(commit_sha):
        raise ValueError("Exact 40-character git commit SHA required")
    private.verify(str(snapshot))
    private.private_directory(destination)
    # A pair manifest is an extra file, not a member of the private snapshot.
    # Writing it into the snapshot would invalidate an otherwise recoverable
    # five-store backup, even though the post-write verification rejects it.
    private.separate_destination(snapshot, destination)
    # Pair only immutable offline inputs: the stored hashes are reverified
    # after the manifest has been written, including every private file.
    data = {"schema": 1,
            "created_utc": datetime.now(timezone.utc).isoformat(timespec="seconds"),
            "commit_sha": commit_sha,
            "mariadb_dump": private_hash(dump, MAX_DUMP),
            "private_manifest": private_hash(Path(snapshot) / private.MANIFEST, 4096)}
    private.write_private(Path(destination), NAME,
                          json.dumps(data, sort_keys=True, separators=(",", ":")).encode() + b"\n")
    private.sync_directory(Path(destination))
    return verified_pair(dump, snapshot, Path(destination) / NAME)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("pair", "verify"))
    parser.add_argument("--mariadb-dump", type=Path, required=True)
    parser.add_argument("--private-snapshot", type=Path, required=True)
    parser.add_argument("--pair-directory", type=Path, required=True)
    parser.add_argument("--commit-sha", help="Required on pair; exact source commit")
    parser.add_argument("--ack-stopped", action="store_true")
    parser.add_argument("--ack-isolated", action="store_true")
    args = parser.parse_args(argv)
    if not args.ack_isolated or (args.mode == "pair" and not args.ack_stopped):
        parser.error("Explicit isolated-pilot and stopped-service acknowledgment required")
    if args.mode == "pair":
        if not args.commit_sha:
            parser.error("--commit-sha required when pairing")
        result = create_pair(args.mariadb_dump, args.private_snapshot,
                             args.pair_directory, args.commit_sha)
    else:
        result = verified_pair(args.mariadb_dump, args.private_snapshot,
                               args.pair_directory / NAME, args.commit_sha)
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
