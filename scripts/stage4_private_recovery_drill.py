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
ID = re.compile(r"[A-Za-z0-9_-]{20,1400}\Z")
ACTOR = re.compile(r"[1-9][0-9]{0,18}\Z")
MAX_SAFE_INTEGER = (1 << 53) - 1
MAX_ACCOUNT_ID = (1 << 63) - 1
TRANSPORTS = {"ble", "cable", "hybrid", "internal", "nfc", "smart-card", "usb"}
# Must mirror the fixed security event names in webadmin/server/remote-audit.ts.
AUDIT_EVENTS = {
    "login_ok", "login_refused", "mfa_ok", "mfa_refused", "logout",
    "session_revoked", "route_refused", "api_unavailable",
    "passkey_enrolled", "passkey_enroll_refused", "passkey_login_ok", "passkey_login_refused",
    "passkey_device_renamed", "passkey_revoke_intent", "passkey_revoked",
    "passkey_enroll_intent", "passkey_recovery_intent", "passkey_recovered",
    "rate_limiter_unavailable",
}


def safe_time(value):
    return type(value) is int and 0 <= value <= MAX_SAFE_INTEGER


def safe_actor(value, enforce_int64=False):
    return (isinstance(value, str) and ACTOR.fullmatch(value) is not None and
            (not enforce_int64 or int(value) <= MAX_ACCOUNT_ID))


def valid_passkeys(doc):
    if set(doc) != {"schema", "rpID", "users", "credentials"} or not isinstance(doc.get("rpID"), str) or (
        not 1 <= len(doc["rpID"]) <= 253 or
        not re.fullmatch(r"[a-z0-9]+(?:[.-][a-z0-9]+)*", doc["rpID"]) or
        not isinstance(doc.get("users"), list) or not isinstance(doc.get("credentials"), list) or
        len(doc["users"]) > 64 or len(doc["credentials"]) > 256
    ):
        return False
    actors, handles, ids = {}, set(), set()
    for user in doc["users"]:
        if not isinstance(user, dict) or set(user) != {"actor", "handle", "epoch"} or (
            not safe_actor(user["actor"], True) or not isinstance(user["handle"], str) or
            not ID.fullmatch(user["handle"]) or not safe_time(user["epoch"]) or
            user["actor"] in actors or user["handle"] in handles
        ):
            return False
        actors[user["actor"]] = user["handle"]
        handles.add(user["handle"])
    fields = {"actor", "id", "userHandle", "publicKey", "counter", "deviceType", "backedUp",
              "transports", "name", "createdAt", "lastUsedAt", "revokedAt"}
    for item in doc["credentials"]:
        if not isinstance(item, dict) or set(item) != fields or (
            not safe_actor(item["actor"], True) or item["actor"] not in actors or
            not isinstance(item["id"], str) or not ID.fullmatch(item["id"]) or item["id"] in ids or
            item["userHandle"] != actors[item["actor"]] or
            not isinstance(item["publicKey"], str) or not ID.fullmatch(item["publicKey"]) or
            not safe_time(item["counter"]) or item["deviceType"] not in ("singleDevice", "multiDevice") or
            type(item["backedUp"]) is not bool or
            (item["deviceType"] == "singleDevice" and item["backedUp"]) or
            not isinstance(item["transports"], list) or len(item["transports"]) > 7 or
            any(not isinstance(t, str) or t not in TRANSPORTS for t in item["transports"]) or
            not isinstance(item["name"], str) or len(item["name"]) > 64 or
            re.search(r"[\x00-\x1f\x7f<>]", item["name"]) or
            not safe_time(item["createdAt"]) or
            (item["lastUsedAt"] is not None and not safe_time(item["lastUsedAt"])) or
            (item["revokedAt"] is not None and not safe_time(item["revokedAt"]))
        ):
            return False
        ids.add(item["id"])
    return True


def private_directory(path):
    candidate = Path(path)
    if not candidate.is_absolute():
        raise ValueError("Absolute private directory required")
    s = candidate.lstat()
    if not stat.S_ISDIR(s.st_mode) or s.st_uid != os.geteuid() or s.st_mode & 0o077:
        raise ValueError("Unsafe private directory")
    return candidate


