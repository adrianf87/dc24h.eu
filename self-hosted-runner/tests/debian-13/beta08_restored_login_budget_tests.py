"""Fail-closed rate-window scheduling for the real Ubuntu passkey restore CI."""
import hashlib
import hmac
import unittest

from beta06_authenticated_api_process_tests import restored_login_budget_delay


class RestoredLoginBudgetTests(unittest.TestCase):
    token = "ab" * 32
    actor = "42"
    now = 100_000

    def catalog(self, *, actor_count=4, global_count=3, started=60_000):
        def entry(name, count):
            key = hmac.new(bytes.fromhex(self.token), name.encode("ascii"), hashlib.sha256).hexdigest()
            return {"key": key, "count": count, "starts": started}
        return {"schema": 1, "buckets": [
            entry("login:global", global_count), entry("login:" + self.actor, actor_count)]}

    def delay(self, catalog, token=None):
        return restored_login_budget_delay(catalog, token or self.token, self.actor, self.now)

    def test_full_actor_budget_requires_real_expiration_without_reset(self):
        state = self.catalog()
        self.assertEqual(self.delay(state), 21.5)
        self.assertEqual(state["buckets"][1]["count"], 4)

    def test_free_actor_slot_and_expired_bucket_do_not_delay(self):
        self.assertEqual(self.delay(self.catalog(actor_count=3)), 0)
        self.assertEqual(self.delay(self.catalog(started=39_999)), 0)

    def test_global_budget_is_also_respected(self):
        self.assertEqual(self.delay(self.catalog(actor_count=3, global_count=12)), 21.5)

    def test_unknown_key_or_missing_budget_cannot_be_treated_as_free(self):
        with self.assertRaisesRegex(AssertionError, "continuity missing"):
            self.delay(self.catalog(), "cd" * 32)
        missing = self.catalog()
        missing["buckets"].pop()
        with self.assertRaisesRegex(AssertionError, "continuity missing"):
            self.delay(missing)

    def test_invalid_or_future_window_is_rejected(self):
        with self.assertRaisesRegex(AssertionError, "Invalid restored"):
            self.delay(self.catalog(actor_count=-1))
        with self.assertRaisesRegex(AssertionError, "Invalid restored"):
            self.delay(self.catalog(started=self.now + 1))


if __name__ == "__main__":
    unittest.main()
