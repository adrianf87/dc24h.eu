import assert from "node:assert/strict";
import { test } from "node:test";
import { createServer, request } from "node:http";
import { once } from "node:events";
import type { AddressInfo } from "node:net";
import { publicGateway } from "../server/public-gateway.ts";
import type { PublicGatewayDependencies } from "../server/public-gateway.ts";
import { PUBLIC_WEBADMIN_ORIGIN } from "../server/public-email-code.ts";

const grant = { actor: "7", role: "owner" as const, grant: "a".repeat(64),
  session: "b".repeat(32), csrf: "c".repeat(64) };
type Reply = { code: number; body: Record<string, unknown>; cookies: string[] };

async function fixture() {
  let at = 100_000, roleValid = true, emailHealthy = true, auditHealthy = true, mailSuccess = true;
  const mails: Array<{ email: string; code: string }> = [];
  const events: string[] = [];
  const deps = {
    backend: {
      verifyPassword: async (actor: string, proof: string) =>
        actor === "7" && proof === "synthetic-password" ? grant : null,
      reauthorize: async () => roleValid,
      summary: async () => ({ server: { running: true } }),
    },
    email: { healthy: () => emailHealthy, forActor: (actor: string) =>
      actor === "7" ? "bound@example.test" : null },
    sendCode: async (email: string, code: string) => {
      mails.push({ email, code }); return mailSuccess;
    },
    limiter: { healthy: () => auditHealthy, hit: () => false },
    auditHealthy: () => auditHealthy,
    audit: (event: string) => { events.push(event); },
    now: () => at,
  } as unknown as PublicGatewayDependencies;
  const server = createServer(publicGateway(deps));
  server.listen(0, "127.0.0.1");
  await once(server, "listening");
  const port = (server.address() as AddressInfo).port;
  const call = (path: string, data?: unknown, headers: Record<string, string> = {}): Promise<Reply> => {
    const encoded = data === undefined ? "" : JSON.stringify(data);
    return new Promise((resolve, reject) => {
      const req = request({ hostname: "127.0.0.1", port, path,
        method: data === undefined ? "GET" : "POST",
        headers: { Host: "127.0.0.1:4278", ...(data === undefined ? {} : {
          Origin: PUBLIC_WEBADMIN_ORIGIN, "Content-Type": "application/json",
          "Content-Length": String(Buffer.byteLength(encoded)),
        }), ...headers } }, res => {
        const chunks: Buffer[] = [];
        res.on("data", c => chunks.push(c));
        res.on("end", () => {
          try { resolve({ code: res.statusCode ?? 0,
            body: JSON.parse(Buffer.concat(chunks).toString()) as Record<string, unknown>,
            cookies: res.headers["set-cookie"] ?? [] }); }
          catch (error) { reject(error); }
        });
      });
      req.on("error", reject); req.end(encoded);
    });
  };
  const password = async () => {
    const initial = await call("/stage4/v1/challenge");
    assert.equal(initial.code, 200);
    return call("/stage4/v1/login/send", { actor_id: "7", password: "synthetic-password" }, {
      Cookie: initial.cookies[0]!.split(";")[0]!,
      "X-DC24H-Stage4-Csrf": initial.body.csrf as string,
    });
  };
  return { call, password, mails, events,
    revokeRole: () => { roleValid = false; },
    stopEmailCatalog: () => { emailHealthy = false; },
    stopAudit: () => { auditHealthy = false; },
    stopMail: () => { mailSuccess = false; },
    time: (value: number) => { at = value; },
    close: () => new Promise<void>((resolve, reject) => server.close(e => e ? reject(e) : resolve())),
  };
}

const pendingHeaders = (pending: Reply) => ({
  Cookie: pending.cookies.find(c => c.startsWith("__Host-dc24h_stage4_email_pending="))!.split(";")[0]!,
  "X-DC24H-Stage4-Csrf": pending.body.csrf as string,
});
const sessionHeaders = (session: Reply) => ({
  Cookie: session.cookies.find(c => c.startsWith("__Host-dc24h_stage4_session="))!.split(";")[0]!,
  "X-DC24H-Stage4-Csrf": session.body.csrf as string,
});
async function login(f: Awaited<ReturnType<typeof fixture>>) {
  const sent = await f.password(); assert.equal(sent.code, 200);
  const verified = await f.call("/stage4/v1/login/verify", { code: f.mails.at(-1)!.code }, pendingHeaders(sent));
  assert.equal(verified.code, 200);
  return { sent, verified, headers: sessionHeaders(verified) };
}

