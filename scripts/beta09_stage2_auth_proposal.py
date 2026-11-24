#!/usr/bin/env python3
"""IN-MEMORY design probe, not a deployable private-store authenticator.

No reads/writes of real files, runtime integration, key storage, migration or
acceptance side effects. Owner approval is required before any v2 deployment.
An independently retained checkpoint is the anti-rollback trust boundary.
"""

import hashlib
import hmac
import json
import re


FILES = ("audit.jsonl", "mfa.json", "passkeys.json", "rate-limits.json", "replay.jsonl")
DOMAIN = b"dc24h-beta09-five-store-DESIGN-ONLY-v2\x00"
HEX = re.compile(r"[0-9a-f]{64}\Z")
MAX_BYTES = {"audit.jsonl": 8 * 1024 * 1024, "mfa.json": 16384,
             "passkeys.json": 1024 * 1024, "rate-limits.json": 65536,
             "replay.jsonl": 1024 * 1024}


def _key(key):
    if not isinstance(key, (bytes, bytearray)) or len(key) != 32:
        raise ValueError("Independent 256-bit test key required")


def _stores(files):
    if not isinstance(files, dict) or set(files) != set(FILES):
        raise ValueError("Five complete private components required")
    result = {}
    for name in FILES:
        value = files[name]
        if not isinstance(value, bytes) or len(value) > MAX_BYTES[name] or (
            not value and name not in ("audit.jsonl", "replay.jsonl")
        ):
            raise ValueError("Invalid private component size/type")
        result[name] = {"size": len(value), "sha256": hashlib.sha256(value).hexdigest()}
    return result


def _mac(payload, key):
    body = json.dumps(payload, sort_keys=True, separators=(",", ":"), ensure_ascii=True).encode()
    return hmac.new(key, DOMAIN + body, hashlib.sha256).hexdigest()


def _key_id(key):
    return hashlib.sha256(key).hexdigest()[:32]


def propose(files, key, generation, pair_hash, previous_mac="0" * 64):
    """Return synthetic candidate metadata; it cannot be installed or trusted."""
    _key(key)
    if type(generation) is not int or not 0 < generation <= 2**53 - 1 or (
        not isinstance(pair_hash, str) or not HEX.fullmatch(pair_hash) or
        not isinstance(previous_mac, str) or not HEX.fullmatch(previous_mac)
    ):
        raise ValueError("Invalid proposal generation or pair anchor")
    body = {"schema": 2, "generation": generation, "key_id": _key_id(key),
            "pair_sha256": pair_hash,
            "previous_mac": previous_mac, "stores": _stores(files)}
    return {**body, "mac": _mac(body, key)}


def inspect(candidate, files, key, checkpoint, pair_hash):
    """Pure fail-closed proof against an OUT-OF-BAND trusted checkpoint.

    Passing this function proves only the synthetic design arithmetic. It is
    not an approved runtime startup preflight or a filesystem-atomic capture.
    """
    _key(key)
    if not isinstance(candidate, dict) or set(candidate) != {
        "schema", "generation", "key_id", "pair_sha256", "previous_mac", "stores", "mac"
    } or not isinstance(candidate.get("mac"), str) or not HEX.fullmatch(candidate["mac"]):
        raise ValueError("Invalid detached authenticity proposal")
    if not isinstance(checkpoint, dict) or set(checkpoint) != {"generation", "mac"} or (
        type(checkpoint["generation"]) is not int or not isinstance(checkpoint["mac"], str) or
        not HEX.fullmatch(checkpoint["mac"])
    ):
        raise ValueError("Missing independent Owner checkpoint")
    body = {key: value for key, value in candidate.items() if key != "mac"}
    if not hmac.compare_digest(candidate["mac"], _mac(body, key)):
        raise ValueError("Unauthenticated detached manifest")
    if candidate["mac"] != checkpoint["mac"] or candidate["generation"] != checkpoint["generation"]:
        raise ValueError("Rollback or unpinned manifest refused")
    if body != {"schema": 2, "generation": checkpoint["generation"], "key_id": _key_id(key),
                "pair_sha256": pair_hash, "previous_mac": candidate["previous_mac"],
                "stores": _stores(files)}:
        raise ValueError("Torn or changed five-store pair/generation")
    return {"schema": 1, "design_only": True, "files_matched": 5,
            "runtime_authenticity_activated": False}


def inspect_successor(candidate, files, key, checkpoint, pair_hash):
    """Prove arithmetic of a *proposed* next generation; never persist it."""
    if not isinstance(candidate, dict):
        raise ValueError("Invalid successor manifest")
    if not isinstance(checkpoint, dict) or type(checkpoint.get("generation")) is not int or (
        not isinstance(checkpoint.get("mac"), str) or not HEX.fullmatch(checkpoint["mac"])
    ) or type(candidate.get("generation")) is not int or (
        candidate["generation"] != checkpoint["generation"] + 1 or
        candidate.get("previous_mac") != checkpoint["mac"]
    ):
        raise ValueError("Nonmonotonic or missing predecessor")
    future = {"generation": candidate["generation"], "mac": candidate["mac"]}
    return inspect(candidate, files, key, future, pair_hash)
