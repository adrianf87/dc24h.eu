import assert from "node:assert/strict";
import { once } from "node:events";
import { createServer, request } from "node:http";
import test from "node:test";
import { publicBackend, PublicUpstreamError } from "../server/public-backend.ts";
import { UBUNTU_WEBADMIN_ORIGIN } from "../server/public-email-code.ts";
import { databasePublicGateway } from "../server/public-gateway.ts";
import { stage4SessionDigest } from "../server/stage4-store.ts";
import type { Stage4Store } from "../server/stage4-store.ts";
import { parseTlsRenewal } from "../src/lib/tls-renewal.ts";
import { tlsStatus } from "./tls-renewal-fixture.ts";

test("TLS activation requires the Owner's session, CSRF and recent OTP, with only fixed operations", async () => {
  const token = "abc1".repeat(16), csrf = "xyz2".repeat(16);
  const current = 2000000;
  const good = { token, identity: { id: "7", role: "owner", status: "active", revision: "9" }, issued: current };
  let active: typeof good | null = good;
  let allow = true, revokedDuringRead = false;
  const calls: unknown[] = [];
  const db = {
    session: async (value: string) => value === token ? active : null,
    allowed: async () => allow,
    isCsrf: (purpose: string, value: string, supplied: string) =>
      purpose === "session" && value === token && supplied === csrf,
    touch: async () => {},
  } as unknown as Stage4Store;
  const password = "Fixture password only!";
  const backend: ReturnType<typeof publicBackend> = {
    ...publicBackend({ socket: "/fixture/not-used.sock", token: "" }),
    tlsRenewal: async (action, digest, proof) => {
      calls.push([action, digest, proof]);
      if (proof && proof !== password) throw new PublicUpstreamError(403, "proof_refused");
      if (revokedDuringRead) active = null;
      return parseTlsRenewal({ ...tlsStatus, enabled: action === "enable",
        schedule: action === "enable" ? "active" : "disabled", internal_path: "/not-returned" });
    },
  };
  const server = createServer(databasePublicGateway({ db, backend, origin: UBUNTU_WEBADMIN_ORIGIN,
    now: () => current, sendCode: async () => false }));
  server.listen(0, "127.0.0.1");
  await once(server, "listening");
  const address = server.address();
  assert(address && typeof address !== "string");
  const get = (path: string, payload?: object, headers: Record<string, string> = {}, method?: string) =>
    new Promise<[number, { data?: { enabled: boolean }; error?: string }]>((resolve, reject) => {
      const text = payload ? JSON.stringify(payload) : null;
      const requestHeaders = { Host: "127.0.0.1:4278", Origin: UBUNTU_WEBADMIN_ORIGIN, "X-DC24H-Stage4-IP": "192.0.2.10",
        Cookie: "__Host-dc24h_stage4_session=" + token,
        ...(text ? { "Content-Type": "application/json", "Content-Length": Buffer.byteLength(text),
          "X-DC24H-Stage4-Csrf": csrf } : {}), ...headers };
      const client = request({ hostname: "127.0.0.1", port: address.port, path: "/stage4/v1/tls/" + path,
        method: method ?? (payload ? "POST" : "GET"), headers: requestHeaders }, response => {
        const chunks: Buffer[] = [];
        response.on("data", (chunk: Buffer) => chunks.push(chunk));
        response.on("error", reject);
        response.on("end", () => {
          try { resolve([response.statusCode!, JSON.parse(Buffer.concat(chunks).toString())]); }
          catch (error) { reject(error); }
        });
      });
      client.on("error", reject);
      client.end(text);
    });
  try {
    const input = { enabled: true, password };
    assert.equal((await get("renewal", input, { Cookie: "" }))[0], 401);
    assert.equal((await get("renewal", input, { Origin: "https://foreign.test" }))[0], 403);
    assert.equal((await get("renewal", input, { "X-DC24H-Stage4-Csrf": "invalid" }))[0], 403);
    assert.equal((await get("renewal", input, { "X-DC24H-Admin-Grant": "incorrect" }))[0], 403);
    assert.equal((await get("renewal", { ...input, command: "systemctl restart apache2" }))[0], 400);
    assert.equal((await get("renewal/check", { lineage: "test" }))[0], 400);
    assert.equal((await get("reload"))[0], 404);
    assert.equal((await get("renewal?query=1"))[0], 404);
    assert.equal((await get("renewal", undefined, {}, "PUT"))[0], 404);
    active = { ...good, identity: { ...good.identity, role: "registered" } };
    assert.equal((await get("renewal"))[0], 403);
    assert.equal((await get("renewal", input))[0], 403);
    active = { ...good, issued: current - 300000 };
    assert.deepEqual(await get("renewal", input), [403, { error: "fresh_login_required" }]);
    assert.equal(calls.length, 0);

    active = { ...good, identity: { ...good.identity, role: "administrator" } };
    assert.deepEqual(await get("renewal"), [200, { schema: 1, data: tlsStatus }]);
    assert.equal((await get("renewal/check", {}))[0], 200);
    assert.equal((await get("renewal", input))[0], 403);
    active = good;
    allow = false;
    assert.equal((await get("renewal", input))[0], 429);
    allow = true;
    assert.deepEqual(await get("renewal", { enabled: true, password: "Incorrect fixture password" }),
      [403, { error: "proof_refused" }]);
    const [code, body] = await get("renewal", input);
    assert.equal(code, 200);
    assert.equal(body.data?.enabled, true);
    assert.deepEqual(calls.at(-1), ["enable", stage4SessionDigest(token), password]);
    assert.deepEqual(await get("renewal", { enabled: false, password }), [200, { schema: 1, data: tlsStatus }]);
    assert.deepEqual(calls.at(-1), ["disable", stage4SessionDigest(token), password]);
    revokedDuringRead = true;
    assert.equal((await get("renewal"))[0], 401);
  } finally {
    await new Promise<void>(resolve => server.close(() => resolve()));
  }
});
