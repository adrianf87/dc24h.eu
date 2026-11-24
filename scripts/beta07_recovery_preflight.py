#!/usr/bin/env python3
"""Read-only, fail-closed preflight of an offline six-component recovery pair.

This command NEVER initializes MariaDB, imports SQL, restores files, or stops
writers. Operators must independently prove stopped application writers and
schema-v11 authenticity; hash matching alone cannot prove either property.
"""

import argparse
import json
from pathlib import Path
import sys

import stage1_recovery_bundle as pair
import stage4_private_recovery_drill as private


def preflight(dump, snapshot, pair_directory, commit_sha, sql_destination, private_destination):
    """Prove source integrity and that both chosen destinations are fresh.

    No file system changes are made; dangling symlinks are not fresh paths.
    This intentionally does not equate an arbitrary SQL dump with schema v11.
    """
    sources = [Path(dump), Path(snapshot), Path(pair_directory)]
    targets = [Path(sql_destination), Path(private_destination)]
    if any(not path.is_absolute() for path in sources + targets):
        raise ValueError("Absolute offline recovery paths required")
    if targets[0] == targets[1]:
        raise ValueError("SQL and private destinations must be separate")
    for target in targets:
        # Never stage inside either source or within the other destination.
        resolved = target.resolve(strict=False)
        for source in sources:
            original = source.resolve(strict=True)
            if resolved == original or resolved.is_relative_to(original):
                raise ValueError("Recovery target overlaps original evidence")
        for other in targets:
            if target is not other:
                another = other.resolve(strict=False)
                if resolved.is_relative_to(another) or another.is_relative_to(resolved):
                    raise ValueError("Recovery destinations overlap")
        private.private_directory(target.parent)
        if target.is_symlink() or target.exists():
            raise ValueError("Recovery target already exists")
    evidence = pair.verified_pair(sources[0], sources[1], sources[2] / pair.NAME, commit_sha)
    # Report only an exact commit, counts and booleans. The SQL dump, private
    # material, local paths and even account/host labels are not emitted.
    return {"schema": 1, "source_sha": evidence["commit_sha"],
            "verified_components": evidence["verified_files"],
            "pair_sha256": evidence["pair_hash"], "fresh_targets_checked": 2,
            "schema_v11_verified_after_import": False,
            "writers_quiescent_verified": False,
            "restore_performed": False, "destructive_changes": False}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mariadb-dump", type=Path, required=True)
    parser.add_argument("--private-snapshot", type=Path, required=True)
    parser.add_argument("--pair-directory", type=Path, required=True)
    parser.add_argument("--commit-sha", required=True)
    parser.add_argument("--fresh-sql-destination", type=Path, required=True)
    parser.add_argument("--fresh-private-destination", type=Path, required=True)
    parser.add_argument("--ack-isolated", action="store_true")
    args = parser.parse_args(argv)
    if not args.ack_isolated:
        parser.error("Explicit isolated disposable fixture acknowledgement required")
    try:
        result = preflight(args.mariadb_dump, args.private_snapshot,
                           args.pair_directory, args.commit_sha,
                           args.fresh_sql_destination, args.fresh_private_destination)
    except (OSError, ValueError, TypeError, KeyError, UnicodeError, json.JSONDecodeError):
        print("Recovery preflight refused; verify isolated offline evidence privately.", file=sys.stderr)
        return 1
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
