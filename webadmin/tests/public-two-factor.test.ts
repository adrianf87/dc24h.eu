import assert from "node:assert/strict";
import { test } from "node:test";
import {
  createPublicPasswordPasskeyGate, requirePublicWebAuthnIdentity,
  PUBLIC_WEBADMIN_ORIGIN, PUBLIC_WEBADMIN_PORT, PUBLIC_WEBADMIN_RP_ID,
} from "../server/public-two-factor.ts";

type Assertion = { approved: boolean; actor?: string };
const challenge = "synthetic-webauthn-challenge-0000000001";

function fixture() {
  let time = 1_000;
  let allowed = true;
  let passwordChecks = 0;
  let passkeyChecks = 0;
  let sessionCount = 0;
  const gate = createPublicPasswordPasskeyGate({
    verifyPassword: async (actor: string, password: string) => {
      passwordChecks += 1;
      return password === "synthetic-valid-password"
        ? { actor, role: "owner" as const } : null;
    },
    passkeys: {
      beginLogin: async () => ({ challenge }),
      finishLogin: async (actor: string, received: string, assertion: Assertion) => {
        passkeyChecks += 1;
        return assertion.approved && received === challenge && (!assertion.actor || assertion.actor === actor);
      },
    },
    reauthorize: async () => allowed,
    issueSession: async (grant) => {
      sessionCount += 1;
      return "session-" + grant.actor;
    },
    now: () => time,
  });
  return {
    gate, setTime: (value: number) => { time = value; },
    setAllowed: (value: boolean) => { allowed = value; },
    counts: () => ({ passwordChecks, passkeyChecks, sessionCount }),
  };
}

test("Stage IV pins RP ID to the hostname and Origin to HTTPS port 7278", () => {
  assert.equal(PUBLIC_WEBADMIN_PORT, 7278);
  assert.deepEqual(requirePublicWebAuthnIdentity(PUBLIC_WEBADMIN_ORIGIN, PUBLIC_WEBADMIN_RP_ID),
    { origin: "https://server-747585786.dc24h.eu:7278",
      rpID: "server-747585786.dc24h.eu" });
  for (const [origin, rpID] of [
    ["https://server-747585786.dc24h.eu", PUBLIC_WEBADMIN_RP_ID],
    ["https://server-747585786.dc24h.eu:1512", PUBLIC_WEBADMIN_RP_ID],
    ["http://server-747585786.dc24h.eu:7278", PUBLIC_WEBADMIN_RP_ID],
    ["https://62.171.156.32:7278", PUBLIC_WEBADMIN_RP_ID],
    [PUBLIC_WEBADMIN_ORIGIN, "dc24h.eu"],
    [PUBLIC_WEBADMIN_ORIGIN, "server-747585786.dc24h.eu:7278"],
  ]) assert.throws(() => requirePublicWebAuthnIdentity(origin, rpID));
});

test("wrong password, missing password and invalid actor never start a passkey gate", async () => {
  const f = fixture();
  assert.equal(await f.gate.begin("7", "wrong"), null);
  assert.equal(await f.gate.begin("7", ""), null);
  assert.equal(await f.gate.begin("invalid", "synthetic-valid-password"), null);
  assert.equal(f.gate.pendingCount(), 0);
  assert.equal(f.counts().sessionCount, 0);
  assert.equal(f.counts().passkeyChecks, 0);
});

test("password alone does not create a session; verified passkey is required", async () => {
  const f = fixture();
  const started = await f.gate.begin("7", "synthetic-valid-password");
  assert.ok(started);
  assert.equal(started.options.challenge, challenge);
  assert.equal(f.counts().sessionCount, 0);
  assert.equal(await f.gate.finish(started.ticket, { approved: false }), null);
  assert.equal(f.counts().sessionCount, 0);
  assert.equal(f.gate.pendingCount(), 0);
});

test("two distinct factors and live reauthorization issue one session exactly once", async () => {
  const f = fixture();
  const started = await f.gate.begin("7", "synthetic-valid-password");
  assert.ok(started);
  assert.equal(await f.gate.finish(started.ticket, { approved: true, actor: "7" }), "session-7");
  assert.equal(await f.gate.finish(started.ticket, { approved: true, actor: "7" }), null);
  assert.deepEqual(f.counts(), { passwordChecks: 1, passkeyChecks: 1, sessionCount: 1 });
});

test("replayed, expired, cancelled and cross-account ceremonies fail closed", async () => {
  const f = fixture();
  const a = await f.gate.begin("7", "synthetic-valid-password");
  assert.ok(a);
  assert.equal(await f.gate.finish(a.ticket, { approved: true, actor: "8" }), null);
  const b = await f.gate.begin("7", "synthetic-valid-password");
  assert.ok(b);
  f.setTime(61_001);
  assert.equal(await f.gate.finish(b.ticket, { approved: true, actor: "7" }), null);
  const c = await f.gate.begin("7", "synthetic-valid-password");
  assert.ok(c);
  f.gate.cancel(c.ticket);
  assert.equal(await f.gate.finish(c.ticket, { approved: true, actor: "7" }), null);
  assert.equal(f.counts().sessionCount, 0);
});

test("role or grant revocation between factors rejects session issuance", async () => {
  const f = fixture();
  const started = await f.gate.begin("7", "synthetic-valid-password");
  assert.ok(started);
  f.setAllowed(false);
  assert.equal(await f.gate.finish(started.ticket, { approved: true }), null);
  assert.equal(f.counts().sessionCount, 0);
});

test("concurrent finish calls cannot reuse the same single-use ticket", async () => {
  const f = fixture();
  const started = await f.gate.begin("7", "synthetic-valid-password");
  assert.ok(started);
  const results = await Promise.all([
    f.gate.finish(started.ticket, { approved: true }),
    f.gate.finish(started.ticket, { approved: true }),
  ]);
  assert.deepEqual(results.sort(), [null, "session-7"].sort());
  assert.equal(f.counts().sessionCount, 1);
});

test("unsafe TTL or resource limits are rejected before use", () => {
  const f = fixture();
  assert.equal(f.gate.pendingCount(), 0);
  assert.throws(() => createPublicPasswordPasskeyGate({
    verifyPassword: async () => null,
    passkeys: { beginLogin: async () => null, finishLogin: async () => false },
    reauthorize: async () => false,
    issueSession: async () => null,
    ttlMs: 120_000,
  }));
});
