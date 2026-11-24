#!/usr/bin/env python3
"""Offline, fail-closed staged API-token rotation with rate-budget continuity.

The gateway currently HMACs limiter keys using the local C++ API token. Its
persisted hashed buckets cannot be rekeyed. Refuse any transition until every
bucket has expired, then atomically exchange an owner-only token with a
recoverable old-key backup. Never print or export either key.
"""

import argparse
import json
import os
from pathlib import Path
import re
import secrets
import stat
import time

import stage4_private_recovery_drill as private


TOKEN = re.compile(rb"[0-9a-f]{64}\n\Z")
WINDOW_MS = 60_000


def token_data(path):
    path = Path(path)
    private.private_directory(path.parent)
    fd = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
    try:
        info = os.fstat(fd)
        if (not stat.S_ISREG(info.st_mode) or info.st_uid != os.geteuid() or
            info.st_nlink != 1 or info.st_mode & 0o077 or info.st_size != 65):
            raise ValueError("Unsafe local API token file")
        value = os.read(fd, 66)
        if not TOKEN.fullmatch(value):
            raise ValueError("Invalid local API token encoding")
        return value
    finally:
        os.close(fd)


def require_quiescent_limiter(path, now_ms):
    root = private.private_directory(Path(path).parent)
    raw = private.read_private(root, Path(path).name, private.FILES["rate-limits.json"])
    try:
        private.validate_format("rate-limits.json", raw)
        rows = json.loads(raw)["buckets"]
        if any(now_ms < row["starts"] or now_ms - row["starts"] <= WINDOW_MS for row in rows):
            raise ValueError("Active/uncertain login budgets: wait offline before rotating")
    finally:
        raw[:] = b"\0" * len(raw)


def rotate(action, *, token, candidate, backup, limiter, now_ms):
    paths = [Path(value) for value in (token, candidate, backup, limiter)]
    if any(not path.is_absolute() for path in paths) or len(set(paths)) != 4 or (
        any(path.parent != paths[0].parent for path in paths[:3])):
        raise ValueError("Absolute, distinct token paths in one private directory required")
    root = private.private_directory(paths[0].parent)
    if not isinstance(now_ms, int) or now_ms < 0:
        raise ValueError("Invalid rotation time")
    original = token_data(paths[0])
    require_quiescent_limiter(paths[3], now_ms)
    if action == "prepare":
        if paths[1].exists() or paths[2].exists() or paths[1].is_symlink() or paths[2].is_symlink():
            raise ValueError("Unfinished rotation must be resolved first")
        new = secrets.token_hex(32).encode("ascii") + b"\n"
        if new == original:
            raise ValueError("Token must change")
        fd = os.open(paths[1], os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600)
        try:
            if os.write(fd, new) != len(new):
                raise OSError("Incomplete token staging")
            os.fsync(fd)
        except BaseException:
            os.close(fd)
            paths[1].unlink(missing_ok=True)
            raise
        else:
            os.close(fd)
        private.sync_directory(root)
        return "staged"
    if action == "commit":
        proposed = token_data(paths[1])
        if proposed == original or paths[2].exists() or paths[2].is_symlink():
            raise ValueError("Unusable candidate or existing recovery backup")
        # An exclusive, fsynced copy avoids hardlinks (a hardlink would violate
        # the private token reader's single-link anti-aliasing check).
        fd = os.open(paths[2], os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600)
        try:
            if os.write(fd, original) != len(original):
                raise OSError("Incomplete old-token backup")
            os.fsync(fd)
        except BaseException:
            os.close(fd)
            paths[2].unlink(missing_ok=True)
            raise
        else:
            os.close(fd)
        private.sync_directory(root)
        try:
            os.replace(paths[1], paths[0])
        except BaseException:
            paths[2].unlink()
            raise
        private.sync_directory(root)
        if token_data(paths[0]) != proposed or token_data(paths[2]) != original:
            raise ValueError("Token cutover integrity failed; stop and recover offline")
        return "committed"
    if action == "rollback":
        previous = token_data(paths[2])
        if previous == original:
            raise ValueError("No distinct prior token to restore")
        if paths[1].exists() or paths[1].is_symlink():
            raise ValueError("Unresolved candidate must be inspected offline")
        os.replace(paths[2], paths[0])
        private.sync_directory(root)
        if token_data(paths[0]) != previous:
            raise ValueError("Token rollback integrity failed")
        return "rolled_back"
    raise ValueError("Unknown offline rotation action")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("prepare", "commit", "rollback"))
    parser.add_argument("--token", type=Path, required=True)
    parser.add_argument("--candidate", type=Path, required=True)
    parser.add_argument("--backup", type=Path, required=True)
    parser.add_argument("--rate-limits", type=Path, required=True)
    parser.add_argument("--ack-stopped", action="store_true")
    parser.add_argument("--ack-isolated", action="store_true")
    args = parser.parse_args()
    if not args.ack_stopped or not args.ack_isolated:
        parser.error("Stop all C++/Node writers and acknowledge isolated offline rotation")
    try:
        result = rotate(args.action, token=args.token, candidate=args.candidate,
                        backup=args.backup, limiter=args.rate_limits,
                        now_ms=int(time.time() * 1000))
    except (OSError, ValueError, KeyError, TypeError, json.JSONDecodeError):
        parser.error("Private token rotation refused; inspect owner-only files offline")
    print("Offline gateway token rotation " + result + "; restart and verify the local services")


if __name__ == "__main__":
    main()