def read_private(root, name, maximum, allow_empty=False):
    # A replaced FIFO must fail promptly; a blocking open would hang before
    # fstat could reject its type.
    fd = os.open(root / name, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
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
            type(row["schema"]) is not int or row["schema"] != 1 or
            type(row["sequence"]) is not int or row["sequence"] != sequence or row["previous"] != previous or
            not isinstance(row["event"], str) or row["event"] not in AUDIT_EVENTS or
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
                not safe_actor(row["actor"]) or not safe_time(row["step"]) or
                ("nonce" in row and (not isinstance(row["nonce"], str) or
                    not re.fullmatch(r"[0-9a-f]{32}", row["nonce"])))):
                raise ValueError("Invalid replay ledger")
    else:
        obj = json.loads(data)
        if not isinstance(obj, dict) or type(obj.get("schema")) is not int or obj["schema"] != 1:
            raise ValueError("Invalid private state format")
        field = {"mfa.json": "accounts", "passkeys.json": "credentials", "rate-limits.json": "buckets"}[name]
        if field not in obj or not isinstance(obj[field], dict if name == "mfa.json" else list):
            raise ValueError("Invalid private state payload")
        if name == "mfa.json" and (set(obj) != {"schema", "accounts"} or
            not 1 <= len(obj["accounts"]) <= 64 or
            len({seed for seed in obj["accounts"].values() if isinstance(seed, str)}) != len(obj["accounts"]) or
            any(not safe_actor(actor) or not isinstance(seed, str) or
                not re.fullmatch(r"[A-Z2-7]{32}(?:[A-Z2-7]{8}){0,4}", seed)
                for actor, seed in obj["accounts"].items())):
            raise ValueError("Invalid private MFA catalog")
        if name == "passkeys.json" and not valid_passkeys(obj):
            raise ValueError("Invalid private passkey catalog")
        if name == "rate-limits.json" and (set(obj) != {"schema", "buckets"} or
            len(obj["buckets"]) > 256 or
            len({bucket.get("key") for bucket in obj["buckets"] if isinstance(bucket, dict) and
                isinstance(bucket.get("key"), str)}) != len(obj["buckets"]) or
            any(not isinstance(bucket, dict) or set(bucket) != {"key", "count", "starts"} or
                not isinstance(bucket.get("key"), str) or
                not HEX.fullmatch(bucket["key"]) or type(bucket.get("count")) is not int or
                bucket["count"] < 1 or bucket["count"] > 1_000_000 or
                not safe_time(bucket.get("starts"))
                for bucket in obj["buckets"])):
            raise ValueError("Invalid private rate-limit catalog")


def snapshot(source, destination):
    original = private_directory(source)
    # Preflight all source paths before creating a destination. A missing
    # ledger must fail closed rather than silently create an empty replacement.
    fingerprints = {}
    for name, maximum in FILES.items():
        data = read_private(original, name, maximum, name in ("audit.jsonl", "replay.jsonl"))
        try:
            validate_format(name, data)
            fingerprints[name] = (len(data), hashlib.sha256(data).hexdigest())
        finally:
            data[:] = b"\0" * len(data)
    target = make_private_destination(destination)
    entries = {}
    for name, maximum in FILES.items():
        data = read_private(original, name, maximum, name in ("audit.jsonl", "replay.jsonl"))
        try:
            validate_format(name, data)
            entry = {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}
            if (entry["bytes"], entry["sha256"]) != fingerprints[name]:
                raise ValueError("Private source changed after snapshot preflight")
            entries[name] = entry
            write_private(target, name, data)
        finally:
            data[:] = b"\0" * len(data)
    manifest = {"schema": 1, "files": entries,
                "created_utc": datetime.now(timezone.utc).isoformat(timespec="seconds")}
    write_private(target, MANIFEST, json.dumps(manifest, sort_keys=True).encode("utf8") + b"\n")
    sync_directory(target)
    verify(target)
    return len(entries)


def check_payloads(root, manifest, include_manifest):
    expected = set(FILES) | ({MANIFEST} if include_manifest else set())
    if {child.name for child in root.iterdir()} != expected:
        raise ValueError("Incomplete or unexpected private files")
    for name, maximum in FILES.items():
        data = read_private(root, name, maximum, name in ("audit.jsonl", "replay.jsonl"))
        try:
            meta = manifest["files"][name]
            if not isinstance(meta, dict) or set(meta) != {"bytes", "sha256"} or (
                not safe_time(meta["bytes"]) or meta["bytes"] != len(data) or
                not isinstance(meta["sha256"], str) or not HEX.fullmatch(meta["sha256"]) or
                hashlib.sha256(data).hexdigest() != meta["sha256"]):
                raise ValueError("Corrupt private backup or restored payload")
            validate_format(name, data)
        finally:
            data[:] = b"\0" * len(data)


def verified_manifest(root):
    if {child.name for child in root.iterdir()} != set(FILES) | {MANIFEST}:
        raise ValueError("Incomplete or unexpected backup contents")
    raw = read_private(root, MANIFEST, 4096)
    try:
        manifest = json.loads(raw)
    finally:
        raw[:] = b"\0" * len(raw)
    if not isinstance(manifest, dict) or set(manifest) != {"schema", "created_utc", "files"} or (
        type(manifest["schema"]) is not int or manifest["schema"] != 1 or
        not isinstance(manifest["created_utc"], str) or
        not isinstance(manifest["files"], dict) or
        set(manifest["files"]) != set(FILES)):
        raise ValueError("Invalid private backup manifest")
    check_payloads(root, manifest, include_manifest=True)
    return manifest


def verify(source):
    root = private_directory(source)
    verified_manifest(root)
    return len(FILES)


def restore_to_fresh_directory(source, destination):
    backup = private_directory(source)
    manifest = verified_manifest(backup)
    target = make_private_destination(destination)
    for name, maximum in FILES.items():
        data = read_private(backup, name, maximum, name in ("audit.jsonl", "replay.jsonl"))
        try:
            meta = manifest["files"][name]
            if len(data) != meta["bytes"] or hashlib.sha256(data).hexdigest() != meta["sha256"]:
                raise ValueError("Backup changed since verification")
            validate_format(name, data)
            write_private(target, name, data)
        finally:
            data[:] = b"\0" * len(data)
    sync_directory(target)
    # Validate the written five-file bundle against the original manifest;
    # the gateway data directory must not acquire an extra manifest file.
    check_payloads(target, manifest, include_manifest=False)
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
