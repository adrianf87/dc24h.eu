import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { createServer, request } from "node:http";
import { setTimeout as delay } from "node:timers/promises";
import { join } from "node:path";
import test from "node:test";
import mariadb from "mariadb";
import { publicBackend } from "../server/public-backend.ts";
import { databasePublicGateway } from "../server/public-gateway.ts";
import { openStage4Store } from "../server/stage4-store.ts";
import type { RuntimeConfiguration } from "../server/stage4-store.ts";

test("disposable C++/MariaDB authorization and mock SMTP through the gateway", async () => {
  const fixture = process.env.STAGE4_FIXTURE!;
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
  const server = createServer(databasePublicGateway({ db, backend, now: () => time,
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
              host: "127.0.0.1:4278", origin: "https://server-747585786.dc24h.eu:7278",
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
        assert(item.includes("Secure") && item.includes("HttpOnly") && item.includes("SameSite=Strict"));
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
    const initialCode = messages.at(-1)!.code;
    const wrong = initialCode === "00000000" ? "99999999" : "00000000";
    assert.equal((await owner.request("login/verify", { code: initialCode }, false)).status, 403);
    assert.equal((await owner.request("login/verify", { code: wrong })).status, 403);
    assert.equal(await count("SELECT COUNT(*) n FROM stage4_challenges WHERE attempts=1"), 1);
    assert.equal(await count("SELECT COUNT(*) n FROM stage4_challenges WHERE HEX(code_mac) REGEXP '^[0-9A-F]{64}$'"), 1);
    const enrollment = await owner.request("login/verify", { code: initialCode });
    assert.equal(enrollment.status, 200);
    assert.equal(enrollment.json.role, "owner");
    assert.equal((await owner.request("summary")).status, 200);
    assert.equal(await count("SELECT COUNT(*) n FROM stage4_emails WHERE verified_at IS NOT NULL"), 1);
    assert.equal((await owner.request("login/verify", { code: initialCode })).status, 403);

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
    assert.equal((await second.request("password/change", { current: adminPassword, password: nextPassword })).status, 200);
    assert.equal((await second.request("session")).status, 401);

    const user = browser("192.0.2.12");
    const signup = { username: "SignupFixture", email: "signup@example.test", password: "A new fixture password 73!" };
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
    await db.clean();
  } finally {
    await new Promise<void>((resolve, reject) => server.close(error => error ? reject(error) : resolve()));
    await Promise.all([db.close(), app.end(), root.end()]);
  }
});
