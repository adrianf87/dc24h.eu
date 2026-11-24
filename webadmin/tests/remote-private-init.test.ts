import assert from "node:assert/strict";
import { test } from "node:test";
import { chmodSync, mkdirSync, mkdtempSync, readFileSync, renameSync, rmSync, symlinkSync, unlinkSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { initializeRemotePrivateState } from "../server/remote-private-init.ts";
import { assertPrivateDirectory } from "../server/remote-private-path.ts";
import { initializeTotpReplay, loadTotpVerifier, totpCodeForTest } from "../server/remote-mfa.ts";
import { initializeRemoteAuditStore, openRemoteAuditStore } from "../server/remote-audit-store.ts";
import { initializePasskeyStore, openPasskeyStore } from "../server/remote-passkeys.ts";
import { initializeRemoteRateLimitStore, openRemoteRateLimitStore } from "../server/remote-rate-limit-store.ts";

const secret = "A".repeat(32);
const token = "a".repeat(64);
const file = (dir: string, name: string) => join(dir, name);

test("noncanonical private paths cannot alias an otherwise valid store", () => {
  const dir = mkdtempSync(join(tmpdir(), "dc24h-private-canonical-")); chmodSync(dir, 0o700);
  try {
    assertPrivateDirectory(dir);
    for (const alias of [dir + "/", dir + "//", dir + "/./", dir + "/../" + dir.split("/").at(-1)])
      assert.throws(() => assertPrivateDirectory(alias), /Noncanonical/);
    assert.throws(() => initializeRemotePrivateState(dir + "/./", "localhost"), /Noncanonical/);
  } finally { rmSync(dir, { recursive: true, force: true }); }
});

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

test("intermediate ancestor symlink is rejected for every private store", () => {
  const root = mkdtempSync(join(tmpdir(), "dc24h-private-ancestor-")); chmodSync(root, 0o700);
  try {
    const real = join(root, "real"); mkdirSync(real, { mode: 0o700 });
    const state = join(real, "state"); mkdirSync(state, { mode: 0o700 });
    writeFileSync(file(state, "mfa.json"),
      JSON.stringify({ schema: 1, accounts: { "7": secret } }), { mode: 0o600 });
    initializeRemotePrivateState(state, "localhost");
    const alias = join(root, "alias"); symlinkSync(real, alias, "dir");
    const throughAlias = join(alias, "state");
    assert.throws(() => initializeRemotePrivateState(throughAlias, "localhost"), /Symlink/);
    assert.throws(() => loadTotpVerifier(file(throughAlias, "mfa.json"),
      file(throughAlias, "replay.jsonl")), /Symlink/);
    assert.throws(() => openRemoteAuditStore(file(throughAlias, "audit.jsonl")), /Symlink/);
    assert.throws(() => openPasskeyStore(file(throughAlias, "passkeys.json"), "localhost"), /Symlink/);
    assert.throws(() => openRemoteRateLimitStore(file(throughAlias, "rate-limits.json"), token), /Symlink/);
    assert.throws(() => initializeRemoteAuditStore(file(throughAlias, "new-audit.jsonl")), /Symlink/);
    assert.throws(() => initializePasskeyStore(file(throughAlias, "new-passkeys.json"), "localhost"), /Symlink/);
    assert.throws(() => initializeRemoteRateLimitStore(file(throughAlias, "new-rate-limits.json")), /Symlink/);
    assert.throws(() => initializeTotpReplay(file(throughAlias, "new-replay.jsonl")), /Symlink/);
    for (const name of ["new-audit.jsonl", "new-passkeys.json", "new-rate-limits.json", "new-replay.jsonl"])
      assert.throws(() => readFileSync(file(state, name)), /ENOENT/);
    // The real path remains usable; the test proves refusal of the alias,
    // not an artificial permanent denial of a valid restored catalog.
    const good = openPasskeyStore(file(state, "passkeys.json"), "localhost");
    assert.equal(good.healthy(), true);
  } finally { rmSync(root, { recursive: true, force: true }); }
});

test("an ancestor swapped to a symlink after opening fails subsequent state checks", () => {
  const root = mkdtempSync(join(tmpdir(), "dc24h-private-swap-")); chmodSync(root, 0o700);
  try {
    const real = join(root, "real"); mkdirSync(real, { mode: 0o700 });
    const state = join(real, "state"); mkdirSync(state, { mode: 0o700 });
    writeFileSync(file(state, "mfa.json"), JSON.stringify({ schema: 1, accounts: { "7": secret } }),
      { mode: 0o600 });
    initializeRemotePrivateState(state, "localhost");
    const mfa = loadTotpVerifier(file(state, "mfa.json"), file(state, "replay.jsonl"));
    const audit = openRemoteAuditStore(file(state, "audit.jsonl"));
    const passkeys = openPasskeyStore(file(state, "passkeys.json"), "localhost");
    const rate = openRemoteRateLimitStore(file(state, "rate-limits.json"), token);
    try {
      const moved = join(root, "moved"); renameSync(real, moved);
      symlinkSync(moved, real, "dir");
      assert.throws(() => mfa.verify("7", totpCodeForTest(secret, 1_700_000_000_000), 1_700_000_000_000), /Symlink/);
      assert.equal(audit.verify(), false);
      assert.equal(passkeys.healthy(), false);
      assert.equal(rate.healthy(), false);
    } finally { mfa.close(); audit.close(); }
  } finally { rmSync(root, { recursive: true, force: true }); }
});
