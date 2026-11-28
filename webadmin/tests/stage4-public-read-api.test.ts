import assert from "node:assert/strict";
import { createServer, request } from "node:http";
import { once } from "node:events";
import test from "node:test";
import { databasePublicGateway } from "../server/public-gateway.ts";
import type { PublicBackend } from "../server/public-backend.ts";
import type { Stage4Store } from "../server/stage4-store.ts";

const sessionToken = "a".repeat(64);
const host = "127.0.0.1:4278";
const user = { token: sessionToken, issued: Date.now(),
  identity: { id: "7", role: "owner", status: "active", revision: "1" } };

test("Stage IV public read adapters enforce OTP session, role, method and revocation", async () => {
  let actor: typeof user | null = user;
  let revokeDuringRead = false;
  let reads = 0, touched = 0;
  const db = {
    session: async () => actor,
    allowed: async () => true,
    touch: async () => { touched++; },
  } as unknown as Stage4Store;
  const backend = {
    identitySummary: async () => { reads++; if (revokeDuringRead) actor = null;
      return { schema: 1, source: "stage4-account-service",
        accounts: { total: 4, active: 3, owners: 1, administrators: 1, registered: 2 },
        verified_emails: 3 }; },
  } as unknown as PublicBackend;
  const server = createServer(databasePublicGateway({ db, backend, sendCode: async () => false }));
  server.listen(0, "127.0.0.1");
  await once(server, "listening");
  const address = server.address();
  assert(address && typeof address !== "string");
  const query = (path: string, options: { cookie?: boolean; method?: string; origin?: string;
      forwarded?: boolean } = {}) => new Promise<{ code: number; text: string }>((resolve, reject) => {
    const incoming = request({ hostname: "127.0.0.1", port: address.port,
      path: "/stage4/v1/" + path, method: options.method ?? "GET",
      headers: { Host: host, "X-DC24H-Stage4-IP": "192.0.2.10",
        ...(options.cookie === false ? {} : { Cookie: "__Host-dc24h_stage4_session=" + sessionToken }),
        ...(options.origin ? { Origin: options.origin } : {}),
        ...(options.forwarded ? { "X-Forwarded-For": "192.0.2.99" } : {}),
      },
    }, res => {
      const chunks: Buffer[] = [];
      res.on("data", piece => chunks.push(piece as Buffer));
      res.on("end", () => resolve({ code: res.statusCode ?? 0,
        text: Buffer.concat(chunks).toString("utf8") }));
    });
    incoming.on("error", reject); incoming.end();
  });
  try {
    assert.equal((await query("identity-summary", { cookie: false })).code, 401);
    assert.equal((await query("identity-summary", { forwarded: true })).code, 403);
    assert.equal((await query("identity-summary", { origin: "https://evil.example" })).code, 403);
    assert.equal((await query("identity-summary", { method: "POST" })).code, 404);
    assert.equal((await query("telemetry")).code, 404);
    assert.equal((await query("users/list")).code, 404);
    assert.equal((await query("admin/access")).code, 404);
    assert.equal(reads, 0);
    const summary = await query("identity-summary");
    assert.equal(summary.code, 200);
    assert.equal(JSON.parse(summary.text).data.accounts.total, 4);
    assert.equal(touched, 1);
    user.identity.role = "registered";
    assert.equal((await query("identity-summary")).code, 403);
    user.identity.role = "administrator";
    revokeDuringRead = true;
    assert.equal((await query("identity-summary")).code, 401);
    assert.equal(touched, 1);
  } finally {
    await new Promise<void>((resolve, reject) => server.close(error => error ? reject(error) : resolve()));
  }
});
