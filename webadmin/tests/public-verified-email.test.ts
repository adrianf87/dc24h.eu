import assert from "node:assert/strict";
import { test } from "node:test";
import { chmodSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { openVerifiedEmailCatalog } from "../server/public-verified-email.ts";

function catalog(content: unknown, fn: (path: string) => void) {
  const directory = mkdtempSync(join(tmpdir(), "dc24h-email-binding-"));
  chmodSync(directory, 0o700);
  const path = join(directory, "verified-emails.json");
  try {
    writeFileSync(path, JSON.stringify(content) + "\n", { mode: 0o600 });
    fn(path);
  } finally { rmSync(directory, { recursive: true, force: true }); }
}

test("only the registered verified account mailbox can receive login codes", () => {
  catalog({ schema: 1, accounts: [{ actor: "7", email: "owner@example.test", verified: true }] }, path => {
    const roster = openVerifiedEmailCatalog(path);
    assert.equal(roster.forActor("7"), "owner@example.test");
    assert.equal(roster.forActor("8"), null);
    assert.equal(roster.healthy(), true);
  });
});

test("unverified, duplicated, unsafe or header-injecting email records are rejected", () => {
  for (const accounts of [
    [{ actor: "7", email: "owner@example.test", verified: false }],
    [{ actor: "7", email: "owner@example.test", verified: true },
      { actor: "8", email: "owner@example.test", verified: true }],
    [{ actor: "7", email: "owner@example.test\nBcc:attacker@example.test", verified: true }],
    [{ actor: "7", email: "owner@example.test", verified: true, code: "12345678" }],
  ]) catalog({ schema: 1, accounts }, path => assert.throws(() => openVerifiedEmailCatalog(path)));
});

test("an externally changed or public-readable catalog fails closed", () => {
  catalog({ schema: 1, accounts: [{ actor: "7", email: "owner@example.test", verified: true }] }, path => {
    const roster = openVerifiedEmailCatalog(path);
    writeFileSync(path, '{"schema":1,"accounts":[]}\n');
    assert.equal(roster.healthy(), false);
    assert.throws(() => roster.forActor("7"));
    chmodSync(path, 0o644);
    assert.throws(() => openVerifiedEmailCatalog(path));
  });
});