test("Stage IV exact routes, public Origin and private proxy Host reject wrong requests", async () => {
  const f = await fixture();
  try {
    for (const path of ["/remote/v1/login", "/stage4/v1/login/options", "/admin-api/v1/status",
      "/stage4/v1/devices", "/stage4/v1/challenge?x=1", "/stage4/v1/%63hallenge",
      "/stage4/v1/../session", "/stage4/v1/challenge/"]) {
      assert.equal((await f.call(path)).code, 404, path);
    }
    for (const headers of ([{ Host: "server-747585786.dc24h.eu:7278" },
      { Origin: "https://server-747585786.dc24h.eu:1512" },
      { "Sec-Fetch-Site": "same-site" }, { "X-Forwarded-User": "owner" },
      { Authorization: "Bearer forged" }] as Record<string, string>[])) {
      assert.equal((await f.call("/stage4/v1/challenge", undefined, headers)).code, 403);
    }
    assert.equal(f.mails.length, 0);
  } finally { await f.close(); }
});

test("password sends code to bound mailbox; no session before the code succeeds", async () => {
  const f = await fixture();
  try {
    assert.equal((await f.call("/stage4/v1/session")).code, 401);
    const sent = await f.password();
    assert.equal(sent.code, 200);
    assert.deepEqual(f.mails.map(m => m.email), ["bound@example.test"]);
    assert.match(f.mails[0]!.code, /^[0-9]{8}$/);
    assert.equal(JSON.stringify(sent.body).includes(f.mails[0]!.code), false);
    assert.equal((await f.call("/stage4/v1/session")).code, 401);
    const headers = pendingHeaders(sent);
    assert.equal((await f.call("/stage4/v1/login/verify", { code: f.mails[0]!.code }, {
      Cookie: headers.Cookie,
    })).code, 403);
    const wrong = f.mails[0]!.code === "00000000" ? "11111111" : "00000000";
    assert.equal((await f.call("/stage4/v1/login/verify", { code: wrong }, headers)).code, 403);
    const verified = await f.call("/stage4/v1/login/verify", { code: f.mails[0]!.code }, headers);
    assert.equal(verified.code, 200);
    const cookie = verified.cookies.find(c => c.startsWith("__Host-dc24h_stage4_session="));
    assert.ok(cookie?.includes("Secure") && cookie.includes("HttpOnly") &&
      cookie.includes("SameSite=Strict") && !cookie.includes("Domain="));
    assert.equal((await f.call("/stage4/v1/session", undefined, sessionHeaders(verified))).code, 200);
    assert.equal((await f.call("/stage4/v1/login/verify", { code: f.mails[0]!.code }, headers)).code, 403);
  } finally { await f.close(); }
});

test("role withdrawal, email roster loss, CSRF and logout invalidate sessions", async () => {
  const f = await fixture();
  try {
    const s = await login(f);
    assert.equal((await f.call("/stage4/v1/logout", {}, { Cookie: s.headers.Cookie })).code, 403);
    assert.equal((await f.call("/stage4/v1/logout", {}, s.headers)).code, 200);
    assert.equal((await f.call("/stage4/v1/session", undefined, s.headers)).code, 401);
    const x = await login(f);
    f.revokeRole();
    assert.equal((await f.call("/stage4/v1/session", undefined, x.headers)).code, 403);
    assert.equal((await f.call("/stage4/v1/session", undefined, x.headers)).code, 401);
    f.stopEmailCatalog();
    assert.equal((await f.call("/stage4/v1/challenge")).code, 503);
  } finally { await f.close(); }
});

test("failed delivery and unavailable audit fail closed", async () => {
  const f = await fixture();
  try {
    f.stopMail();
    assert.equal((await f.password()).code, 403);
    assert.equal((await f.call("/stage4/v1/session")).code, 401);
    f.stopAudit();
    assert.equal((await f.call("/stage4/v1/challenge")).code, 503);
  } finally { await f.close(); }
});

test("email verification code expires and cannot be replayed across login attempts", async () => {
  const f = await fixture();
  try {
    const sent = await f.password(); assert.equal(sent.code, 200);
    f.time(400_000);
    assert.equal((await f.call("/stage4/v1/login/verify", { code: f.mails[0]!.code }, pendingHeaders(sent))).code, 403);
    assert.equal((await f.call("/stage4/v1/session")).code, 401);
  } finally { await f.close(); }
});
