import assert from "node:assert/strict";
import { test } from "node:test";
import { createServer, request } from "node:http";
import { once } from "node:events";
import type { AddressInfo } from "node:net";
import { publicGateway } from "../server/public-gateway.ts";
import type { PublicGatewayDependencies } from "../server/public-gateway.ts";
import { PUBLIC_WEBADMIN_ORIGIN } from "../server/public-two-factor.ts";

const validGrant = { actor: "7", role: "owner" as const, grant: "a".repeat(64),
  session: "b".repeat(32), csrf: "c".repeat(64) };
const candidate = { id: "some-credential", response: { approved: true }, type: "public-key" };
type Reply = { code: number; body: Record<string, unknown>;
  setCookie: string[] };

async function fixture() {
  let at = 100_000, allowed = true, approved = true, healthy = true, epoch = 0;
  let passkeyChecks = 0, passwordChecks = 0;
  const events: string[] = [];
  const dependencies = {
    backend: {
      verifyPassword: async (actor: string, password: string) => {
        passwordChecks++;
        return actor === "7" && password === "valid-synthetic-password" ? validGrant : null;
      },
      reauthorize: async () => allowed,
      summary: async () => ({ server: { running: true } }),
    },
    passkeys: {
      store: {
        healthy: () => true,
        epoch: () => epoch,
        forActor: () => [{ id: "some-credential", name: "Test Key", deviceType: "singleDevice",
          backedUp: false, createdAt: 123, lastUsedAt: null }],
        revoke: (_actor: string, id: string) => { if (id !== "some-credential") return false; epoch++; return true; },
      },
      ceremonies: {
        beginLogin: async () => ({ challenge: "synthetic-webauthn-challenge-0123456789" }),
        finishLogin: async (_actor: string, _challenge: string, response: typeof candidate) => {
          passkeyChecks++; return approved && response.response.approved;
        },
        beginEnrollment: async () => ({ challenge: "synthetic-enrollment-challenge" }),
        finishEnrollment: async () => true,
      },
    },
    limiter: { healthy: () => healthy, hit: () => false },
    auditHealthy: () => healthy,
    audit: (event: string) => { events.push(event); },
    now: () => at,
  } as unknown as PublicGatewayDependencies;
  const server = createServer(publicGateway(dependencies));
  server.listen(0, "127.0.0.1");
  await once(server, "listening");
  const port = (server.address() as AddressInfo).port;

  const call = async (path: string, data?: unknown, additional: Record<string, string> = {}): Promise<Reply> => {
    const body = data === undefined ? "" : JSON.stringify(data);
    return new Promise((resolve, reject) => {
      const req = request({ hostname: "127.0.0.1", port, path, method: data === undefined ? "GET" : "POST",
        headers: { Host: "127.0.0.1:4278", ...(data === undefined ? {} : {
          Origin: PUBLIC_WEBADMIN_ORIGIN, "Content-Type": "application/json",
          "Content-Length": String(Buffer.byteLength(body)),
        }), ...additional } }, res => {
        const chunks: Buffer[] = [];
        res.on("data", c => chunks.push(c));
        res.on("end", () => {
          try { resolve({ code: res.statusCode ?? 0,
            body: JSON.parse(Buffer.concat(chunks).toString("utf8")) as Record<string, unknown>,
            setCookie: res.headers["set-cookie"] ?? [] }); }
          catch (error) { reject(error); }
        });
      });
      req.on("error", reject);
      req.end(body);
    });
  };
  const runLogin = async () => {
    const initial = await call("/stage4/v1/challenge");
    assert.equal(initial.code, 200);
    const options = await call("/stage4/v1/login/options", {
      actor_id: "7", password: "valid-synthetic-password",
    }, { Cookie: initial.setCookie[0]!.split(";")[0]!,
      "X-DC24H-Stage4-Csrf": initial.body.csrf as string });
    return { options, initial };
  };
  return {
    call, runLogin, events,
    setAllowed: (v: boolean) => { allowed = v; },
    setApproved: (v: boolean) => { approved = v; },
    setHealthy: (v: boolean) => { healthy = v; },
    changeEpoch: () => { epoch++; },
    setTime: (v: number) => { at = v; },
    counts: () => ({ passwordChecks, passkeyChecks }),
    close: () => new Promise<void>((resolve, reject) => server.close(error => error ? reject(error) : resolve())),
  };
}

const pendingHeaders = (options: Reply) => ({
  Cookie: options.setCookie.find(v => v.startsWith("__Host-dc24h_stage4_pending="))!.split(";")[0]!,
  "X-DC24H-Stage4-Csrf": options.body.csrf as string,
});
const sessionHeaders = (finish: Reply) => ({
  Cookie: finish.setCookie.find(v => v.startsWith("__Host-dc24h_stage4_session="))!.split(";")[0]!,
  "X-DC24H-Stage4-Csrf": finish.body.csrf as string,
});
async function authenticated(f: Awaited<ReturnType<typeof fixture>>) {
  const { options } = await f.runLogin();
  assert.equal(options.code, 200);
  const finish = await f.call("/stage4/v1/login/finish", { response: candidate }, pendingHeaders(options));
  assert.equal(finish.code, 200);
  return { options, finish, headers: sessionHeaders(finish) };
}

