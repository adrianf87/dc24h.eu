import assert from "node:assert/strict";
import { test } from "node:test";
import { mkdtempSync, readFileSync, rmSync, chmodSync, writeFileSync, statSync, utimesSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { initializePasskeyStore, openPasskeyStore } from "../server/remote-passkeys.ts";
import { passkeyCeremonies, relyingParty } from "../server/remote-webauthn.ts";

function withStore(fn: (path: string) => void) {
  const dir = mkdtempSync(join(tmpdir(), "dc24h-passkey-"));
  chmodSync(dir, 0o700);
  try {
    const path = join(dir, "passkeys.json");
    initializePasskeyStore(path, "localhost");
    fn(path);
  }
  finally { rmSync(dir, { recursive: true, force: true }); }
}
const sample = (id: string, actor = "7") => ({
  id, actor, userHandle: "a".repeat(43), publicKey: "b".repeat(60),
  counter: 0, deviceType: "singleDevice" as const, backedUp: false,
  transports: ["internal"], name: "Passkey", createdAt: 1000,
  lastUsedAt: null, revokedAt: null,
});

test("deleted passkey catalog refuses restart and first-install init cannot reset revoked keys", () => {
  const dir = mkdtempSync(join(tmpdir(), "dc24h-passkey-missing-")); chmodSync(dir, 0o700);
  const path = join(dir, "passkeys.json");
  try {
    assert.throws(() => openPasskeyStore(path, "localhost"), /ENOENT/);
    initializePasskeyStore(path, "localhost");
    const store = openPasskeyStore(path, "localhost");
    const handle = store.userHandle("7"), id = "c".repeat(32);
    store.add({ ...sample(id), userHandle: handle });
    store.revoke("7", id, 2000);
    const old = readFileSync(path, "utf8");
    assert.throws(() => initializePasskeyStore(path, "localhost"), /EEXIST/);
    assert.equal(readFileSync(path, "utf8"), old);
    rmSync(path);
    assert.throws(() => openPasskeyStore(path, "localhost"), /ENOENT/);
  } finally { rmSync(dir, { recursive: true, force: true }); }
});

test("private file catalogs are bounded, reusable after restart, and require an exact RP ID", () => withStore(path => {
  const first = openPasskeyStore(path, "localhost");
  const handle = first.userHandle("7");
  assert.match(handle, /^[A-Za-z0-9_-]{43}$/);
  first.add({ ...sample("c".repeat(32)), userHandle: handle });
  assert.equal(first.forActor("7").length, 1);
  const restarted = openPasskeyStore(path, "localhost");
  assert.equal(restarted.userHandle("7"), handle);
  assert.equal(restarted.get("8", "c".repeat(32)), undefined);
  assert.throws(() => openPasskeyStore(path, "example.org"), /Invalid passkey catalog/);
  assert.throws(() => first.add({ ...sample("c".repeat(32)), userHandle: handle }), /duplicate/);
  assert.ok(!readFileSync(path, "utf8").includes("privateKey"));
}));

test("revocation invalidates the actor epoch and permanently rejects revoked keys", () => withStore(path => {
  const store = openPasskeyStore(path, "localhost");
  const id = "f".repeat(32), handle = store.userHandle("7");
  store.add({ ...sample(id), userHandle: handle });
  assert.equal(store.markUsed("7", id, 0, 1, "singleDevice", false, 2000), true);
  assert.equal(store.markUsed("7", id, 0, 2, "singleDevice", false, 2001), false);
  assert.equal(store.markUsed("7", id, 1, 1, "singleDevice", false, 2001), false);
  assert.equal(store.markUsed("7", id, 1, 2, "singleDevice", false, 2002), true);
  assert.equal(store.rename("8", id, "Stolen"), false);
  assert.equal(store.rename("7", id, "<script>"), false);
  assert.equal(store.rename("7", id, " My laptop "), true);
  assert.equal(store.get("7", id)?.name, "My laptop");
  assert.equal(store.epoch("7"), 0);
  assert.equal(store.revoke("7", id, 3000), true);
  assert.equal(store.epoch("7"), 1);
  assert.equal(store.forActor("7").length, 0);
  assert.equal(store.markUsed("7", id, 2, 3, "singleDevice", false, 3001), false);
  assert.equal(store.revoke("7", id, 3001), false);
  assert.equal(openPasskeyStore(path, "localhost").epoch("7"), 1);
  assert.equal(store.revokeAll("7", 3002), 0);
}));

test("two passkeys preserve counter and backup policy, account isolation, rename and actor-wide revoke across restart", () => withStore(path => {
  const store = openPasskeyStore(path, "localhost");
  const first = "a".repeat(32), second = "b".repeat(32), foreign = "c".repeat(32);
  const owner = store.userHandle("7"), outsider = store.userHandle("8");
  store.add({ ...sample(first), userHandle: owner });
  store.add({ ...sample(second), userHandle: owner, deviceType: "multiDevice", backedUp: true });
  store.add({ ...sample(foreign, "8"), userHandle: outsider });
  assert.deepEqual(store.forActor("7").map(c => c.id), [first, second]);
  assert.equal(store.get("8", first), undefined, "another actor never owns the first key");
  assert.equal(store.markUsed("7", first, 0, 1, "singleDevice", false, 1000), true);
  assert.equal(store.markUsed("7", first, 0, 2, "singleDevice", false, 1001), false, "counter rollback");
  assert.equal(store.markUsed("7", first, 1, 2, "multiDevice", true, 1001), false, "unexpected backup downgrade");
  assert.equal(store.markUsed("7", second, 0, 0, "multiDevice", true, 1002), true,
    "synced multi-device keys may have a zero hardware counter");
  assert.equal(store.markUsed("7", second, 0, 0, "singleDevice", false, 1003), false,
    "backup eligibility must not downgrade after a multi-device credential was enrolled");
  assert.equal(store.get("7", second)?.deviceType, "multiDevice");
  assert.equal(store.rename("8", second, "Interloper"), false);
  assert.equal(store.rename("7", second, "Travel key"), true);
  assert.equal(store.revoke("7", first, 2000), true);
  assert.equal(store.get("7", first), undefined);
  assert.equal(store.markUsed("7", second, 0, 0, "multiDevice", true, 2001), true,
    "the remaining key is usable after one-key revoke");
  assert.equal(store.epoch("7"), 1, "all existing actor sessions must invalidate");
  assert.equal(store.epoch("8"), 0);
  assert.equal(store.revokeAll("7", 3000), 1);
  assert.equal(store.forActor("7").length, 0);
  assert.equal(store.forActor("8").length, 1, "foreign actor's credential survives");
  const recovered = openPasskeyStore(path, "localhost");
  assert.equal(recovered.epoch("7"), 2);
  assert.equal(recovered.get("7", second), undefined);
  assert.equal(recovered.markUsed("7", second, 0, 1, "multiDevice", true, 3001), false);
  assert.equal(recovered.get("8", foreign)?.id, foreign);
}));

test("external rewrite, unsafe permissions, corrupt data or another writer fail closed", () => withStore(path => {
  const store = openPasskeyStore(path, "localhost");
  store.userHandle("7");
  const competing = openPasskeyStore(path, "localhost");
  competing.userHandle("8");
  assert.equal(store.healthy(), false, "other process must not reuse stale cached credentials");
  assert.throws(() => store.forActor("7"), /unavailable/);
  chmodSync(path, 0o644);
  assert.throws(() => openPasskeyStore(path, "localhost"), /Unsafe/);
  chmodSync(path, 0o600);
  writeFileSync(path, "{\"schema\":1}");
  assert.throws(() => openPasskeyStore(path, "localhost"), /Invalid passkey catalog/);
}));

test("same-length counter rollback with the previous mtime fails closed, even if JSON remains valid", () => withStore(path => {
  const initial = openPasskeyStore(path, "localhost");
  const handle = initial.userHandle("7"), id = "f".repeat(32);
  initial.add({ ...sample(id), userHandle: handle });
  assert.equal(initial.markUsed("7", id, 0, 5, "singleDevice", false, 2000), true);
  const fixed = new Date("2026-01-01T00:00:00Z");
  utimesSync(path, fixed, fixed);
  const live = openPasskeyStore(path, "localhost");
  const before = readFileSync(path, "utf8");
  const modified = JSON.parse(before);
  modified.credentials[0].counter = 0;
  const after = JSON.stringify(modified) + "\n";
  assert.equal(Buffer.byteLength(after), Buffer.byteLength(before), "same-length corruption fixture");
  writeFileSync(path, after);
  utimesSync(path, fixed, fixed);
  assert.equal(statSync(path).mtimeMs, fixed.getTime(), "mtime was restored after the in-place write");
  assert.equal(live.healthy(), false, "content changed despite unchanged inode/size/mtime");
  assert.throws(() => live.markUsed("7", id, 5, 6, "singleDevice", false, 3000), /unavailable/);
}));

test("noncanonical passkey identifiers, unexpected fields and duplicate transports fail closed", () => {
  for (const mutate of [
    (data: any) => { data.credentials[0].id = "A".repeat(42) + "B"; },
    (data: any) => { data.credentials[0].publicKey = "A".repeat(42) + "B"; },
    (data: any) => { data.users[0].handle = "A".repeat(42) + "B"; },
    (data: any) => { data.credentials[0].transports = ["internal", "internal"]; },
    (data: any) => { data.credentials[0].privateKey = "should-never-exist"; },
    (data: any) => { data.users[0].admin = true; },
    (data: any) => { data.unreviewed = true; },
  ]) withStore(path => {
    const store = openPasskeyStore(path, "localhost");
    const handle = store.userHandle("7");
    store.add({ ...sample("c".repeat(32)), userHandle: handle });
    const data = JSON.parse(readFileSync(path, "utf8"));
    mutate(data);
    writeFileSync(path, JSON.stringify(data) + "\n", { mode: 0o600 });
    assert.throws(() => openPasskeyStore(path, "localhost"), /Invalid passkey catalog/);
  });
});

test("WebAuthn RP configuration does not trust browser Host or forwarded origin", () => {
  assert.deepEqual(relyingParty("https://localhost:8443", "localhost"),
    { origin: "https://localhost:8443", rpID: "localhost" });
  for (const [origin, rpID] of [
    ["http://localhost:8443", "localhost"], ["https://localhost:8443", "evil.test"],
    ["https://localhost:8443/path", "localhost"], ["https://u:p@localhost:8443", "localhost"],
  ]) assert.throws(() => relyingParty(origin!, rpID!), /RP ID/);
  withStore(path => {
    const ceremonies = passkeyCeremonies(openPasskeyStore(path, "localhost"), "https://localhost:8443", "localhost");
    assert.equal(typeof ceremonies.beginEnrollment, "function");
  });
});
