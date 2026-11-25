#!/usr/bin/env python3
"""Offline, non-overwriting backup and restore rehearsal for private WebAdmin state.

Only use on an isolated host after stopping the gateway. MariaDB backup/restore
is a separate operator-approved step. This tool never touches a live service.
"""

import argparse
import base64
import binascii
from contextlib import contextmanager
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
    "rate_limiter_unavailable", "rate_limited",
}


def safe_time(value):
    return type(value) is int and 0 <= value <= MAX_SAFE_INTEGER


def safe_actor(value, enforce_int64=False):
    return (isinstance(value, str) and ACTOR.fullmatch(value) is not None and
            (not enforce_int64 or int(value) <= MAX_ACCOUNT_ID))


def canonical_identifier(value):
    # Match the running Node passkey verifier: unpadded base64url has exactly
    # one encoding per byte string, including its otherwise ignored tail bits.
    if not isinstance(value, str) or not ID.fullmatch(value):
        return False
    try:
        decoded = base64.urlsafe_b64decode(value + "=" * (-len(value) % 4))
        return base64.urlsafe_b64encode(decoded).decode("ascii").rstrip("=") == value
    except (ValueError, binascii.Error):
        return False


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
            not safe_actor(user["actor"], True) or not canonical_identifier(user["handle"]) or
            not safe_time(user["epoch"]) or
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
            not canonical_identifier(item["id"]) or item["id"] in ids or
            item["userHandle"] != actors[item["actor"]] or
            not canonical_identifier(item["publicKey"]) or
            not safe_time(item["counter"]) or item["deviceType"] not in ("singleDevice", "multiDevice") or
            type(item["backedUp"]) is not bool or
            (item["deviceType"] == "singleDevice" and item["backedUp"]) or
            not isinstance(item["transports"], list) or len(item["transports"]) > 7 or
            any(not isinstance(t, str) or t not in TRANSPORTS for t in item["transports"]) or
            len(set(item["transports"])) != len(item["transports"]) or
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
    if ".." in candidate.parts:
        raise ValueError("Noncanonical private directory")
    # lstat(candidate) alone follows symlinks in its ancestors. An operator
    # supplied path must never escape through an intermediate symlink, even
    # when the final directory itself has owner-only permissions.
    ancestor = Path(candidate.anchor)
    fingerprints = []
    for component in candidate.parts[1:]:
        ancestor = ancestor / component
        metadata = ancestor.lstat()
        if not stat.S_ISDIR(metadata.st_mode):
            raise ValueError("Symlink or non-directory in private path")
        fingerprints.append((ancestor, metadata.st_dev, metadata.st_ino))
    for ancestor, device, inode in fingerprints:
        metadata = ancestor.lstat()
        if (not stat.S_ISDIR(metadata.st_mode) or
                (metadata.st_dev, metadata.st_ino) != (device, inode)):
            raise ValueError("Private path changed during validation")
    s = candidate.lstat()
    if not stat.S_ISDIR(s.st_mode) or s.st_uid != os.geteuid() or s.st_mode & 0o077:
        raise ValueError("Unsafe private directory")
    return candidate


