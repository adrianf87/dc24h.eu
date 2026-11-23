import assert from "node:assert/strict";
import { test } from "node:test";
import { chmodSync, mkdtempSync, readFileSync, rmSync, symlinkSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { openRemoteRateLimitStore } from "../server/remote-rate-limit-store.ts";

const secret = "a".repeat(64);
test("durable hashed budgets survive restart, expire only after the window, and never record identities", () => {
  const dir = mkdtempSync(join(tmpdir(), "dc24h-rate-")); chmodSync(dir, 0o700);
  const path = join(dir, "limits.json");
  try {
    const first = openRemoteRateLimitStore(path, secret);
    for (let n = 0; n < 4; n++) assert.equal(first.hit("login:12345", 4, 100_000), false);
    const text = readFileSync(path, "utf8");
    assert.ok(!text.includes("12345") && !text.includes("login:") && !text.includes(secret));
    const restarted = openRemoteRateLimitStore(path, secret);
    assert.equal(restarted.hit("login:12345", 4, 100_001), true, "restart cannot reset quota");
    assert.equal(restarted.hit("login:12345", 4, 99_999), true, "backward clock cannot reset quota");
    assert.equal(restarted.hit("login:12345", 4, 160_001), false, "window eventually expires");
    assert.equal(restarted.healthy(), true);
  } finally { rmSync(dir, { recursive: true, force: true }); }
});

test("unsafe files, concurrent writers, corruption and outside replacement fail closed", () => {
  const dir = mkdtempSync(join(tmpdir(), "dc24h-rate-sec-")); chmodSync(dir, 0o700);
  const path = join(dir, "limits.json"), alias = join(dir, "alias.json");
  try {
    const first = openRemoteRateLimitStore(path, secret);
    const second = openRemoteRateLimitStore(path, secret);
    first.hit("login:7", 4, 10);
    assert.equal(second.healthy(), false, "another writer may not spend from stale state");
    assert.throws(() => second.hit("login:7", 4, 10), /unavailable/);
    chmodSync(path, 0o644);
    assert.throws(() => openRemoteRateLimitStore(path, secret), /Unsafe/);
    chmodSync(path, 0o600);
    writeFileSync(alias, "{\"schema\":1,\"buckets\":[]}", { mode: 0o600 });
    rmSync(path);
    symlinkSync(alias, path);
    assert.throws(() => openRemoteRateLimitStore(path, secret));
    rmSync(path);
    writeFileSync(path, '{"schema":1,"buckets":[{"key":"bad"}]}', { mode: 0o600 });
    assert.throws(() => openRemoteRateLimitStore(path, secret), /Invalid rate-limit catalog/);
  } finally { rmSync(dir, { recursive: true, force: true }); }
});
