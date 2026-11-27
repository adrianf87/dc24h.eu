import assert from "node:assert/strict";
import { test } from "node:test";
import { createPublicEmailCodeGate } from "../server/public-email-code.ts";
import { isSafeMailbox, createLoginMessage } from "../server/public-msmtp.ts";

function fixture() {
  let at = 10_000, roleValid = true, mailbox: string | null = "verified@example.test", delivered = true;
  const sent: Array<{ email: string; code: string }> = [];
  let sessions = 0;
  const gate = createPublicEmailCodeGate({
    verifyPassword: async (actor: string, password: string) => actor === "7" && password === "synthetic-good" ?
      { actor, role: "owner" as const } : null,
    verifiedEmail: () => mailbox,
    sendCode: async (email: string, code: string) => { sent.push({ email, code }); return delivered; },
    reauthorize: async () => roleValid,
    issueSession: async () => { sessions++; return "valid-session"; },
    now: () => at,
  });
  return { gate, sent, sessionCount: () => sessions,
    time: (value: number) => { at = value; },
    revoke: () => { roleValid = false; },
    setMailbox: (value: string | null) => { mailbox = value; },
    sendFails: () => { delivered = false; },
  };
}

test("password alone and absent verified email never send a code or create a session", async () => {
  const f = fixture();
  assert.equal(await f.gate.begin("7", "wrong"), null);
  f.setMailbox(null);
  assert.equal(await f.gate.begin("7", "synthetic-good"), null);
  assert.equal(f.sent.length, 0);
  assert.equal(f.sessionCount(), 0);
});

test("password proof sends an eight-digit code to the stored verified recipient only", async () => {
  const f = fixture();
  const started = await f.gate.begin("7", "synthetic-good");
  assert.ok(started);
  assert.deepEqual(f.sent.map(m => m.email), ["verified@example.test"]);
  assert.match(f.sent[0]!.code, /^[0-9]{8}$/);
  assert.equal(started.expires_in, 300);
  assert.equal(f.sessionCount(), 0);
  assert.equal(await f.gate.finish(started.ticket, f.sent[0]!.code), "valid-session");
  assert.equal(f.sessionCount(), 1);
  assert.equal(await f.gate.finish(started.ticket, f.sent[0]!.code), null);
});

test("wrong codes, malformed codes, five guesses and expiry fail closed", async () => {
  const f = fixture();
  const attempt = await f.gate.begin("7", "synthetic-good");
  assert.ok(attempt);
  for (let i = 0; i < 5; i++) {
    const wrong = f.sent[0]!.code === "00000000" ? "11111111" : "00000000";
    assert.equal(await f.gate.finish(attempt.ticket, wrong), null);
  }
  assert.equal(await f.gate.finish(attempt.ticket, f.sent[0]!.code), null);
  const second = await f.gate.begin("7", "synthetic-good");
  assert.ok(second);
  f.time(310_000);
  assert.equal(await f.gate.finish(second.ticket, f.sent[1]!.code), null);
  assert.equal(f.sessionCount(), 0);
});

test("current role/grant revocation and concurrent replay cannot create extra sessions", async () => {
  const f = fixture();
  const first = await f.gate.begin("7", "synthetic-good");
  assert.ok(first);
  f.revoke();
  assert.equal(await f.gate.finish(first.ticket, f.sent[0]!.code), null);
  assert.equal(f.sessionCount(), 0);
  const g = fixture();
  const second = await g.gate.begin("7", "synthetic-good");
  assert.ok(second);
  const results = await Promise.all([
    g.gate.finish(second.ticket, g.sent[0]!.code),
    g.gate.finish(second.ticket, g.sent[0]!.code),
  ]);
  assert.deepEqual(results.sort(), [null, "valid-session"].sort());
  assert.equal(g.sessionCount(), 1);
});

test("mailer failure, unsafe mailbox and resource limits do not authorize", async () => {
  const f = fixture();
  f.sendFails();
  assert.equal(await f.gate.begin("7", "synthetic-good"), null);
  assert.equal(f.sessionCount(), 0);
  assert.equal(f.gate.pendingCount(), 0);
  for (const bad of ["victim@example.test\nBcc:attacker@example.test", "bad@", "x..y@example.test"]) {
    assert.equal(isSafeMailbox(bad), false);
  }
  assert.equal(isSafeMailbox("person@example.test"), true);
  const message = createLoginMessage("person@example.test", "12345678");
  assert.match(message, /^From: admin@dc24h\.eu\nTo: person@example\.test\nSubject:/);
  assert.match(message, /Your dc24h\.eu WebAdmin sign-in code is: 12345678/);
  assert.throws(() => createLoginMessage("person@example.test\nBcc:evil@example.test", "12345678"));
  assert.throws(() => createPublicEmailCodeGate({
    verifyPassword: async () => null, verifiedEmail: () => null,
    sendCode: async () => false, reauthorize: async () => false,
    issueSession: async () => null, ttlMs: 600_000,
  }));
});