@contextmanager
def private_directory_fd(path):
    """Pin a checked directory so later file opens cannot follow a replaced parent.

    Absolute path checks alone have a validation-to-open race. All file opens
    using this descriptor are relative to the same verified directory inode.
    """
    directory = private_directory(path)
    before = directory.lstat()
    fd = os.open(directory, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    try:
        opened = os.fstat(fd)
        after = private_directory(directory).lstat()
        identity = (before.st_dev, before.st_ino)
        if (opened.st_dev, opened.st_ino) != identity or (
                after.st_dev, after.st_ino) != identity:
            raise ValueError("Private directory changed while opening")
        try:
            yield fd
        finally:
            after = private_directory(directory).lstat()
            if (after.st_dev, after.st_ino) != identity:
                raise ValueError("Private directory changed during operation")
    finally:
        os.close(fd)


@contextmanager
def read_private_stream(path):
    """Stream a large private source through a pinned parent and same file FD.

    Used for SQL preflight: never materialize an operator-sized dump in RAM,
    and do not accept a pathname that was replaced while the FD was read.
    """
    file = Path(path)
    with private_directory_fd(file.parent) as directory_fd:
        fd = os.open(file.name, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK,
                     dir_fd=directory_fd)
        with os.fdopen(fd, "rb") as source:
            before = os.fstat(source.fileno())
            yield source
            current = os.stat(file.name, dir_fd=directory_fd, follow_symlinks=False)
            after = os.fstat(source.fileno())
            if (not stat.S_ISREG(current.st_mode) or
                    (before.st_dev, before.st_ino) != (current.st_dev, current.st_ino) or
                    (before.st_dev, before.st_ino) != (after.st_dev, after.st_ino)):
                raise ValueError("Private stream source replaced during read")


def read_private(root, name, maximum, allow_empty=False):
    # A replaced FIFO must fail promptly; a blocking open would hang before
    # fstat could reject its type.
    if Path(name).name != name or name in (".", ".."):
        raise ValueError("Private source must be a single filename")
    with private_directory_fd(root) as directory_fd:
        fd = os.open(name, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK,
                     dir_fd=directory_fd)
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
            # A same-size in-place rewrite can preserve timestamps; check
            # the same descriptor's content a second time before accepting.
            os.lseek(fd, 0, os.SEEK_SET)
            verified = 0
            while verified < len(data):
                chunk = os.read(fd, min(65536, len(data) - verified))
                if not chunk or chunk != data[verified:verified + len(chunk)]:
                    raise ValueError("Private source changed while reading")
                verified += len(chunk)
            after = os.fstat(fd)
            current = os.stat(name, dir_fd=directory_fd, follow_symlinks=False)
            if (after.st_size != s.st_size or after.st_mtime_ns != s.st_mtime_ns or
                    after.st_ctime_ns != s.st_ctime_ns or
                    (current.st_dev, current.st_ino) != (s.st_dev, s.st_ino) or
                    not stat.S_ISREG(current.st_mode)):
                raise ValueError("Private source changed while reading")
            return data
        finally:
            os.close(fd)


def write_private(root, name, data):
    if Path(name).name != name or name in (".", ".."):
        raise ValueError("Private output must be a single filename")
    with private_directory_fd(root) as directory_fd:
        fd = os.open(name, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW,
                     0o600, dir_fd=directory_fd)
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
    with private_directory_fd(path) as fd:
        os.fsync(fd)


def make_private_destination(path):
    target = Path(path)
    if not target.is_absolute():
        raise ValueError("Absolute destination required")
    private_directory(target.parent)
    target.mkdir(mode=0o700, exist_ok=False)
    sync_directory(target.parent)
    return private_directory(target)


def abandon_partial_destination(target):
    """Remove only our freshly-created private output after an interrupted copy.

    Unexpected entries or replaced inodes are preserved for investigation;
    never recursively delete an operator-selected path. The caller created
    this directory with exclusive mkdir, so there was no prior user content.
    """
    try:
        root = private_directory(target)
        children = list(root.iterdir())
        if any(child.name not in set(FILES) | {MANIFEST} for child in children):
            return
        for child in children:
            metadata = child.lstat()
            if (not stat.S_ISREG(metadata.st_mode) or metadata.st_uid != os.geteuid() or
                    metadata.st_nlink != 1 or metadata.st_mode & 0o077):
                return
        for child in children:
            child.unlink()
        root.rmdir()
        sync_directory(root.parent)
    except (OSError, ValueError):
        # Keep an uncertain partial archive rather than delete any unrelated
        # file. Permission/ownership drift is also uncertain. Cleanup must not
        # mask the original copy failure or turn it into a false success.
        pass


def separate_destination(source, destination):
    target = Path(destination)
    if not target.is_absolute():
        raise ValueError("Absolute destination required")
    original = Path(source)
    # Check both lexical and resolved paths: never create a rehearsal inside
    # the source private state or the backup that is being restored.
    if (target == original or target.is_relative_to(original) or
        target.resolve(strict=False) == original.resolve(strict=True) or
        target.resolve(strict=False).is_relative_to(original.resolve(strict=True))):
        raise ValueError("Recovery destination must be separate from its source")


def canonical_utc_timestamp(value):
    if not isinstance(value, str) or not re.fullmatch(
        r"\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d\+00:00", value
    ):
        return False
    try:
        parsed = datetime.fromisoformat(value)
        return parsed.isoformat(timespec="seconds") == value
    except ValueError:
        return False


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
                not safe_actor(row["actor"], True) or not safe_time(row["step"]) or
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
            any(not safe_actor(actor, True) or not isinstance(seed, str) or
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
    separate_destination(original, destination)
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
    try:
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
    except BaseException:
        abandon_partial_destination(target)
        raise


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
        not canonical_utc_timestamp(manifest["created_utc"]) or
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
    separate_destination(backup, destination)
    manifest = verified_manifest(backup)
    target = make_private_destination(destination)
    try:
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
    except BaseException:
        abandon_partial_destination(target)
        raise


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
