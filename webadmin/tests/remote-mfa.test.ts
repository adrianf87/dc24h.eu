import assert from "node:assert/strict";
import { test } from "node:test";
import { chmodSync, mkdtempSync, rmSync, symlinkSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { loadTotpVerifier, totpCodeForTest } from "../server/remote-mfa.ts";

// RFC 6238 SHA-1 vector at t=59s: 8-digit 94287082, six-digit 287082.
const secret = "GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ";

test("RFC 6238 TOTP checks time, account, input and one-time replay across restarts", () => {
  assert.equal(totpCodeForTest(secret, 59_000), "287082");
  const dir = mkdtempSync(join(tmpdir(), "dc24h-mfa-"));
  const enrollment = join(dir, "mfa.json"), replay = join(dir, "replay.jsonl");
  try {
    writeFileSync(enrollment, JSON.stringify({ schema: 1, accounts: { "7": secret } }), { mode: 0o600 });
    const first = loadTotpVerifier(enrollment, replay);
    assert.equal(first.verify("8", "287082", 59_000), false, "account binding");
    assert.equal(first.verify("7", "0287082", 59_000), false, "seven digits");
    assert.equal(first.verify("7", "287082", 59_000), true);
    assert.equal(first.verify("7", "287082", 59_000), false, "same-step replay");
    first.close();
    const second = loadTotpVerifier(enrollment, replay);
    assert.equal(second.verify("7", "287082", 59_000), false, "restart replay");
    assert.equal(second.verify("7", totpCodeForTest(secret, 90_000), 90_000), true);
    second.close();
  } finally { rmSync(dir, { recursive: true, force: true }); }
});

test("independently loaded MFA verifiers cannot both consume the same shared ledger step", () => {
  const dir = mkdtempSync(join(tmpdir(), "dc24h-mfa-shared-"));
  const enrollment = join(dir, "mfa.json"), replay = join(dir, "replay.jsonl");
  try {
    writeFileSync(enrollment, JSON.stringify({ schema: 1, accounts: { "7": secret } }), { mode: 0o600 });
    const first = loadTotpVerifier(enrollment, replay);
    const second = loadTotpVerifier(enrollment, replay);
    try {
      assert.equal(first.verify("7", totpCodeForTest(secret, 59_000), 59_000), true);
      assert.equal(second.verify("7", totpCodeForTest(secret, 59_000), 59_000), false,
        "a verifier opened before the first acceptance must see the consumed step");
      assert.equal(second.verify("7", totpCodeForTest(secret, 90_000), 90_000), true);
      assert.equal(first.verify("7", totpCodeForTest(secret, 90_000), 90_000), false);
    } finally { first.close(); second.close(); }
  } finally { rmSync(dir, { recursive: true, force: true }); }
});

test("MFA replay ledger preserves legacy records and rejects malformed append markers", () => {
  const dir = mkdtempSync(join(tmpdir(), "dc24h-mfa-ledger-"));
  const enrollment = join(dir, "mfa.json"), replay = join(dir, "replay.jsonl");
  try {
    writeFileSync(enrollment, JSON.stringify({ schema: 1, accounts: { "7": secret } }), { mode: 0o600 });
    writeFileSync(replay, '{"actor":"7","step":1}\n', { mode: 0o600 });
    const verifier = loadTotpVerifier(enrollment, replay);
    try {
      assert.equal(verifier.verify("7", totpCodeForTest(secret, 59_000), 59_000), false,
        "legacy consumed time steps remain protected");
      assert.equal(verifier.verify("7", totpCodeForTest(secret, 90_000), 90_000), true);
    } finally { verifier.close(); }
    // A partial or invalid marker cannot be silently treated as unused state.
    writeFileSync(replay, '{"actor":"7","step":3,"nonce":"invalid"}\n', { mode: 0o600 });
    assert.throws(() => loadTotpVerifier(enrollment, replay), /Invalid MFA replay entry/);
  } finally { rmSync(dir, { recursive: true, force: true }); }
});

test("MFA enrollment and replay ledger reject loose permissions and symlink attacks", () => {
  const dir = mkdtempSync(join(tmpdir(), "dc24h-mfa-sec-"));
  const enrollment = join(dir, "mfa.json"), replay = join(dir, "replay.jsonl");
  try {
    writeFileSync(enrollment, JSON.stringify({ schema: 1, accounts: { "7": secret } }), { mode: 0o600 });
    chmodSync(enrollment, 0o644);
    assert.throws(() => loadTotpVerifier(enrollment, replay));
    chmodSync(enrollment, 0o600);
    writeFileSync(enrollment, JSON.stringify({ schema: 1, accounts: { "7": secret, "8": secret } }), { mode: 0o600 });
    assert.throws(() => loadTotpVerifier(enrollment, replay), /secrets must differ/);
    writeFileSync(enrollment, JSON.stringify({ schema: 1, accounts: { "7": secret } }), { mode: 0o600 });
    writeFileSync(replay, "", { mode: 0o600 });
    const alias = join(dir, "symlink.jsonl");
    symlinkSync(replay, alias);
    assert.throws(() => loadTotpVerifier(enrollment, alias));
    writeFileSync(replay, '{"actor":"7","step":1}', { mode: 0o600 });
    assert.throws(() => loadTotpVerifier(enrollment, replay), /Incomplete MFA replay/);
  } finally { rmSync(dir, { recursive: true, force: true }); }
});
