import assert from "node:assert/strict";
import { test } from "node:test";
import { createServer } from "node:http";
import { once } from "node:events";
import { mkdtempSync, rmSync } from "node:fs";
import { join } from "node:path";
import { tmpdir } from "node:os";
import { parseIdentitySummary, publicBackend } from "../server/public-backend.ts";
import { ACCESS_ROUTES } from "../src/lib/access.ts";
import { PERMISSION_KEYS } from "../src/lib/roles.ts";
import { snapshot } from "./fixtures.ts";

test("real Unix-socket bridge rechecks a scoped C++ grant and projects only public counts", async t => {
  const directory = mkdtempSync(join(tmpdir(), "dc24h-stage4-api-"));
  const socket = join(directory, "api.sock");
  const token = "c".repeat(64), grant = "d".repeat(64);
  const cSession = "e".repeat(32), csrf = "f".repeat(64);
  let roleRevoked = false;
  const seen: string[] = [];
  const api = createServer((req, res) => {
    seen.push(`${req.method} ${req.url}`);
    assert.equal(req.headers.host, "localhost");
    assert.equal(req.headers.authorization, "Bearer " + token);
    assert.equal(req.headers["x-forwarded-for"], undefined);
    let form = "";
    req.on("data", chunk => { form += chunk.toString(); });
    req.on("end", () => {
      res.setHeader("Content-Type", "application/json; charset=utf-8");
      if (req.url === "/api/v1/admin/access") {
        assert.equal(req.method, "POST");
        const data = new URLSearchParams(form);
        if ((data.get("actor_id") !== "7" && data.get("account_name") !== "Administrator") ||
            data.get("proof") !== "synthetic-correct") {
          res.writeHead(403); res.end('{"error":"denied"}'); return;
        }
        res.end(JSON.stringify({ maintenance: true, restart_required: true, expires_in: 600,
          actor: { id: "7", role: "owner", status: "active" }, grant, session: cSession, csrf,
          permissions: PERMISSION_KEYS.map(key => ({ key, granted: true,
            implemented: key !== "configuration.manage" })),
          routes: ACCESS_ROUTES.map(([method, path, permission]) => ({
            method, path, permission, allowed: true })),
        })); return;
      }
      if (req.url === "/api/v1/management") {
        assert.equal(req.headers["x-dc24h-admin-grant"], grant);
        assert.equal(req.headers["x-dc24h-session"], cSession);
        assert.equal(req.headers["x-dc24h-csrf"], csrf);
      } else {
        assert.equal(req.headers["x-dc24h-admin-grant"], undefined);
        assert.equal(req.headers["x-dc24h-session"], undefined);
        assert.equal(req.headers["x-dc24h-csrf"], undefined);
      }
      if (req.url === "/api/v1/management") {
        if (roleRevoked) { res.writeHead(403); res.end("{\"error\":\"revoked\"}"); return; }
        res.end(JSON.stringify({ schema: 2, maintenance: true, restart_required: true,
          areas: [
            ["accounts", "accounts.manage", true], ["roles", "roles.manage", true],
            ["bans", "users.ban+blocks.manage", true],
            ["configuration", "configuration.manage", false], ["hub", "hub.manage", true],
          ].map(([key, permission, implemented]) => ({ key, permission, implemented, allowed: true })),
        })); return;
      }
      if (req.url === "/api/v1/status") {
        res.end(JSON.stringify({ ...snapshot, server: { ...snapshot.server, private_key: "HIDDEN" },
          private_token: token, sensitive_actor: "owner@example.org" })); return;
      }
      if (req.url === "/api/v1/stage4/identity-summary") {
        res.end(JSON.stringify({ schema: 1, source: "stage4-account-service",
          accounts: { total: 7, active: 6, owners: 1, administrators: 2, registered: 4 },
          verified_emails: 5, private_token: token })); return;
      }
      res.writeHead(404); res.end('{"error":"not_found"}');
    });
  });
  try {
    try {
      api.listen(socket);
      await once(api, "listening");
    } catch (error) {
      // Managed local executors can prohibit AF_UNIX bind. Hosted Ubuntu CI
      // has normal Unix sockets and must execute the full integration test.
      if ((error as NodeJS.ErrnoException).code === "EPERM" &&
          process.env.RUNNER_ENVIRONMENT !== "github-hosted") {
        t.skip("Unix sockets are prohibited in this local sandbox"); return;
      }
      throw error;
    }
    const adapter = publicBackend({ socket, token });
    assert.equal(await adapter.verifyPassword("7", "wrong"), null);
    const account = await adapter.verifyPassword("7", "synthetic-correct");
    assert.ok(account);
    assert.equal(account.role, "owner");
    assert.equal(await adapter.verifyNamedPassword("Administrator", "wrong"), null);
    const byName = await adapter.verifyNamedPassword("Administrator", "synthetic-correct");
    assert.equal(byName?.actor, "7");
    assert.equal(byName?.role, "owner");
    assert.equal(await adapter.reauthorize(account), true);
    const summary = await adapter.summary(account);
    const serialized = JSON.stringify(summary);
    assert.ok(serialized.includes("active_connections"));
    assert.ok(!serialized.includes("HIDDEN"));
    assert.ok(!serialized.includes(token));
    assert.ok(!serialized.includes("owner@example.org"));
    const identities = await adapter.identitySummary();
    assert.deepEqual(identities, { schema: 1, source: "stage4-account-service",
      accounts: { total: 7, active: 6, owners: 1, administrators: 2, registered: 4 },
      verified_emails: 5 });
    assert.ok(!JSON.stringify(identities).includes(token));
    roleRevoked = true;
    assert.equal(await adapter.reauthorize(account), false);
    assert.equal(await adapter.verifyPassword("7", "synthetic-correct"), null);
    assert.deepEqual(new Set(seen), new Set([
      "POST /api/v1/admin/access", "GET /api/v1/management", "GET /api/v1/status", "GET /api/v1/stage4/identity-summary",
    ]));
  } finally {
    if (api.listening)
      await new Promise<void>((resolve, reject) => api.close(error => error ? reject(error) : resolve()));
    rmSync(directory, { recursive: true, force: true });
  }
});

test("Stage IV identity summary adapter fails closed on broken/sensitive counts", () => {
  assert.throws(() => parseIdentitySummary({ schema: 1, source: "stage4-account-service",
    accounts: { total: 2, active: 3, owners: 1, administrators: 0, registered: 1 },
    verified_emails: 1 }), /invalid_identity_summary/);
  assert.throws(() => parseIdentitySummary({ schema: 1, source: "stage4-account-service",
    accounts: { total: "2", active: 1, owners: 1, administrators: 0, registered: 1 },
    verified_emails: 1 }), /invalid_identity_summary/);
});
