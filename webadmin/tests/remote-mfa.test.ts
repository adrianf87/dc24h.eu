import assert from "node:assert/strict";
import { test } from "node:test";
import { appendFileSync, chmodSync, copyFileSync, mkdirSync, mkdtempSync, readFileSync, rmSync, symlinkSync, unlinkSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { compactTotpReplay, initializeTotpReplay, loadTotpVerifier, replaceTotpSecret, totpCodeForTest } from "../server/remote-mfa.ts";

// RFC 6238 SHA-1 vector at t=59s: 8-digit 94287082, six-digit 287082.
const secret = "GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ";
const replacement = "JBSWY3DPEHPK3PXPJBSWY3DPEHPK3PXP";

test("RFC 6238 TOTP checks time, account, input and one-time replay across restarts", () => {
  assert.equal(totpCodeForTest(secret, 59_000), "287082");
  const dir = mkdtempSync(join(tmpdir(), "dc24h-mfa-"));
  const enrollment = join(dir, "mfa.json"), replay = join(dir, "replay.jsonl");
  try {
    writeFileSync(enrollment, JSON.stringify({ schema: 1, accounts: { "7": secret } }), { mode: 0o600 });
    initializeTotpReplay(replay);
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
    initializeTotpReplay(replay);
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

test("deleted TOTP replay history refuses restart and bootstrap never overwrites a prior ledger", () => {
  const dir = mkdtempSync(join(tmpdir(), "dc24h-mfa-missing-"));
  const enrollment = join(dir, "mfa.json"), replay = join(dir, "replay.jsonl");
  try {
    writeFileSync(enrollment, JSON.stringify({ schema: 1, accounts: { "7": secret } }), { mode: 0o600 });
    assert.throws(() => loadTotpVerifier(enrollment, replay), /ENOENT/,
      "a missing consumed-step history must never be treated as a fresh empty ledger");
    initializeTotpReplay(replay);
    const verifier = loadTotpVerifier(enrollment, replay);
    assert.equal(verifier.verify("7", totpCodeForTest(secret, 59_000), 59_000), true);
    verifier.close();
    const oldHistory = readFileSync(replay, "utf8");
    assert.throws(() => initializeTotpReplay(replay), /EEXIST/);
    assert.equal(readFileSync(replay, "utf8"), oldHistory);
    unlinkSync(replay);
    assert.throws(() => loadTotpVerifier(enrollment, replay), /ENOENT/);
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
    writeFileSync(replay, '{"actor":"7","step":3}\n\n', { mode: 0o600 });
    assert.throws(() => loadTotpVerifier(enrollment, replay), /empty record/,
      "a blank line inside a replay ledger is corruption, not a valid omitted entry");
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

test("offline TOTP rotation replaces the enrollment atomically, rejects cached old seeds and preserves replay", () => {
  const dir = mkdtempSync(join(tmpdir(), "dc24h-mfa-rotate-"));
  const enrollment = join(dir, "mfa.json"), replay = join(dir, "replay.jsonl");
  try {
    writeFileSync(enrollment, JSON.stringify({ schema: 1, accounts: { "7": secret } }), { mode: 0o600 });
    initializeTotpReplay(replay);
    const old = loadTotpVerifier(enrollment, replay);
    assert.throws(() => replaceTotpSecret({ enrollmentPath: enrollment, replayPath: replay,
      actor: "7", newSeed: replacement, newCode: "000000", oldCode: totpCodeForTest(secret, 59_000),
      now: 59_000, offlineAcknowledged: true }), /New MFA authenticator proof failed/);
    assert.throws(() => replaceTotpSecret({ enrollmentPath: enrollment, replayPath: replay,
      actor: "7", newSeed: replacement, newCode: totpCodeForTest(replacement, 59_000),
      now: 59_000, offlineAcknowledged: true }), /Current MFA proof required/);
    replaceTotpSecret({ enrollmentPath: enrollment, replayPath: replay, actor: "7",
      newSeed: replacement, newCode: totpCodeForTest(replacement, 59_000),
      oldCode: totpCodeForTest(secret, 59_000), now: 59_000, offlineAcknowledged: true });
    assert.throws(() => old.verify("7", totpCodeForTest(secret, 90_000), 90_000), /restart required/);
    old.close();
    const fresh = loadTotpVerifier(enrollment, replay);
    try {
      assert.equal(fresh.verify("7", totpCodeForTest(replacement, 59_000), 59_000), false,
        "new seed cannot reuse a step consumed by an old seed");
      assert.equal(fresh.verify("7", totpCodeForTest(replacement, 90_000), 90_000), true);
      assert.equal(fresh.verify("7", totpCodeForTest(secret, 120_000), 120_000), false);
    } finally { fresh.close(); }
  } finally { rmSync(dir, { recursive: true, force: true }); }
});

test("offline MFA recovery still requires new-factor proof and preserves the old replay ledger", () => {
  const dir = mkdtempSync(join(tmpdir(), "dc24h-mfa-recover-"));
  const enrollment = join(dir, "mfa.json"), replay = join(dir, "replay.jsonl");
  try {
    writeFileSync(enrollment, JSON.stringify({ schema: 1, accounts: { "7": secret } }), { mode: 0o600 });
    writeFileSync(replay, '{"actor":"7","step":3}\n', { mode: 0o600 });
    const request = { enrollmentPath: enrollment, replayPath: replay, actor: "7", newSeed: replacement,
      newCode: totpCodeForTest(replacement, 120_000), now: 120_000, recovery: true as const, offlineAcknowledged: true as const };
    replaceTotpSecret(request);
    assert.equal(readFileSync(replay, "utf8"), '{"actor":"7","step":3}\n', "recovery must not reset replay protection");
    const verifier = loadTotpVerifier(enrollment, replay);
    try { assert.equal(verifier.verify("7", request.newCode, 120_000), true); }
    finally { verifier.close(); }
  } finally { rmSync(dir, { recursive: true, force: true }); }
});

test("offline replay compaction preserves maximum consumed steps and rejects stale verifiers", () => {
  const dir = mkdtempSync(join(tmpdir(), "dc24h-mfa-compact-"));
  const enrollment = join(dir, "mfa.json"), replay = join(dir, "replay.jsonl");
  try {
    writeFileSync(enrollment, JSON.stringify({ schema: 1, accounts: { "7": secret } }), { mode: 0o600 });
    writeFileSync(replay, '{"actor":"7","step":0}\n{"actor":"7","step":1}\n{"actor":"7","step":1}\n', { mode: 0o600 });
    const old = loadTotpVerifier(enrollment, replay);
    compactTotpReplay(replay, true);
    assert.equal(readFileSync(replay, "utf8"), '{"actor":"7","step":1}\n');
    assert.throws(() => old.verify("7", totpCodeForTest(secret, 90_000), 90_000), /replay ledger changed/);
    old.close();
    const fresh = loadTotpVerifier(enrollment, replay);
    try {
      assert.equal(fresh.verify("7", totpCodeForTest(secret, 59_000), 59_000), false);
      assert.equal(fresh.verify("7", totpCodeForTest(secret, 90_000), 90_000), true);
    } finally { fresh.close(); }
  } finally { rmSync(dir, { recursive: true, force: true }); }
});

test("stopped-writer TOTP restore keeps consumed steps across clock rollback, rotation and partial append", () => {
  const dir = mkdtempSync(join(tmpdir(), "beta07-mfa-recover-"));
  const enrollment = join(dir, "mfa.json"), replay = join(dir, "replay.jsonl");
  const recoveredDir = join(dir, "recovered"), recoveredEnrollment = join(recoveredDir, "mfa.json");
  const recoveredReplay = join(recoveredDir, "replay.jsonl");
  try {
    writeFileSync(enrollment, JSON.stringify({ schema: 1, accounts: { "7": secret } }), { mode: 0o600 });
    initializeTotpReplay(replay);
    const original = loadTotpVerifier(enrollment, replay);
    assert.equal(original.verify("7", totpCodeForTest(secret, 150_000), 150_000), true);
    assert.equal(original.verify("7", totpCodeForTest(secret, 60_000), 60_000), false,
      "clock rollback must not revive any earlier time step");
    original.close();
    const savedLedger = readFileSync(replay, "utf8");
    appendFileSync(replay, '{"actor":"7","step":'); // Simulated interrupted append.
    assert.throws(() => loadTotpVerifier(enrollment, replay), /Incomplete MFA replay ledger/);
    assert.throws(() => replaceTotpSecret({ enrollmentPath: enrollment, replayPath: replay,
      actor: "7", newSeed: replacement, newCode: totpCodeForTest(replacement, 180_000),
      oldCode: totpCodeForTest(secret, 180_000), now: 180_000, offlineAcknowledged: true }),
    /Incomplete MFA replay ledger/, "rotation must reject a damaged replay store");
    // A byte-exact offline backup, not a newly initialized blank ledger, restores the state.
    writeFileSync(replay, savedLedger, { mode: 0o600 });
    replaceTotpSecret({ enrollmentPath: enrollment, replayPath: replay,
      actor: "7", newSeed: replacement, newCode: totpCodeForTest(replacement, 180_000),
      oldCode: totpCodeForTest(secret, 180_000), now: 180_000, offlineAcknowledged: true });
    mkdirSync(recoveredDir, { mode: 0o700 });
    copyFileSync(enrollment, recoveredEnrollment); copyFileSync(replay, recoveredReplay);
    chmodSync(recoveredEnrollment, 0o600); chmodSync(recoveredReplay, 0o600);
    const reopened = loadTotpVerifier(recoveredEnrollment, recoveredReplay);
    try {
      assert.equal(reopened.verify("7", totpCodeForTest(replacement, 180_000), 180_000), false,
        "rotation consumed this step in the matching replay ledger");
      assert.equal(reopened.verify("7", totpCodeForTest(secret, 210_000), 210_000), false,
        "old authenticator must fail after cutover");
      assert.equal(reopened.verify("7", totpCodeForTest(replacement, 210_000), 210_000), true);
    } finally { reopened.close(); }
  } finally { rmSync(dir, { recursive: true, force: true }); }
});
