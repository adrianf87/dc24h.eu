import assert from "node:assert/strict";
import { test } from "node:test";
import { chmodSync, existsSync, mkdtempSync, readdirSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { enrollFirstVerifiedEmail } from "../server/public-email-enrollment.ts";
import { openVerifiedEmailCatalog } from "../server/public-verified-email.ts";

async function fixture(callback: (args: {
  path: string; run: (options?: {
    password?: string; role?: "administrator" | "owner" | "registered";
    accepted?: boolean; delivery?: boolean; stale?: boolean; revoke?: boolean;
  }) => Promise<boolean>;
  readMail: () => { recipient: string; code: string } | null;
  dir: string;
}) => Promise<void>) {
  const dir = mkdtempSync(join(tmpdir(), "dc24h-stage4-enroll-"));
  chmodSync(dir, 0o700);
  const path = join(dir, "verified-emails.json");
  let mail: { recipient: string; code: string } | null = null;
  let at = 1000;
  const run = (opts: { password?: string; role?: "administrator" | "owner" | "registered";
    accepted?: boolean; delivery?: boolean; stale?: boolean; revoke?: boolean } = {}) =>
    enrollFirstVerifiedEmail(path, "7", "admin@dc24h.eu", opts.password ?? "valid-password", {
      verifyPassword: async (actor, password) => opts.accepted === false || password !== "valid-password" ? null :
        { actor, role: (opts.role ?? "administrator") as "administrator" },
      reauthorize: async () => !opts.revoke,
      sendCode: async (recipient, code) => {
        mail = { recipient, code };
        return opts.delivery !== false;
      },
      readCode: async () => {
        if (opts.stale) at += 300_000;
        return mail!.code;
      },
      now: () => at,
    });
  try { await callback({ path, run, readMail: () => mail, dir }); }
  finally { rmSync(dir, { recursive: true, force: true }); }
}

test("Administrator mailbox requires C++ password, email delivery and OTP proof", async () => {
  await fixture(async ({ path, run, readMail, dir }) => {
    assert.equal(await run({ password: "wrong" }), false);
    assert.equal(readMail(), null);
    assert.equal(await run({ role: "registered" }), false);
    assert.equal(await run({ delivery: false }), false);
    assert.equal(existsSync(path), false);
    assert.equal(await run(), true);
    assert.equal(readMail()?.recipient, "admin@dc24h.eu");
    assert.match(readMail()!.code, /^[0-9]{8}$/);
    assert.equal(openVerifiedEmailCatalog(path).forActor("7"), "admin@dc24h.eu");
    assert.equal(openVerifiedEmailCatalog(path).forActor("8"), null);
    assert.deepEqual(readdirSync(dir), ["verified-emails.json"]);
  });
});

test("an existing catalog cannot be replaced by email re-enrollment", async () => {
  await fixture(async ({ path, run, readMail }) => {
    writeFileSync(path, '{"schema":1,"accounts":[]}\n', { mode: 0o600 });
    assert.equal(await run(), false);
    assert.equal(readMail(), null);
  });
});

test("expired OTP, role revocation and private permissions fail without a binding", async () => {
  for (const opts of [{ stale: true }, { revoke: true }])
    await fixture(async ({ path, run }) => {
      assert.equal(await run(opts), false);
      assert.equal(existsSync(path), false);
    });
  await fixture(async ({ path, run, dir }) => {
    chmodSync(dir, 0o755);
    await assert.rejects(run());
    assert.equal(existsSync(path), false);
  });
});

test("invalid actor, invalid email and wrong OTP are rejected with no catalog", async () => {
  await fixture(async ({ path, dir }) => {
    const fake = {
      verifyPassword: async (actor: string) => ({ actor, role: "administrator" as const }),
      reauthorize: async () => true,
      sendCode: async () => true,
      readCode: async () => "wrong-code",
    };
    for (const [actor, email] of [["0", "admin@dc24h.eu"],
      ["7", "other@example.test\nBcc:bad@example.test"]]) {
      assert.equal(await enrollFirstVerifiedEmail(path, actor!, email!, "valid-password", fake), false);
    }
    // A guessed code must not create the private roster.
    assert.equal(await enrollFirstVerifiedEmail(path, "7", "admin@dc24h.eu", "valid-password", fake), false);
    assert.equal(existsSync(path), false);
    assert.deepEqual(readdirSync(dir), []);
  });
});
