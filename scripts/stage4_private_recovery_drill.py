#!/usr/bin/env python3
"""Offline, non-overwriting backup and restore rehearsal for private WebAdmin state.

Only use on an isolated host after stopping the gateway. MariaDB backup/restore
is a separate operator-approved step. This tool never touches a live service.
"""

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import stat
import sys
import time


FILES = {
    "mfa.json": 16384,
    "replay.jsonl": 1024 * 1024,
    "audit.jsonl": 8 * 1024 * 1024,
    "passkeys.json": 1024 * 1024,
    "rate-limits.json": 65536,
}
MANIFEST = "manifest.json"
ZERO = "0" * 64
HEX = re.compile(r"[0-9a-f]{64}\Z")


def private_directory(path):
    candidate = Path(path)
    if not candidate.is_absolute():
        raise ValueError("Absolute private directory required")
    s = candidate.lstat()
    if not stat.S_ISDIR(s.st_mode) or s.st_uid != os.geteuid() or s.st_mode & 0o077:
        raise ValueError("Unsafe private directory")
    return candidate


def read_private(root, name, maximum, allow_empty=False):
    fd = os.open(root / name, os.O_RDONLY | os.O_NOFOLLOW)
    try:
        s = os.fstat(fd)
        if not stat.S_ISREG(s.st_mode) or s.st_uid != os.geteuid() or s.st_nlink != 1 or (
            s.st_mode & 0o077) or s.st_size > maximum or (s.st_size == 0 and not allow_empty):
            raise ValueError("Unsafe private source file")
        data = bytearray()
        while len(data) < s.st_size:
            chunk = os.read(fd, min(65536, s.st_size - len(data)))
            if not chunk:
                raise ValueError("Truncated private source")
            data.extend(chunk)
        if os.fstat(fd).st_size != s.st_size:
            raise ValueError("Private source changed while reading")
        return data
    finally:
        os.close(fd)


def write_private(root, name, data):
    fd = os.open(root / name, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600)
    try:
        at = 0
        while at < len(data):
            written = os.write(fd, memoryview(data)[at:])
            if not written:
                raise OSError("Incomplete private archive write")
            at += written
        os.fsync(fd)
    finally:
        os.close(fd)


