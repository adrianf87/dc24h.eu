import assert from "node:assert/strict";
import { test } from "node:test";
import { createHash } from "node:crypto";
import { proposeAuditCheckpoint, verifyProposedCheckpoint } from "./beta09-audit-continuity.ts";

const zero = "0".repeat(64);
const key = Buffer.alloc(32, 0x3a); // synthetic test-only value, NOT an Owner secret
// Synthetic fixture is limited to fixed, non-personal audit event names.
// Numeric selectors avoid treating the audit-event label as a credential input.
const fixtureEvents = ["login_refused", "rate_limited", "mfa_ok", "route_refused"] as const;
const makeAuditFixtureRow = (sequence: number, previous: string, eventId: 0 | 1 | 2 | 3) => {
  const event = fixtureEvents[eventId];
  const at = "2026-11-24T00:00:00.000Z";
  const digest = createHash("sha256").update(JSON.stringify([1, sequence, previous, event, at])).digest("hex");
  return { schema: 1, sequence, previous, event, at, digest };
};
const archive = (...rows: ReturnType<typeof makeAuditFixtureRow>[]) => Buffer.from(rows.map(r => JSON.stringify(r)).join("\n") + "\n");

test("design-only Owner checkpoint binds two archived segments and a continuous audit sequence", () => {
  const a = makeAuditFixtureRow(1, zero, 0);
  const b = makeAuditFixtureRow(2, a.digest, 1);
  const first = archive(a, b);
  const checkpoint1 = proposeAuditCheckpoint(first, key);
  verifyProposedCheckpoint(first, checkpoint1, key, 1);
  const c = makeAuditFixtureRow(3, b.digest, 2);
  const second = archive(c);
  const checkpoint2 = proposeAuditCheckpoint(second, key, checkpoint1);
  verifyProposedCheckpoint(second, checkpoint2, key, 2, checkpoint1);
  assert.equal(checkpoint2.first_sequence, 3);
  assert.equal(checkpoint2.previous_checkpoint.length, 64);
  assert.notEqual(checkpoint2.previous_checkpoint, zero);
  assert.throws(() => verifyProposedCheckpoint(first, checkpoint1, key, 2), /rolled-back/);
  assert.throws(() => verifyProposedCheckpoint(second, checkpoint2, key, 2), /replayed/);
  assert.throws(() => verifyProposedCheckpoint(second, checkpoint2, Buffer.alloc(32, 0x41), 2, checkpoint1), /rolled-back/);
  assert.throws(() => verifyProposedCheckpoint(second, checkpoint2, key, 3, checkpoint1), /rolled-back/);
});

test("checkpoint key_id is a domain-separated HMAC tag, not a fast Owner key hash", () => {
  const raw = archive(makeAuditFixtureRow(1, zero, 0));
  const proof = proposeAuditCheckpoint(raw, key);
  assert.match(proof.key_id, /^[0-9a-f]{32}$/);
  assert.equal(proposeAuditCheckpoint(raw, key).key_id, proof.key_id);
  assert.notEqual(proposeAuditCheckpoint(raw, Buffer.alloc(32, 0x41)).key_id, proof.key_id);
  assert.throws(
    () => verifyProposedCheckpoint(raw, { ...proof, key_id: "0".repeat(32) }, key, 1),
    /rolled-back/,
  );
});

test("proposed audit checkpoint refuses unexpected fields in the legacy audit schema", () => {
  const fixture = makeAuditFixtureRow(1, zero, 0);
  const withUnexpectedField = Buffer.from(JSON.stringify({ ...fixture, extra: "not-allowed" }) + "\n");
  assert.throws(() => proposeAuditCheckpoint(withUnexpectedField, key), /continue/);
});

test("prototype refuses recomputed unkeyed chains, missing archives, replay, and same-size forgery", () => {
  const one = makeAuditFixtureRow(1, zero, 0);
  const raw = archive(one);
  const proof = proposeAuditCheckpoint(raw, key);
  const fake = makeAuditFixtureRow(1, zero, 3);
  const forged = archive(fake);
  assert.equal(forged.length, raw.length, "attack replaces a same-length event and recomputes the unkeyed hash");
  assert.throws(() => verifyProposedCheckpoint(forged, proof, key, 1), /foreign/);
  assert.throws(() => verifyProposedCheckpoint(Buffer.alloc(0), proof, key, 1), /foreign/);
  const bogus = { ...proof, tail_digest: fake.digest };
  assert.throws(() => verifyProposedCheckpoint(forged, bogus, key, 1), /Forged/);
  assert.throws(() => proposeAuditCheckpoint(archive(makeAuditFixtureRow(2, zero, 0)), key, proof), /continue/);
  assert.throws(() => proposeAuditCheckpoint(Buffer.alloc(9 * 1024 * 1024), key), /capacity/);
});
