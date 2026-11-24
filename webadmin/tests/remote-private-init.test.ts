import assert from "node:assert/strict";
import { test } from "node:test";
import { chmodSync, mkdtempSync, readFileSync, rmSync, symlinkSync, unlinkSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { initializeRemotePrivateState } from "../server/remote-private-init.ts";
import { loadTotpVerifier } from "../server/remote-mfa.ts";
import { openRemoteAuditStore } from "../server/remote-audit-store.ts";
import { openPasskeyStore } from "../server/remote-passkeys.ts";
import { openRemoteRateLimitStore } from "../server/remote-rate-limit-store.ts";

const secret = "A".repeat(32);
const token = "a".repeat(64);
const file = (dir: string, name: string) => join(dir, name);

test("first-installation bootstrap creates only four non-secret files and refuses a reset", () => {
  const dir = mkdtempSync(join(tmpdir(), "dc24h-private-init-")); chmodSync(dir, 0o700);
  try {
    const mfa = file(dir, "mfa.json");
    writeFileSync(mfa, JSON.stringify({ schema: 1, accounts: { "7": secret } }), { mode: 0o600 });
    initializeRemotePrivateState(dir, "localhost");
    const verifier = loadTotpVerifier(mfa, file(dir, "replay.jsonl"));
    verifier.close();
    const audit = openRemoteAuditStore(file(dir, "audit.jsonl"));
    audit.append("login_refused", 1_700_000_000_000);
    audit.close();
    const passkeys = openPasskeyStore(file(dir, "passkeys.json"), "localhost");
    passkeys.userHandle("7");
    const rate = openRemoteRateLimitStore(file(dir, "rate-limits.json"), token);
    rate.hit("login:7", 4, 100_000);
    const records = ["replay.jsonl", "audit.jsonl", "passkeys.json", "rate-limits.json"]
      .map(name => readFileSync(file(dir, name), "utf8"));
    assert.throws(() => initializeRemotePrivateState(dir, "localhost"), /State already exists/);
    assert.deepEqual(["replay.jsonl", "audit.jsonl", "passkeys.json", "rate-limits.json"]
      .map(name => readFileSync(file(dir, name), "utf8")), records);
    unlinkSync(file(dir, "rate-limits.json"));
    assert.throws(() => openRemoteRateLimitStore(file(dir, "rate-limits.json"), token), /ENOENT/);
    assert.throws(() => initializeRemotePrivateState(dir, "localhost"), /State already exists/,
      "partial state loss must not be misclassified as a first installation");
  } finally { rmSync(dir, { recursive: true, force: true }); }
});

test("bootstrap refuses symlinks and unsafe MFA enrollment without creating any state", () => {
  const dir = mkdtempSync(join(tmpdir(), "dc24h-private-init-sec-")); chmodSync(dir, 0o700);
  try {
    const mfa = file(dir, "mfa.json"), linked = file(dir, "linked.json");
    writeFileSync(linked, JSON.stringify({ schema: 1, accounts: { "7": secret } }), { mode: 0o600 });
    symlinkSync(linked, mfa);
    assert.throws(() => initializeRemotePrivateState(dir, "localhost"), /Existing private MFA enrollment/);
    unlinkSync(mfa);
    writeFileSync(mfa, readFileSync(linked), { mode: 0o600 });
    chmodSync(mfa, 0o644);
    assert.throws(() => initializeRemotePrivateState(dir, "localhost"), /Existing private MFA enrollment/);
    assert.throws(() => openRemoteAuditStore(file(dir, "audit.jsonl")), /ENOENT/);
  } finally { rmSync(dir, { recursive: true, force: true }); }
});
