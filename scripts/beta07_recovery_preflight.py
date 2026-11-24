#!/usr/bin/env python3
"""Read-only, fail-closed preflight of an offline six-component recovery pair.

This command NEVER initializes MariaDB, imports SQL, restores files, or stops
writers. Operators must independently prove stopped application writers and
schema-v11 authenticity; hash matching alone cannot prove either property.
"""

import argparse
import json
from pathlib import Path
import re
import sys

import stage1_recovery_bundle as pair
import stage4_private_recovery_drill as private


MIGRATION_CREATE = re.compile(rb"^CREATE TABLE(?: IF NOT EXISTS)?\s+`?dc24h_schema_migrations`?\s*\($", re.I)
MIGRATION_INSERT = re.compile(rb"^INSERT INTO\s+`?dc24h_schema_migrations`?\s+VALUES\s+", re.I)
# A *necessary* check of a standard mariadb-dump export, never a SQL parser,
# schema migration, statement sanitizer or substitute for C++ inspection after
# an import on a disposable fresh MariaDB instance.
MIGRATION_ROW = re.compile(
    rb"\(\s*([1-9][0-9]*)\s*,\s*'([a-z0-9_]+)'\s*,\s*'([0-9a-f]{64})'\s*,\s*"
    rb"'([0-9]{4}-[0-9]{2}-[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2}(?:\.[0-9]{1,6})?)'\s*,\s*'applied'\s*\)"
)


def sql_migration_row_count(dump):
    """Fail closed unless a standard offline export contains all 11 applied rows.

    No SQL is executed, printed or parsed as trusted executable statements.
    Caller already verified this owner's SQL file and matched its paired hash.
    """
    table_count, versions = 0, {}
    with Path(dump).open("rb") as source:
        while line := source.readline(1024 * 1024 + 1):
            stripped = line.lstrip()
            if len(line) > 1024 * 1024 or not line.endswith(b"\n"):
                if b"dc24h_schema_migrations" in stripped[:128]:
                    raise ValueError("Oversized migration export line")
                # Other tables may legitimately contain huge extended
                # INSERTs. Skip their remainder in bounded chunks, without
                # interpreting or copying their possibly private values.
                while line and not line.endswith(b"\n"):
                    line = source.readline(1024 * 1024 + 1)
                continue
            if MIGRATION_CREATE.fullmatch(stripped.rstrip(b"\r\n")):
                table_count += 1
            prefix = MIGRATION_INSERT.match(stripped)
            if not prefix:
                continue
            values = stripped[prefix.end():].rstrip(b"\r\n")
            matches = list(MIGRATION_ROW.finditer(values))
            if not matches:
                raise ValueError("Malformed migration export (row shape)")
            # MariaDB versions can format separators and timestamp fractions
            # differently. Accept only whitespace around complete tuples and
            # commas, never arbitrary unparsed SQL between valid rows.
            cursor = 0
            valid = bool(matches)
            for match in matches:
                separator = values[cursor:match.start()].strip()
                if separator != (b"" if cursor == 0 else b","):
                    valid = False
                    break
                cursor = match.end()
            if not valid:
                raise ValueError("Malformed migration export (row separator)")
            if values[cursor:].strip() != b";":
                raise ValueError("Malformed migration export (row terminator)")
            for match in matches:
                version = int(match.group(1))
                if version in versions or version > 11:
                    raise ValueError("Unexpected/duplicate migration version")
                versions[version] = True
    if table_count != 1:
        raise ValueError(f"Migration table declaration count invalid ({min(table_count, 2)})")
    if set(versions) != set(range(1, 12)):
        raise ValueError(f"Applied schema-v11 migration row count invalid ({min(len(versions), 12)})")
    return len(versions)


def preflight(dump, snapshot, pair_directory, commit_sha, sql_destination, private_destination):
    """Prove source integrity and that both chosen destinations are fresh.

    No file system changes are made; dangling symlinks are not fresh paths.
    The SQL-migration scan is a necessary offline format check; only C++ and
    MariaDB can prove imported schema/version validity on a fresh instance.
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
    migration_rows = sql_migration_row_count(sources[0])
    # Report only an exact commit, counts and booleans. The SQL dump, private
    # material, local paths and even account/host labels are not emitted.
    return {"schema": 1, "source_sha": evidence["commit_sha"],
            "verified_components": evidence["verified_files"],
            "pair_sha256": evidence["pair_hash"], "fresh_targets_checked": 2,
            "sql_export_applied_migration_rows": migration_rows,
            "sql_export_shape_checked": True,
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