def sync_directory(path):
    fd = os.open(path, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    try:
        os.fsync(fd)
    finally:
        os.close(fd)


def make_private_destination(path):
    target = Path(path)
    if not target.is_absolute():
        raise ValueError("Absolute destination required")
    private_directory(target.parent)
    target.mkdir(mode=0o700, exist_ok=False)
    sync_directory(target.parent)
    return private_directory(target)


def validate_audit(data):
    if data and not data.endswith(b"\n"):
        raise ValueError("Truncated audit chain")
    previous, sequence = ZERO, 0
    for line in data[:-1].split(b"\n") if data else []:
        row = json.loads(line)
        sequence += 1
        if not isinstance(row, dict) or set(row) != {"schema", "sequence", "previous", "event", "at", "digest"} or (
            row["schema"] != 1 or row["sequence"] != sequence or row["previous"] != previous or
            not isinstance(row["event"], str) or not re.fullmatch(r"[a-z_]+", row["event"]) or
            not isinstance(row["at"], str) or not isinstance(row["digest"], str) or not HEX.fullmatch(row["digest"])):
            raise ValueError("Invalid audit sequence")
        at = row["at"]
        if not re.fullmatch(r"\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d\.\d{3}Z", at):
            raise ValueError("Invalid audit timestamp")
        datetime.fromisoformat(at.replace("Z", "+00:00"))
        serialized = json.dumps([1, sequence, previous, row["event"], at],
                                separators=(",", ":"), ensure_ascii=False).encode("utf8")
        previous = hashlib.sha256(serialized).hexdigest()
        if row["digest"] != previous:
            raise ValueError("Audit digest mismatch")
    return sequence


def validate_format(name, data):
    if name == "audit.jsonl":
        validate_audit(data)
    elif name == "replay.jsonl":
        if data and not data.endswith(b"\n"):
            raise ValueError("Truncated replay ledger")
        for line in data[:-1].split(b"\n") if data else []:
            row = json.loads(line)
            if not isinstance(row, dict) or set(row) not in ({"actor", "step"}, {"actor", "step", "nonce"}) or (
                not isinstance(row["actor"], str) or not re.fullmatch(r"[1-9]\d{0,18}", row["actor"]) or
                type(row["step"]) is not int or row["step"] < 0):
                raise ValueError("Invalid replay ledger")
    else:
        obj = json.loads(data)
        if not isinstance(obj, dict) or obj.get("schema") != 1:
            raise ValueError("Invalid private state format")
        field = {"mfa.json": "accounts", "passkeys.json": "credentials", "rate-limits.json": "buckets"}[name]
        if field not in obj or not isinstance(obj[field], dict if name == "mfa.json" else list):
            raise ValueError("Invalid private state payload")
        if name == "mfa.json" and (not 1 <= len(obj["accounts"]) <= 64 or
            any(not re.fullmatch(r"[1-9]\d{0,18}", actor) or not isinstance(seed, str) or
                not re.fullmatch(r"[A-Z2-7]{32}(?:[A-Z2-7]{8}){0,4}", seed)
                for actor, seed in obj["accounts"].items())):
            raise ValueError("Invalid private MFA catalog")
        if name == "passkeys.json" and (not isinstance(obj.get("users"), list) or
            not isinstance(obj.get("rpID"), str) or not obj["rpID"] or
            len(obj["users"]) > 64 or len(obj["credentials"]) > 256):
            raise ValueError("Invalid private passkey catalog")
        if name == "rate-limits.json" and (len(obj["buckets"]) > 256 or
            any(not isinstance(bucket, dict) or not isinstance(bucket.get("key"), str) or
                not HEX.fullmatch(bucket["key"]) or type(bucket.get("count")) is not int or
                bucket["count"] < 1 or bucket["count"] > 1_000_000 or
                type(bucket.get("starts")) is not int or bucket["starts"] < 0
                for bucket in obj["buckets"])):
            raise ValueError("Invalid private rate-limit catalog")


def snapshot(source, destination):
    original = private_directory(source)
    # Preflight all source paths before creating a destination. A missing
    # ledger must fail closed rather than silently create an empty replacement.
    for name, maximum in FILES.items():
        data = read_private(original, name, maximum, name in ("audit.jsonl", "replay.jsonl"))
        try:
            validate_format(name, data)
        finally:
            data[:] = b"\0" * len(data)
    target = make_private_destination(destination)
    entries = {}
    for name, maximum in FILES.items():
        data = read_private(original, name, maximum, name in ("audit.jsonl", "replay.jsonl"))
        try:
            validate_format(name, data)
            entries[name] = {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}
            write_private(target, name, data)
        finally:
            data[:] = b"\0" * len(data)
    manifest = {"schema": 1, "files": entries,
                "created_utc": datetime.now(timezone.utc).isoformat(timespec="seconds")}
    write_private(target, MANIFEST, json.dumps(manifest, sort_keys=True).encode("utf8") + b"\n")
    sync_directory(target)
    verify(target)
    return len(entries)


def verify(source):
    root = private_directory(source)
    if {child.name for child in root.iterdir()} != set(FILES) | {MANIFEST}:
        raise ValueError("Incomplete or unexpected backup contents")
    raw = read_private(root, MANIFEST, 4096)
    try:
        manifest = json.loads(raw)
    finally:
        raw[:] = b"\0" * len(raw)
    if not isinstance(manifest, dict) or set(manifest) != {"schema", "created_utc", "files"} or (
        manifest["schema"] != 1 or not isinstance(manifest["files"], dict) or
        set(manifest["files"]) != set(FILES)):
        raise ValueError("Invalid private backup manifest")
    for name, maximum in FILES.items():
        data = read_private(root, name, maximum, name in ("audit.jsonl", "replay.jsonl"))
        try:
            meta = manifest["files"][name]
            if not isinstance(meta, dict) or set(meta) != {"bytes", "sha256"} or (
                meta["bytes"] != len(data) or not isinstance(meta["sha256"], str) or
                not HEX.fullmatch(meta["sha256"]) or hashlib.sha256(data).hexdigest() != meta["sha256"]):
                raise ValueError("Corrupt private backup")
            validate_format(name, data)
        finally:
            data[:] = b"\0" * len(data)
    return len(FILES)


def restore_to_fresh_directory(source, destination):
    verify(source)
    backup = private_directory(source)
    target = make_private_destination(destination)
    for name, maximum in FILES.items():
        data = read_private(backup, name, maximum, name in ("audit.jsonl", "replay.jsonl"))
        try:
            write_private(target, name, data)
        finally:
            data[:] = b"\0" * len(data)
    sync_directory(target)
    # An independent verification is still needed before manual installation;
    # this command never overwrites the real gateway's data directory.
    return len(FILES)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("snapshot", "verify", "restore"))
    parser.add_argument("--source", required=True)
    parser.add_argument("--destination")
    parser.add_argument("--ack-stopped", action="store_true")
    parser.add_argument("--ack-isolated", action="store_true")
    args = parser.parse_args(argv)
    if not args.ack_isolated or (args.action != "verify" and not args.ack_stopped):
        parser.error("Explicit isolated pilot and stopped gateway acknowledgment required")
    if (args.action != "verify") != bool(args.destination):
        parser.error("Destination required only for snapshot/restore")
    started = time.monotonic()
    try:
        if args.action == "snapshot":
            count = snapshot(args.source, args.destination)
        elif args.action == "restore":
            count = restore_to_fresh_directory(args.source, args.destination)
        else:
            count = verify(args.source)
    except (OSError, ValueError, TypeError, KeyError, UnicodeError, json.JSONDecodeError):
        print("Private Stage IV recovery drill refused; inspect isolated state offline.", file=sys.stderr)
        return 1
    print(json.dumps({"schema": 1, "action": args.action, "verified_files": count,
                      "elapsed_seconds": round(time.monotonic() - started, 3),
                      "target_recovery_completed": False}, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