test("public gateway has a strict route, method, peer, Host and Origin boundary", async () => {
  const f = await fixture();
  try {
    for (const path of ["/", "/remote/v1/challenge", "/admin-api/v1/status", "/stage4/v1/session?x=1",
      "/stage4/v1/%63hallenge", "/stage4/v1/../session", "/stage4/v1//challenge",
      "/stage4/v1/challenge/", "/stage4/v1/challenge%00"]) {
      assert.equal((await f.call(path)).code, 404, path);
    }
    for (const headers of ([{ Host: "server-747585786.dc24h.eu:7278" },
      { "X-Forwarded-For": "127.0.0.1" }, { Origin: "https://server-747585786.dc24h.eu:1512" },
      { "Sec-Fetch-Site": "same-site" }, { Authorization: "Bearer fake" }] as Record<string, string>[])) {
      assert.equal((await f.call("/stage4/v1/challenge", undefined, headers)).code, 403);
    }
  } finally { await f.close(); }
});

test("password-only and passkey-only cannot issue sessions; exactly two factors succeed", async () => {
  const f = await fixture();
  try {
    assert.equal((await f.call("/stage4/v1/session")).code, 401);
    const { options } = await f.runLogin();
    assert.equal(options.code, 200);
    assert.equal((await f.call("/stage4/v1/session")).code, 401);
    const noCsrf = await f.call("/stage4/v1/login/finish", { response: candidate }, {
      Cookie: pendingHeaders(options).Cookie,
    });
    assert.equal(noCsrf.code, 403);
    assert.equal(f.counts().passkeyChecks, 0);
    const next = await f.runLogin();
    const finish = await f.call("/stage4/v1/login/finish", { response: candidate }, pendingHeaders(next.options));
    assert.equal(finish.code, 200);
    assert.equal(finish.body.role, "owner");
    assert.ok(finish.setCookie.some(v => v.startsWith("__Host-dc24h_stage4_session=") &&
      v.includes("Path=/") && v.includes("Secure") && v.includes("HttpOnly") &&
      v.includes("SameSite=Strict") && !v.includes("Domain=")));
    const headers = sessionHeaders(finish);
    assert.equal((await f.call("/stage4/v1/session", undefined, headers)).code, 200);
    assert.equal((await f.call("/stage4/v1/login/finish", { response: candidate }, pendingHeaders(next.options))).code, 403);
    assert.equal(f.counts().passkeyChecks, 1);
  } finally { await f.close(); }
});

test("invalid password, absent passkey and revoked backend role are denied", async () => {
  const f = await fixture();
  try {
    const initial = await f.call("/stage4/v1/challenge");
    const rejected = await f.call("/stage4/v1/login/options", { actor_id: "7", password: "incorrect" }, {
      Cookie: initial.setCookie[0]!.split(";")[0]!,
      "X-DC24H-Stage4-Csrf": initial.body.csrf as string,
    });
    assert.equal(rejected.code, 403);
    assert.ok(!JSON.stringify(rejected.body).includes("incorrect"));
    f.setApproved(false);
    const { options } = await f.runLogin();
    assert.equal((await f.call("/stage4/v1/login/finish", { response: candidate }, pendingHeaders(options))).code, 403);
    f.setApproved(true);
    const session = await authenticated(f);
    f.setAllowed(false);
    assert.equal((await f.call("/stage4/v1/session", undefined, session.headers)).code, 403);
    assert.equal((await f.call("/stage4/v1/session", undefined, session.headers)).code, 401);
  } finally { await f.close(); }
});

test("session CSRF, freshness, idle expiry and device epoch revocation fail closed", async () => {
  const f = await fixture();
  try {
    const auth = await authenticated(f);
    assert.equal((await f.call("/stage4/v1/logout", {}, { Cookie: auth.headers.Cookie })).code, 403);
    assert.equal((await f.call("/stage4/v1/logout", {}, {
      ...auth.headers, Origin: "https://server-747585786.dc24h.eu:1512",
    })).code, 403);
    assert.equal((await f.call("/stage4/v1/summary", undefined, auth.headers)).code, 200);
    f.changeEpoch();
    assert.equal((await f.call("/stage4/v1/session", undefined, auth.headers)).code, 401);
    const fresh = await authenticated(f);
    f.setTime(221_000);
    assert.equal((await f.call("/stage4/v1/session", undefined, fresh.headers)).code, 401);
  } finally { await f.close(); }
});

test("revoking own sessions and credentials prevents any session reuse", async () => {
  const f = await fixture();
  try {
    const a = await authenticated(f);
    assert.equal((await f.call("/stage4/v1/sessions/revoke", {}, a.headers)).code, 200);
    assert.equal((await f.call("/stage4/v1/session", undefined, a.headers)).code, 401);
    const b = await authenticated(f);
    assert.equal((await f.call("/stage4/v1/devices")).code, 401);
    assert.equal((await f.call("/stage4/v1/devices", undefined, b.headers)).code, 200);
    assert.equal((await f.call("/stage4/v1/devices/revoke", { id: "wrong" }, b.headers)).code, 403);
    assert.equal((await f.call("/stage4/v1/devices/revoke", { id: "some-credential" }, b.headers)).code, 200);
    assert.equal((await f.call("/stage4/v1/session", undefined, b.headers)).code, 401);
  } finally { await f.close(); }
});

test("audit or rate limiter unavailability denies login and existing sessions", async () => {
  const f = await fixture();
  try {
    const login = await authenticated(f);
    f.setHealthy(false);
    assert.equal((await f.call("/stage4/v1/session", undefined, login.headers)).code, 503);
    assert.equal((await f.call("/stage4/v1/challenge")).code, 503);
    f.setHealthy(true);
    assert.equal((await f.call("/stage4/v1/session", undefined, login.headers)).code, 401);
  } finally { await f.close(); }
});
