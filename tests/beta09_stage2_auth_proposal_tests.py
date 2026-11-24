"""Only deterministic fake content: no real secrets, keys or live migration."""

import copy
import json
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import beta09_stage2_auth_proposal as model


KEY = b"K" * 32  # Completely synthetic design-only bytes, never a real operator key.
WRONG = b"W" * 32
PAIR = "a" * 64


def contents():
    return {
        "mfa.json": b'{"schema":1,"accounts":{"7":"' + b"A" * 32 + b'"}}\n',
        "replay.jsonl": b'{"actor":"7","step":4}\n',
        "audit.jsonl": b'{"synthetic":"hash-chain-placeholder"}\n',
        "passkeys.json": b'{"schema":1,"credentials":[{"counter":4}]}\n',
        "rate-limits.json": b'{"schema":1,"buckets":[{"count":4}]}\n',
    }


class DesignOnlyAuthenticity(unittest.TestCase):
    def setUp(self):
        self.files = contents()
        self.manifest = model.propose(self.files, KEY, 5, PAIR)
        self.pin = {"generation": 5, "mac": self.manifest["mac"]}

    def test_independent_pin_allows_matched_synthetic_set_only(self):
        self.assertEqual(model.inspect(self.manifest, self.files, KEY, self.pin, PAIR),
                         {"schema": 1, "design_only": True, "files_matched": 5,
                          "runtime_authenticity_activated": False})
        self.assertEqual(self.manifest["stores"].keys(), set(model.FILES))
        self.assertNotIn("AAAA", json.dumps(self.manifest))

    def test_same_length_mtime_invariant_offline_tampering_is_denied(self):
        for path, old, new in (
            ("mfa.json", b"A" * 32, b"B" * 32),
            ("replay.jsonl", b'"step":4', b'"step":1'),
            ("passkeys.json", b'"counter":4', b'"counter":0'),
            ("rate-limits.json", b'"count":4', b'"count":1'),
            ("audit.jsonl", b"placeholder", b"PLACEHOLDER"),
        ):
            altered = dict(self.files)
            altered[path] = altered[path].replace(old, new)
            self.assertEqual(len(altered[path]), len(self.files[path]))
            with self.subTest(path=path), self.assertRaisesRegex(ValueError, "Torn or changed"):
                model.inspect(self.manifest, altered, KEY, self.pin, PAIR)
        # File mtime is intentionally not a trusted authenticator input.

    def test_missing_or_foreign_five_store_and_sql_pair_are_denied(self):
        for path in model.FILES:
            altered = dict(self.files)
            del altered[path]
            with self.subTest(path=path), self.assertRaisesRegex(ValueError, "Five complete"):
                model.inspect(self.manifest, altered, KEY, self.pin, PAIR)
        with self.assertRaisesRegex(ValueError, "Torn or changed"):
            model.inspect(self.manifest, self.files, KEY, self.pin, "b" * 64)

    def test_wrong_lost_key_and_forged_tag_do_not_authenticate(self):
        with self.assertRaisesRegex(ValueError, "Unauthenticated"):
            model.inspect(self.manifest, self.files, WRONG, self.pin, PAIR)
        with self.assertRaisesRegex(ValueError, "256-bit"):
            model.inspect(self.manifest, self.files, b"", self.pin, PAIR)
        bad = copy.deepcopy(self.manifest)
        bad["mac"] = "0" * 64
        with self.assertRaisesRegex(ValueError, "Unauthenticated"):
            model.inspect(bad, self.files, KEY, self.pin, PAIR)
        with self.assertRaisesRegex(ValueError, "independent Owner checkpoint"):
            model.inspect(self.manifest, self.files, KEY, None, PAIR)

    def test_replayed_manifest_old_generation_and_reused_pin_are_denied(self):
        newer = model.propose(self.files, KEY, 6, PAIR, self.manifest["mac"])
        self.assertTrue(model.inspect_successor(newer, self.files, KEY, self.pin, PAIR)["design_only"])
        new_pin = {"generation": 6, "mac": newer["mac"]}
        with self.assertRaisesRegex(ValueError, "Rollback"):
            model.inspect(self.manifest, self.files, KEY, new_pin, PAIR)
        with self.assertRaisesRegex(ValueError, "Nonmonotonic"):
            model.inspect_successor(self.manifest, self.files, KEY, new_pin, PAIR)
        with self.assertRaisesRegex(ValueError, "Nonmonotonic"):
            model.inspect_successor(model.propose(self.files, KEY, 7, PAIR, self.manifest["mac"]),
                                    self.files, KEY, self.pin, PAIR)

    def test_tampered_generation_or_extra_field_fails(self):
        bad = copy.deepcopy(self.manifest)
        bad["generation"] = 6
        with self.assertRaisesRegex(ValueError, "Unauthenticated"):
            model.inspect(bad, self.files, KEY, self.pin, PAIR)
        bad = copy.deepcopy(self.manifest)
        bad["unenforced_backup"] = True
        with self.assertRaisesRegex(ValueError, "Invalid detached"):
            model.inspect(bad, self.files, KEY, self.pin, PAIR)


if __name__ == "__main__":
    unittest.main()
