import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { createServer, request } from "node:http";
import { setTimeout as delay } from "node:timers/promises";
import { join } from "node:path";
import test from "node:test";
import mariadb from "mariadb";
import { publicBackend, PublicUpstreamError } from "../server/public-backend.ts";
import { databasePublicGateway } from "../server/public-gateway.ts";
import { approvedStage4Origin } from "../server/stage4-apache-site.ts";
import { openStage4Store, stage4SessionDigest } from "../server/stage4-store.ts";
import type { RuntimeConfiguration } from "../server/stage4-store.ts";

test("disposable C++/MariaDB authorization and mock SMTP through the gateway", async () => {
  const fixture = process.env.STAGE4_FIXTURE!;
  const origin = approvedStage4Origin(process.env.STAGE4_FIXTURE_PORT ?? "");
  assert(origin);
  const state = "/etc/dc24h/stage4-test";
  const settings = JSON.parse(readFileSync(join(state, "stage4-runtime.json"), "utf8")) as RuntimeConfiguration;
  assert.equal(settings.environment, "test");
  const adminPassword = readFileSync(join(fixture, "api/fixture.password"), "utf8");
  const nextPassword = "Updated Cpp passphrase 456!";
  const backend = publicBackend({ socket: join(fixture, "api/auth.sock"),
    token: readFileSync(join(state, "api.token"), "utf8") });
  const root = await mariadb.createConnection({ user: "root", database: settings.database, socketPath: settings.socketPath });
  const app = await mariadb.createConnection(settings);
  let time = Date.now();
  const db = openStage4Store(settings, () => time);
  const messages: Array<{ email: string; code: string }> = [];
  let smtpWorks = true;
  const server = createServer(databasePublicGateway({ db, backend, origin, now: () => time,
    sendCode: async (email, code) => {
      if (!smtpWorks) return false;
      assert.match(code, /^\d{8}$/);
      messages.push({ email, code });
      return true;
    },
  }));
  await new Promise<void>(resolve => server.listen(0, "127.0.0.1", resolve));
  const address = server.address();
  assert(address && typeof address !== "string");
  const port = address.port;
  function browser(ip: string) {
    const cookies = new Map<string, string>();
    let csrf = "";
    const fetchApi = async (path: string, data?: object, useCsrf = true) => {
      const text = data ? JSON.stringify(data) : null;
      const response = await new Promise<{ status: number; cookies: string[]; json: Record<string, unknown> }>(
        (resolve, reject) => {
          const connection = request({ hostname: "127.0.0.1", port, path: "/stage4/v1/" + path,
            method: text ? "POST" : "GET", headers: {
              host: "127.0.0.1:4278", origin,
              "x-dc24h-stage4-ip": ip, cookie: [...cookies].map(([k, v]) => k + "=" + v).join("; "),
              ...(text ? { "content-type": "application/json", "content-length": Buffer.byteLength(text),
                ...(useCsrf ? { "x-dc24h-stage4-csrf": csrf } : {}) } : {}),
            },
          }, incoming => {
            const chunks: Buffer[] = [];
            incoming.on("data", part => chunks.push(part as Buffer));
            incoming.on("error", reject);
            incoming.on("end", () => {
              try {
                resolve({ status: incoming.statusCode!, cookies: incoming.headers["set-cookie"] ?? [],
                  json: JSON.parse(Buffer.concat(chunks).toString()) as Record<string, unknown> });
              } catch (error) { reject(error); }
            });
          });
          connection.on("error", reject);
          connection.end(text);
        });
      const { json } = response;
      for (const item of response.cookies) {
        const [name, value] = item.split(";")[0]!.split("=");
        const attributes = item.split(";").slice(1).map(attribute => attribute.trim().toLowerCase());
        assert(attributes.includes("secure") && attributes.includes("httponly") &&
          attributes.includes("samesite=strict") && attributes.includes("path=/"));
        assert(!attributes.some(attribute => attribute.startsWith("domain=")));
        if (value) cookies.set(name!, value);
        else cookies.delete(name!);
      }
      if (typeof json.csrf === "string") csrf = json.csrf;
      return { status: response.status, json };
    };
    const send = async (path: string, data: object) => {
      assert.equal((await fetchApi("challenge")).status, 200);
      return fetchApi(path, data);
    };
    return { request: fetchApi, send, cookies };
  }
  try {
    await db.check();
    await assert.rejects(mariadb.createConnection({ ...settings, password: "incorrect fixture password" }));
    await assert.rejects(app.query("SELECT verifier FROM dc24h_account_credentials"));
    await assert.rejects(app.query("UPDATE dc24h_accounts SET role='owner'"));
    await assert.rejects(app.query("CREATE TABLE forbidden (id INT)"));
    const owner = browser("192.0.2.10");
    const login = { username: "Administrator", password: adminPassword };
    const ownerBegin = () => owner.send("login/send", login);
    const count = async (sql: string) => Number((await root.query(sql) as { n: bigint }[])[0]!.n);
    assert.equal(await count("SELECT COUNT(*) n FROM dc24h_accounts WHERE role='owner' AND status='disabled'"), 1);
    assert.equal((await owner.send("login/send", { ...login, password: "Incorrect fixture password" })).status, 403);
    assert.equal(messages.length, 0);
    assert.equal((await owner.send("login/send", { ...login, username: "a' OR 1=1" })).status, 400);
    smtpWorks = false;
    assert.equal((await ownerBegin()).status, 403);
    assert.equal(await count("SELECT COUNT(*) n FROM stage4_challenges"), 0);
    smtpWorks = true;
    assert.equal((await ownerBegin()).status, 200);
    assert.equal(messages.at(-1)?.email, "admin@dc24h.eu");
    assert.equal((await owner.request("session")).status, 401);
    assert.equal((await owner.request("tls/renewal")).status, 401);
    const initialCode = messages.at(-1)!.code;
    const wrong = initialCode === "00000000" ? "99999999" : "00000000";
    assert.equal((await owner.request("login/verify", { code: initialCode }, false)).status, 403);
    assert.equal((await owner.request("login/verify", { code: wrong })).status, 403);
    assert.equal(await count("SELECT COUNT(*) n FROM stage4_challenges WHERE attempts=1"), 1);
    assert.equal(await count("SELECT COUNT(*) n FROM stage4_challenges WHERE HEX(code_mac) REGEXP '^[0-9A-F]{64}$'"), 1);
    const pendingCookie = owner.cookies.get("__Host-dc24h_stage4_email_pending");
    const enrollment = await owner.request("login/verify", { code: initialCode });
    assert.equal(enrollment.status, 200);
    assert.equal(enrollment.json.role, "owner");
    const sessionCookie = owner.cookies.get("__Host-dc24h_stage4_session");
    assert(pendingCookie && sessionCookie && pendingCookie !== sessionCookie);
    assert(!owner.cookies.has("__Host-dc24h_stage4_email_pending"));
    assert.equal((await owner.request("summary")).status, 200);
    assert.equal(await count("SELECT COUNT(*) n FROM stage4_emails WHERE verified_at IS NOT NULL"), 1);
    assert.equal((await owner.request("login/verify", { code: initialCode })).status, 403);
    const upstream = (status: number, code: string) => (error: unknown) =>
      error instanceof PublicUpstreamError && error.status === status && error.code === code;
    const tlsChange = { enabled: true, password: adminPassword };
    assert.equal((await owner.request("tls/renewal", tlsChange, false)).status, 403);
    assert.equal((await owner.request("tls/renewal", { ...tlsChange, unit: "apache2.service" })).status, 400);
    await assert.rejects(backend.tlsRenewal("status", "0".repeat(64)), upstream(401, "unauthorized"));
    await assert.rejects(backend.tlsRenewal("enable", stage4SessionDigest(sessionCookie),
      "An incorrect fixture password"), upstream(403, "proof_refused"));

    const accountSession = stage4SessionDigest(sessionCookie);
    await root.execute("UPDATE stage4_sessions SET issued_at=? WHERE token=UNHEX(?)", [Date.now() - 301000, accountSession]);
    await assert.rejects(backend.tlsRenewal("enable", accountSession, adminPassword), upstream(403, "fresh_login_required"));
    await root.execute("UPDATE stage4_sessions SET issued_at=? WHERE token=UNHEX(?)", [time, accountSession]);

    const administrative = (operation: string, values: object = {}, useCsrf = true) =>
      owner.request("admin", { operation, ...values }, useCsrf);
    assert.equal((await administrative("accounts.list", { after: "0" }, false)).status, 403);
    assert.equal((await administrative("accounts.list", { after: "0", actor_id: "1" })).status, 400);
    assert.equal((await administrative("hub.restart")).status, 400);
    assert.equal((await administrative("management")).status, 200);
    assert.equal((await administrative("admin")).status, 200);
    assert.equal((await administrative("accounts.staff", { after: "0" })).status, 200);
    assert.equal((await administrative("accounts.list", { after: "0" })).status, 200);
    const invalidProof = "Incorrect fixture password";
    const auditCount = "SELECT COUNT(*) n FROM dc24h_audit_events";
    const auditBefore = await count(auditCount);
    assert.equal((await administrative("accounts.create", {
      proof: invalidProof, name: "RefusedFixture", password: "A disposable fixture password 52!",
    })).status, 403);
    assert.equal((await administrative("audit.list", { proof: invalidProof, after: "0" })).status, 403);
    assert.equal(await count("SELECT COUNT(*) n FROM dc24h_accounts WHERE name='RefusedFixture'"), 0);
    assert.equal(await count(auditCount), auditBefore);
    const created = await administrative("accounts.create", {
      proof: adminPassword, name: "ManagedFixture", password: "Managed fixture password 42!",
    });
    assert.equal(created.status, 200);
    const createdAccount = (created.json.data as { account: { id: string; revision: string; status: string } }).account;
    assert.equal(createdAccount.status, "disabled");
    const target = { id: createdAccount.id, revision: createdAccount.revision, proof: adminPassword };
    assert.equal((await administrative("accounts.detail", { id: target.id })).status, 200);
    const activated = await administrative("accounts.activate", target);
    assert.equal(activated.status, 200);
    assert.equal((await administrative("accounts.disable", target)).status, 409);
    const changed = await administrative("accounts.role", {
      ...target, revision: (activated.json.data as { account: typeof createdAccount }).account.revision,
      role: "moderator",
    });
    assert.equal(changed.status, 200);
    const revision = (changed.json.data as { account: typeof createdAccount }).account.revision;
    assert.equal((await administrative("accounts.reset", { ...target, revision,
      password: "Reset fixture password 37!" })).status, 200);

    assert.equal((await administrative("bans.list", { after: "0", state: "all" })).status, 200);
    const ban = await administrative("bans.create", { proof: adminPassword, kind: "ip",
      subject: "192.0.2.100", reason: "A disposable fixture ban", expiry: "permanent" });
    assert.equal(ban.status, 200);
    const banTarget = (ban.json.data as { ban: { id: string; revision: string } }).ban;
    assert.equal((await administrative("bans.revoke", { id: banTarget.id, revision: banTarget.revision, proof: adminPassword })).status, 200);
    const audit = await administrative("audit.list", { proof: adminPassword, after: "0" });
    assert.equal(audit.status, 200);
    const events = (audit.json.data as { events: { action: string }[] }).events;
    for (const action of ["accounts.create", "accounts.activate", "accounts.role", "accounts.reset", "bans.create", "bans.revoke"])
      assert(events.some(event => event.action === action));

    const second = browser("192.0.2.11");
    assert.equal((await second.send("login/send", login)).status, 200);
    time += 300_001;
    assert.equal((await second.request("login/verify", { code: messages.at(-1)!.code })).status, 403);
    assert.equal((await second.send("login/send", login)).status, 200);
    let code = messages.at(-1)!.code;
    let invalid = code === "11111111" ? "22222222" : "11111111";
    for (let guess = 0; guess < 5; guess++)
      assert.equal((await second.request("login/verify", { code: invalid })).status, 403);
    assert.equal((await second.request("login/verify", { code })).status, 403);
    assert.equal((await second.send("login/send", login)).status, 200);
    code = messages.at(-1)!.code;
    const concurrent = await Promise.all([second.request("login/verify", { code }), second.request("login/verify", { code })]);
    assert.deepEqual(concurrent.map(r => r.status).sort(), [200, 403]);
    assert.equal((await ownerBegin()).status, 200);
    assert.equal((await owner.request("login/verify", { code: messages.at(-1)!.code })).status, 200);
    assert.equal((await owner.request("session")).status, 200);
    assert.equal((await second.request("password/change", { current: adminPassword, password: nextPassword }, false)).status, 403);
    assert.equal((await second.request("password/change", { current: invalidProof, password: nextPassword })).status, 403);
    assert.equal((await owner.request("session")).status, 200);
    assert.equal((await second.request("password/change", { current: adminPassword, password: nextPassword })).status, 200);
    assert.equal((await second.request("session")).status, 401);
    assert.equal((await owner.request("session")).status, 401);

    const user = browser("192.0.2.12");
    const signup = { username: "SignupFixture", email: "signup@example.test", password: "A new fixture password 73!" };
    assert.equal((await user.send("register/send", { ...signup, role: "owner" })).status, 400);
    assert.equal((await user.send("register/send", signup)).status, 200);
    await delay(50);
    assert.equal(messages.at(-1)?.email, signup.email);
    code = messages.at(-1)!.code;
    assert.equal(await count("SELECT COUNT(*) n FROM dc24h_accounts WHERE name='SignupFixture'"), 0);
    assert.equal((await user.request("register/verify", { code: wrong === code ? "88888888" : wrong })).status, 403);
    assert.equal((await user.request("register/verify", { code })).status, 200);
    assert.equal((await user.request("session")).status, 401);
    const delivered = messages.length;
    assert.equal((await user.send("register/send", { ...signup, username: "OtherFixture" })).status, 200);
    assert.equal((await user.send("register/send", signup)).status, 429);
    assert.equal(messages.length, delivered);
    const userLogin = { username: signup.username, password: signup.password };
    assert.equal((await user.send("login/send", userLogin)).status, 200);
    assert.equal((await user.request("login/verify", { code: messages.at(-1)!.code })).json.role, "registered");
    assert.equal((await user.request("summary")).status, 403);
    assert.equal((await user.request("admin", { operation: "accounts.list", after: "0" })).status, 403);
    assert.equal((await user.request("admin", { operation: "bans.list", after: "0", state: "all" })).status, 403);
    assert.equal((await user.request("admin", { operation: "audit.list", proof: signup.password, after: "0" })).status, 403);
    assert.equal((await user.request("hub/status")).status, 403);
    assert.equal((await user.request("tls/renewal")).status, 403);
    assert.equal((await user.request("tls/renewal", { enabled: true, password: signup.password })).status, 403);
    const registeredSession = stage4SessionDigest(user.cookies.get("__Host-dc24h_stage4_session")!);
    await root.execute("UPDATE stage4_sessions SET seen_at=? WHERE token=UNHEX(?)", [Date.now(), registeredSession]);
    await assert.rejects(backend.tlsRenewal("enable", registeredSession, signup.password), upstream(403, "forbidden"));
    await root.execute("UPDATE dc24h_accounts SET revision=revision+1,status='disabled' WHERE name=?", [signup.username]);
    assert.equal((await user.request("session")).status, 401);

    time += 300_001;
    const replacement = { ...login, password: nextPassword };
    assert.equal((await owner.send("login/send", replacement)).status, 200);
    assert.equal((await owner.request("login/verify", { code: messages.at(-1)!.code })).status, 200);
    assert.equal((await second.send("login/send", replacement)).status, 200);
    assert.equal((await second.request("login/verify", { code: messages.at(-1)!.code })).status, 200);
    assert.equal((await owner.request("sessions/revoke", {})).status, 200);
    assert.equal((await second.request("session")).status, 401);
    assert.equal((await owner.send("login/send", replacement)).status, 200);
    assert.equal((await owner.request("login/verify", { code: messages.at(-1)!.code })).status, 200);
    await root.execute("UPDATE dc24h_accounts SET revision=revision+1,role='administrator' WHERE name='Administrator'");
    assert.equal((await owner.request("session")).status, 401);
    time += 300_001;
    const administrator = browser("192.0.2.13");
    assert.equal((await administrator.send("login/send", replacement)).status, 200);
    assert.equal((await administrator.request("login/verify", { code: messages.at(-1)!.code })).json.role, "administrator");
    assert.equal((await administrator.request("summary")).status, 200);
    // Restore the synthetic owner role before the separate backup/restore test.
    await root.execute("UPDATE dc24h_accounts SET revision=revision+1,role='owner' WHERE name='Administrator'");
    assert.equal((await administrator.request("session")).status, 401);

    const limited = browser("192.0.2.14");
    const undelivered = messages.length;
    for (let attempt = 0; attempt < 4; attempt++)
      assert.equal((await limited.send("login/send", { username: "LimitedFixture", password: "Wrong fixture password" })).status, 403);
    assert.equal((await limited.send("login/send", { username: "LimitedFixture", password: "Wrong fixture password" })).status, 429);
    assert.equal(messages.length, undelivered);

    time += 600_001;
    const idle = browser("192.0.2.16");
    const active = browser("192.0.2.17");
    for (const client of [idle, active]) {
      assert.equal((await client.send("login/send", replacement)).status, 200);
      assert.equal((await client.request("login/verify", { code: messages.at(-1)!.code })).status, 200);
    }
    const issued = time;
    time = issued + 119_999;
    assert.equal((await active.request("session")).status, 200);
    time = issued + 120_000;
    assert.equal((await idle.request("session")).status, 401);
    for (const age of [220_000, 320_000, 420_000, 520_000, 599_999]) {
      time = issued + age;
      assert.equal((await active.request("session")).status, 200);
    }
    time = issued + 600_000;
    assert.equal((await active.request("session")).status, 401);

    const resend = browser("192.0.2.18");
    const deliveredBefore = messages.length;
    for (let attempt = 0; attempt < 4; attempt++)
      assert.equal((await resend.send("login/send", replacement)).status, 200);
    assert.equal((await resend.request("login/verify", { code: messages.at(-1)!.code })).status, 200);
    assert.equal((await resend.request("logout", {})).status, 200);
    assert.equal((await resend.request("session")).status, 401);
    assert.equal((await resend.send("login/send", replacement)).status, 429);
    assert.equal(messages.length, deliveredBefore + 4);
    await db.clean();
  } finally {
    await new Promise<void>((resolve, reject) => server.close(error => error ? reject(error) : resolve()));
    await Promise.all([db.close(), app.end(), root.end()]);
  }
});
