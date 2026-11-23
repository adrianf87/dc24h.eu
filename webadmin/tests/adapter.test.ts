import assert from "node:assert/strict";
import { test } from "node:test";
import {
  mkdtempSync,
  writeFileSync,
  chmodSync,
  symlinkSync,
  unlinkSync,
  rmSync,
} from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { createServer, request } from "node:http";
import type { AddressInfo } from "node:net";
import { once } from "node:events";
import { credentials, middleware } from "../server/local-api.ts";
import { snapshot } from "./fixtures.ts";

test("private credentials and bounded local adapter never expose or accept browser credentials", async () => {
  const directory = mkdtempSync(join(tmpdir(), "dc24h-web-test-")),
    socket = join(directory, "hub.sock"),
    file = join(directory, "admin.token");
  const token = "a".repeat(64);
  writeFileSync(file, token + "\n", { mode: 0o600 });
  let observed = "",
    behavior = "normal";
  const upstream = createServer((req, res) => {
    observed = req.headers.authorization ?? "";
    res.setHeader("Content-Type", "application/json");
    if (behavior === "stall") return;
    if (behavior === "unauthorized") {
      res.statusCode = 401;
      res.end("{}");
      return;
    }
    if (behavior === "oversized") {
      res.end("x".repeat(17000));
      return;
    }
    if (behavior === "invalid") {
      res.end("{}");
      return;
    }
    if (behavior === "mime-lookalike") res.setHeader("Content-Type", "application/json-attacker");
    if (behavior === "mime-utf8") res.setHeader("Content-Type", "application/json; charset=utf-8");
    if (req.url === "/api/v1/health") res.end(JSON.stringify({ status: "ok" }));
    else if (req.url === "/api/v1/users") res.end(JSON.stringify({ schema: 1, truncated: false, users: [], secret: token }));
    else res.end(JSON.stringify({ ...snapshot, secret: token }));
  });
  upstream.listen(socket);
  await once(upstream, "listening");
  chmodSync(socket, 0o600);
  const config = credentials({
    DC24H_API_SOCKET: socket,
    DC24H_API_TOKEN_FILE: file,
  });
  assert.deepEqual(config, { socket, token });
  assert.equal(credentials({}), null);
  assert.throws(() => credentials({ DC24H_API_SOCKET: socket }));
  assert.throws(() =>
    credentials(
      { DC24H_API_SOCKET: socket, DC24H_API_TOKEN_FILE: file },
      directory,
    ),
  );
  const handler = middleware(config);
  const adapter = createServer((req, res) =>
    handler(req, res, () => {
      res.statusCode = 404;
      res.end();
    }),
  );
  adapter.listen(0, "127.0.0.1");
  await once(adapter, "listening");
  const port = (adapter.address() as AddressInfo).port;
  const get = async (
    path: string,
    extra: Record<string, string> = {},
    method = "GET",
  ) =>
    new Promise<{ status: number; text: string }>((resolve, reject) => {
      const outgoing = request(
        { hostname: "127.0.0.1", port, path, method, headers: extra },
        (res) => {
          let text = "";
          res.on("data", (chunk) => (text += chunk));
          res.on("end", () => resolve({ status: res.statusCode!, text }));
        },
      );
      outgoing.on("error", reject);
      outgoing.end();
    });
  try {
    const result = await get("/admin-api/v1/status", {
      Authorization: "Bearer browser-controlled",
    });
    assert.equal(result.status, 200);
    assert.deepEqual(JSON.parse(result.text), snapshot);
    assert.equal(observed, `Bearer ${token}`);
    assert.ok(!result.text.includes(token));
    assert.equal((await get("/admin-api/v1/status?extra=1")).status, 404);
    const users = await get("/admin-api/v1/users");
    assert.equal(users.status, 200);
    assert.deepEqual(JSON.parse(users.text), { schema: 1, truncated: false, users: [] });
    assert.ok(!users.text.includes(token));
    assert.equal(
      (
        await get("/admin-api/v1/status", {
          Origin: "https://untrusted.example",
        })
      ).status,
      403,
    );
    assert.equal(
      (await get("/admin-api/v1/status", { "Sec-Fetch-Site": "cross-site" }))
        .status,
      403,
    );
    assert.equal(
      (await get("/admin-api/v1/status", { Host: "untrusted.example" })).status,
      403,
    );
    assert.equal((await get("/admin-api/v1/status", {}, "POST")).status, 405);
    behavior = "unauthorized";
    assert.equal(
      JSON.parse((await get("/admin-api/v1/status")).text).error,
      "api_unauthorized",
    );
    behavior = "oversized";
    assert.equal((await get("/admin-api/v1/status")).status, 502);
    behavior = "invalid";
    assert.equal(
      JSON.parse((await get("/admin-api/v1/status")).text).error,
      "invalid_api_response",
    );
    behavior = "mime-lookalike";
    assert.equal((await get("/admin-api/v1/status")).status, 502);
    behavior = "mime-utf8";
    assert.equal((await get("/admin-api/v1/status")).status, 200);
    behavior = "stall";
    const started = Date.now();
    const limited = await Promise.all(
      Array.from({ length: 9 }, () => get("/admin-api/v1/status")),
    );
    assert.equal(limited.filter((value) => value.status === 429).length, 1);
    assert.equal(limited.filter((value) => value.status === 502).length, 8);
    assert.ok(Date.now() - started < 3500);
    behavior = "normal";
    chmodSync(file, 0o644);
    assert.throws(
      () =>
        credentials({ DC24H_API_SOCKET: socket, DC24H_API_TOKEN_FILE: file }),
      /Invalid local API adapter configuration/,
    );
    chmodSync(file, 0o600);
    const alias = join(directory, "alias.token");
    symlinkSync(file, alias);
    assert.throws(() =>
      credentials({ DC24H_API_SOCKET: socket, DC24H_API_TOKEN_FILE: alias }),
    );
    unlinkSync(alias);
    upstream.closeAllConnections();
    upstream.close();
    await once(upstream, "close");
    const failed = await get("/admin-api/v1/status");
    assert.equal(failed.status, 502);
    assert.ok(!failed.text.includes(directory));
    assert.ok(!failed.text.includes(token));
  } finally {
    adapter.closeAllConnections();
    adapter.close();
    if (upstream.listening) {
      upstream.closeAllConnections();
      upstream.close();
    }
    rmSync(directory, { recursive: true, force: true });
  }
});
