import assert from "node:assert/strict";
import { test } from "node:test";
import { chmodSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { execFileSync } from "node:child_process";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { initializePasskeyStore, openPasskeyStore } from "../server/remote-passkeys.ts";
import { initializeRemoteAuditStore, openRemoteAuditStore } from "../server/remote-audit-store.ts";
import { initializeTotpReplay, loadTotpVerifier, totpCodeForTest } from "../server/remote-mfa.ts";
import { parseRecoveryRequest } from "../server/remote-passkey-recovery.ts";

test("offline recovery requires a private 2-operator request, durably audits and revokes all keys", () => {
  const dir = mkdtempSync(join(tmpdir(), "dc24h-recovery-")); chmodSync(dir, 0o700);
  try {
    const catalog = join(dir, "passkeys.json"), audit = join(dir, "audit.jsonl"), req = join(dir, "request.json");
    initializePasskeyStore(catalog, "localhost");
    const store = openPasskeyStore(catalog, "localhost");
    const handle = store.userHandle("7");
    for (const id of ["a".repeat(32), "b".repeat(32)]) store.add({
      actor: "7", userHandle: handle, id, publicKey: "c".repeat(60), counter: 0,
      deviceType: "singleDevice", backedUp: false, transports: [], name: "Passkey",
      createdAt: 10, lastUsedAt: null, revokedAt: null,
    });
    initializeRemoteAuditStore(audit);
    openRemoteAuditStore(audit).close();
    writeFileSync(req, JSON.stringify({ schema: 1, actor: "7", approvers: ["op-one", "op-two"] }), { mode: 0o600 });
    assert.equal(parseRecoveryRequest(req).actor, "7");
    assert.match(execFileSync(process.execPath, ["--import", "tsx", "server/remote-passkey-recovery.ts",
      "--catalog", catalog, "--rp-id", "localhost", "--audit", audit,
      "--request", req, "--offline-ack"], { encoding: "utf8" }), /2 keys revoked/);
    assert.equal(openPasskeyStore(catalog, "localhost").forActor("7").length, 0);
    const text = readFileSync(audit, "utf8");
    assert.match(text, /passkey_recovery_intent/);
    assert.match(text, /passkey_recovered/);
    assert.ok(!text.includes("op-one") && !text.includes('"actor"'));
    writeFileSync(req, JSON.stringify({ schema: 1, actor: "7", approvers: ["same", "same"] }));
    assert.throws(() => parseRecoveryRequest(req), /Unsafe/);
    chmodSync(req, 0o644);
    assert.throws(() => parseRecoveryRequest(req), /Unsafe/);
  } finally { rmSync(dir, { recursive: true, force: true }); }
});

test("lost-device recovery persists across restart without resetting the separate TOTP replay ledger", () => {
  const dir = mkdtempSync(join(tmpdir(), "dc24h-fallback-recovery-")); chmodSync(dir, 0o700);
  const seed = "GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ";
  try {
    const catalog = join(dir, "passkeys.json"), audit = join(dir, "audit.jsonl");
    const mfa = join(dir, "mfa.json"), replay = join(dir, "replay.jsonl"), req = join(dir, "request.json");
    initializePasskeyStore(catalog, "localhost");
    const store = openPasskeyStore(catalog, "localhost");
    for (const actor of ["7", "8"]) {
      const handle = store.userHandle(actor);
      store.add({ actor, userHandle: handle, id: actor.repeat(32), publicKey: "c".repeat(60),
        counter: 1, deviceType: "singleDevice", backedUp: false,
        transports: [], name: "Device", createdAt: 1000, lastUsedAt: null, revokedAt: null });
    }
    initializeRemoteAuditStore(audit);
    writeFileSync(mfa, JSON.stringify({ schema: 1, accounts: { "7": seed } }), { mode: 0o600 });
    initializeTotpReplay(replay);
    const factor = loadTotpVerifier(mfa, replay);
    try {
      assert.equal(factor.verify("7", totpCodeForTest(seed, 59_000), 59_000), true);
    } finally { factor.close(); }
    writeFileSync(req, JSON.stringify({ schema: 1, actor: "7", approvers: ["owner-one", "owner-two"] }),
      { mode: 0o600 });
    assert.match(execFileSync(process.execPath, ["--import", "tsx", "server/remote-passkey-recovery.ts",
      "--catalog", catalog, "--rp-id", "localhost", "--audit", audit, "--request", req, "--offline-ack"],
    { encoding: "utf8" }), /1 keys revoked/);
    const restarted = openPasskeyStore(catalog, "localhost");
    assert.equal(restarted.get("7", "7".repeat(32)), undefined,
      "the lost device must remain revoked when the file is reopened");
    assert.equal(restarted.forActor("8").length, 1,
      "recovering one actor must not remove another actor's device");
    assert.equal(restarted.epoch("7"), 1, "the old actor's sessions must be considered stale");
    const fallback = loadTotpVerifier(mfa, replay);
    try {
      assert.equal(fallback.verify("7", totpCodeForTest(seed, 59_000), 59_000), false,
        "offline passkey recovery must not make an already used TOTP code reusable");
      assert.equal(fallback.verify("7", totpCodeForTest(seed, 90_000), 90_000), true,
        "a fresh second factor remains available for a new password-authorized login");
    } finally { fallback.close(); }
    assert.ok(!readFileSync(audit, "utf8").includes(seed));
    assert.equal(readFileSync(replay, "utf8").split("\n").filter(Boolean).length, 2);
  } finally { rmSync(dir, { recursive: true, force: true }); }
});
