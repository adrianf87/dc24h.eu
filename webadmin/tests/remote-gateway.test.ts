import assert from "node:assert/strict";
import { test } from "node:test";
import { createServer, request } from "node:http";
import type { AddressInfo } from "node:net";
import { once } from "node:events";
import { mkdtempSync, chmodSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { remoteGateway } from "../server/remote-gateway.ts";
import { PERMISSION_KEYS } from "../src/lib/roles.ts";
import { ACCESS_ROUTES } from "../src/lib/access.ts";
import { snapshot } from "./fixtures.ts";

const token = "a".repeat(64);
const grant = "b".repeat(64);
const session = "c".repeat(32);
const csrf = "d".repeat(64);
const origin = "https://localhost:8443";
const access = {
  maintenance: true, restart_required: true, expires_in: 600,
  actor: { id: "7", role: "administrator", status: "active" },
  grant, session, csrf,
  permissions: PERMISSION_KEYS.map((key) => ({ key, granted: true, implemented: key !== "configuration.manage" })),
  routes: ACCESS_ROUTES.map(([method, path, permission]) => ({ method, path, permission, allowed: true })),
};
const management = (allowed: boolean) => ({
  schema: 2, maintenance: true, restart_required: true,
  areas: [
    { key: "accounts", permission: "accounts.manage", implemented: true, allowed },
    { key: "roles", permission: "roles.manage", implemented: true, allowed },
    { key: "bans", permission: "users.ban+blocks.manage", implemented: true, allowed },
    { key: "configuration", permission: "configuration.manage", implemented: false, allowed },
    { key: "hub", permission: "hub.manage", implemented: true, allowed },
  ],
});

test("remote gateway authenticates a real API proof but projects only approved aggregates", async () => {
  const directory = mkdtempSync(join(tmpdir(), "dc24h-remote-"));
  const socket = join(directory, "api.sock");
  let clock = 100_000;
  let revoked = false;
  let upstreamCalls: string[] = [];
  const events: string[] = [];
  const api = createServer((req, res) => {
    upstreamCalls.push((req.method ?? "") + " " + req.url);
    assert.equal(req.headers.authorization, "Bearer " + token);
    assert.equal(req.headers.host, "localhost");
    let content = "";
    req.on("data", (part) => { content += part.toString(); });
    req.on("end", () => {
      res.setHeader("Content-Type", "application/json");
      if (req.url === "/api/v1/admin/access" && req.method === "POST") {
        const form = new URLSearchParams(content);
        if (form.get("actor_id") !== "7" || form.get("proof") !== "correct") {
          res.statusCode = 403; res.end('{"error":"unauthorized"}'); return;
        }
        res.end(JSON.stringify(access)); return;
      }
      if (req.url === "/api/v1/management" && req.method === "GET") {
        assert.equal(req.headers["x-dc24h-admin-grant"], grant);
        assert.equal(req.headers["x-dc24h-session"], session);
        assert.equal(req.headers["x-dc24h-csrf"], csrf);
        res.end(JSON.stringify(management(!revoked))); return;
      }
      if (req.url === "/api/v1/status" && req.method === "GET") {
        res.end(JSON.stringify({ ...snapshot, secret: token, server: { ...snapshot.server, private_key: "DO-NOT-LEAK" } })); return;
      }
      res.statusCode = 404; res.end('{"error":"not_found"}');
    });
  });
  api.listen(socket);
  await once(api, "listening");
  chmodSync(socket, 0o600);
  const gateway = createServer(remoteGateway({ socket, token }, {
    origin, now: () => clock, audit: (event) => events.push(event),
  }));
  gateway.listen(0, "127.0.0.1");
  await once(gateway, "listening");
  const port = (gateway.address() as AddressInfo).port;
  const call = async (path: string, method = "GET", body = "", headers: Record<string, string> = {}) => {
    const response = await new Promise<{ status: number; data: string; cookies: string[] }>((resolve, reject) => {
      const outgoing = request({ hostname: "127.0.0.1", port, path, method, headers: {
        ...(method === "POST" ? { Origin: origin, "Content-Type": "application/json",
          "Content-Length": String(Buffer.byteLength(body)) } : {}),
        ...headers,
      } }, (answer) => {
        const chunks: Buffer[] = [];
        answer.on("data", (chunk) => chunks.push(chunk));
        answer.on("end", () => resolve({ status: answer.statusCode!, data: Buffer.concat(chunks).toString("utf8"),
          cookies: answer.headers["set-cookie"] ?? [] }));
      });
      outgoing.on("error", reject); outgoing.end(body);
    });
    return response;
  };
  try {
    assert.equal((await call("/remote/v1/summary")).status, 401);
    assert.equal((await call("/remote/v1/management/hub/start", "POST", "{}")).status, 404);
    assert.equal((await call("/admin-api/v1/status")).status, 404);
    assert.equal((await call("/remote/v1/summary?extra=1")).status, 404);
    assert.equal((await call("/remote/v1/challenge", "POST", "{}")).status, 404);
    assert.equal((await call("/remote/v1/challenge", "GET", "", { Host: "evil.example" })).status, 403);
    assert.equal((await call("/remote/v1/challenge", "GET", "", { Authorization: "Bearer attacker" })).status, 403);
    const challenge = await call("/remote/v1/challenge");
    const loginCookie = challenge.cookies[0]!.split(";")[0]!;
    assert.ok(challenge.cookies[0]!.includes("HttpOnly; Secure; SameSite=Strict"));
    const challengeToken = JSON.parse(challenge.data).csrf as string;
    assert.equal((await call("/remote/v1/login", "POST", JSON.stringify({ actor_id: "7", password: "correct" }),
      { Cookie: loginCookie, "X-DC24H-Remote-CSRF": "0".repeat(64) })).status, 403);
    const fresh = await call("/remote/v1/challenge");
    const login = await call("/remote/v1/login", "POST", JSON.stringify({ actor_id: "7", password: "correct" }), {
      Cookie: fresh.cookies[0]!.split(";")[0]!,
      "X-DC24H-Remote-CSRF": JSON.parse(fresh.data).csrf,
    });
    assert.equal(login.status, 200);
    const browserCookie = login.cookies[0]!.split(";")[0]!;
    const loginData = JSON.parse(login.data);
    assert.equal(loginData.authenticated, true);
    assert.equal(loginData.mode, "read-only");
    assert.ok(!login.data.includes(grant) && !login.data.includes(session));
    assert.equal((await call("/remote/v1/management/hub/start", "POST", "{}", {
      Cookie: browserCookie, "X-DC24H-Remote-CSRF": loginData.csrf,
    })).status, 404);
    assert.equal((await call("/remote/v1/summary", "GET", "", {
      Cookie: browserCookie, "X-Forwarded-For": "198.51.100.2",
    })).status, 403);
    assert.equal((await call("/remote/v1/logout", "POST", "{}", {
      Cookie: browserCookie, Origin: "https://evil.example", "X-DC24H-Remote-CSRF": loginData.csrf,
    })).status, 403);
    assert.equal((await call("/remote/v1/session", "GET", "", { Cookie: browserCookie })).status, 200);
    const summary = await call("/remote/v1/summary", "GET", "", { Cookie: browserCookie });
    assert.equal(summary.status, 200);
    assert.deepEqual(JSON.parse(summary.data), {
      schema: 1, mode: "read-only",
      server: { version: "beta-0.2.1", running: true, uptime_seconds: 3661 },
      network: { active_connections: 7, accepted_connections: 48 },
      users: { online: 4, peak_online: 6 }, chat: { received: 12, rejected: 2 },
    });
    assert.ok(!summary.data.includes(token) && !summary.data.includes("DO-NOT-LEAK"));
    assert.deepEqual(upstreamCalls.filter((s) => s.includes("/api/v1/status")), ["GET /api/v1/status"]);
    assert.equal((await call("/remote/v1/logout", "POST", "{}", { Cookie: browserCookie })).status, 403);
    revoked = true; // The underlying role is reduced while this session is live.
    assert.equal((await call("/remote/v1/summary", "GET", "", { Cookie: browserCookie })).status, 403);
    assert.equal((await call("/remote/v1/session", "GET", "", { Cookie: browserCookie })).status, 401);
    assert.ok(events.includes("session_revoked"));
    assert.ok(!events.some((line) => line.includes("correct") || line.includes(grant)));
    assert.equal((await call("/remote/v1/login", "POST", "{}", { Cookie: loginCookie })).status, 403);
    assert.ok(challengeToken.length === 64);
    revoked = false;
    const renewed = await call("/remote/v1/challenge");
    const again = await call("/remote/v1/login", "POST", JSON.stringify({ actor_id: "7", password: "correct" }), {
      Cookie: renewed.cookies[0]!.split(";")[0]!, "X-DC24H-Remote-CSRF": JSON.parse(renewed.data).csrf,
    });
    assert.equal(again.status, 200);
    clock += 2 * 60_000 + 1;
    assert.equal((await call("/remote/v1/summary", "GET", "", { Cookie: again.cookies[0]!.split(";")[0]! })).status, 401);
  } finally {
    gateway.close();
    api.close();
    await Promise.all([once(gateway, "close"), once(api, "close")]);
    rmSync(directory, { recursive: true, force: true });
  }
});

test("remote gateway rejects public origins and unauthenticated writes", async () => {
  assert.throws(() => remoteGateway(null, { origin: "http://dc24h.eu" }));
  assert.throws(() => remoteGateway(null, { origin: "https://dc24h.eu/path" }));
  const gateway = createServer(remoteGateway(null, { origin }));
  gateway.listen(0, "127.0.0.1");
  await once(gateway, "listening");
  const port = (gateway.address() as AddressInfo).port;
  try {
    const status = await new Promise<number>((resolve, reject) => {
      const req = request({ hostname: "127.0.0.1", port, path: "/remote/v1/login", method: "POST",
        headers: { Origin: "https://evil.example", "Content-Type": "application/json", "Content-Length": "2" } },
      (res) => { res.resume(); res.on("end", () => resolve(res.statusCode!)); });
      req.on("error", reject); req.end("{}");
    });
    assert.equal(status, 403);
  } finally { gateway.close(); await once(gateway, "close"); }
});
