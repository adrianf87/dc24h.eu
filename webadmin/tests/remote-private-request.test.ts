import assert from "node:assert/strict";
import { test } from "node:test";
import { chmodSync, linkSync, mkdirSync, mkdtempSync, readFileSync, rmSync, symlinkSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join, relative } from "node:path";
import { readPrivateJsonRequest } from "../server/remote-private-request.ts";
import { parseRecoveryRequest } from "../server/remote-passkey-recovery.ts";
import { parseTotpMaintenanceRequest } from "../server/remote-mfa-tool.ts";

const recovery = { schema: 1, actor: "7", approvers: ["operator-one", "operator-two"] };
const rotation = { schema: 1, actor: "7", new_seed: "JBSWY3DPEHPK3PXPJBSWY3DPEHPK3PXP",
  new_code: "123456", old_code: "654321" };

test("passkey recovery rejects aliases anywhere in the private request path", () => {
  const root = mkdtempSync(join(tmpdir(), "dc24h-recovery-path-")); chmodSync(root, 0o700);
  try {
    const real = join(root, "owner"), nested = join(real, "requests");
    mkdirSync(real, { mode: 0o700 }); mkdirSync(nested, { mode: 0o700 });
    const path = join(nested, "recovery.json");
    writeFileSync(path, JSON.stringify(recovery), { mode: 0o600 });
    assert.deepEqual(parseRecoveryRequest(path), { actor: "7", approvers: recovery.approvers });

    symlinkSync(real, join(root, "alias"));
    assert.throws(() => parseRecoveryRequest(join(root, "alias", "requests", "recovery.json")),
      /Symlink|private state path/i, "an intermediate symlink must not be trusted");
    assert.throws(() => parseRecoveryRequest(nested + "/../requests/recovery.json"),
      /Noncanonical/i, "noncanonical path aliases must not bypass inode checks");
    assert.throws(() => parseRecoveryRequest(relative(process.cwd(), path)), /Absolute|Noncanonical/i);

    symlinkSync(path, join(nested, "leaf.json"));
    assert.throws(() => parseRecoveryRequest(join(nested, "leaf.json")), /Unsafe private request/);
    linkSync(path, join(nested, "extra-link.json"));
    assert.throws(() => parseRecoveryRequest(path), /Unsafe private request/,
      "hard-linked operator requests are not private single-owner leaves");
    rmSync(join(nested, "extra-link.json"));
    chmodSync(path, 0o644);
    assert.throws(() => parseRecoveryRequest(path), /Unsafe private request/);
    chmodSync(path, 0o600);
    assert.deepEqual(readPrivateJsonRequest(path, 2048), recovery);
    assert.equal(JSON.parse(readFileSync(path, "utf8")).actor, "7");
  } finally { rmSync(root, { recursive: true, force: true }); }
});

test("TOTP offline recovery requires two distinct acknowledgments without exposing seeds", () => {
  const root = mkdtempSync(join(tmpdir(), "dc24h-totp-request-")); chmodSync(root, 0o700);
  try {
    const real = join(root, "real"), path = join(real, "totp.json");
    mkdirSync(real, { mode: 0o700 });
    writeFileSync(path, JSON.stringify(rotation), { mode: 0o600 });
    assert.deepEqual(parseTotpMaintenanceRequest(path, false), {
      actor: rotation.actor, newSeed: rotation.new_seed,
      newCode: rotation.new_code, oldCode: rotation.old_code,
    });
    assert.throws(() => parseTotpMaintenanceRequest(path, true), /Invalid maintenance request/,
      "old-device proof is a rotation, not lost-device recovery");

    const noProof = { schema: 1, actor: "7", new_seed: rotation.new_seed, new_code: rotation.new_code };
    writeFileSync(path, JSON.stringify(noProof));
    assert.throws(() => parseTotpMaintenanceRequest(path, true), /Invalid maintenance request/);
    writeFileSync(path, JSON.stringify({ ...noProof, approvers: ["one", "one"] }));
    assert.throws(() => parseTotpMaintenanceRequest(path, true), /Invalid maintenance request/);
    writeFileSync(path, JSON.stringify({ ...noProof, approvers: ["owner-one", "owner-two"] }));
    assert.deepEqual(parseTotpMaintenanceRequest(path, true), {
      actor: "7", newSeed: rotation.new_seed, newCode: rotation.new_code,
    });
    assert.throws(() => parseTotpMaintenanceRequest(path, false), /Invalid maintenance request/,
      "rotation does not accept a recovery acknowledgment in place of the previous OTP");
    symlinkSync(real, join(root, "link"));
    assert.throws(() => parseTotpMaintenanceRequest(join(root, "link", "totp.json"), true),
      /Symlink|private state path/i);
    writeFileSync(path, " ".repeat(1025));
    assert.throws(() => parseTotpMaintenanceRequest(path, true), /Unsafe private request/);
  } finally { rmSync(root, { recursive: true, force: true }); }
});
