import assert from "node:assert/strict";
import { test } from "node:test";
import type { IncomingMessage, ServerResponse } from "node:http";
import { mkdtempSync, chmodSync, rmSync } from "node:fs";
import { join } from "node:path";
import { tmpdir } from "node:os";
import { remoteGateway } from "../server/remote-gateway.ts";
import { openPasskeyStore } from "../server/remote-passkeys.ts";
import type { PasskeyCeremonies } from "../server/remote-webauthn.ts";
import { ACCESS_ROUTES } from "../src/lib/access.ts";
import { PERMISSION_KEYS } from "../src/lib/roles.ts";
import { snapshot } from "./fixtures.ts";

test("passkey gateway binds login and enrollment to one-use challenges, password authority, Origin, CSRF, and revocation", async () => {
  const dir = mkdtempSync(join(tmpdir(), "dc24h-passkey-gateway-")); chmodSync(dir, 0o700);
  try {
    const store = openPasskeyStore(join(dir, "passkeys.json"), "localhost");
    const handle = store.userHandle("7");
    const id = "p".repeat(32);
    store.add({ id, actor: "7", userHandle: handle, publicKey: "q".repeat(60), counter: 0,
      deviceType: "multiDevice", backedUp: false, transports: [], name: "Phone",
      createdAt: 10, lastUsedAt: null, revokedAt: null });
    const ceremonies = {
      beginLogin: async () => ({ challenge: "passkey-challenge", allowCredentials: [{ id }] }),
      finishLogin: async (_actor: string, challenge: string, response: { valid: boolean; expireBeforeReturn?: boolean }) => {
        if (response.expireBeforeReturn) now += 61_000;
        return challenge === "passkey-challenge" && response.valid;
      },
      beginEnrollment: async () => {
        const hold = pausedEnrollment;
        if (hold) { pausedEnrollment = null; hold.entered(); await hold.resume; }
        return { challenge: "enrollment-challenge", rp: { id: "localhost" } };
      },
      finishEnrollment: async (_actor: string, challenge: string, response: { valid: boolean; expireBeforePersist?: boolean }, at: number,
        beforePersist: () => Promise<boolean>) => {
        if (response.expireBeforePersist) now += 51_000;
        if (challenge !== "enrollment-challenge" || !response.valid || !await beforePersist()) return false;
        store.add({ id: "z".repeat(32), actor: "7", userHandle: handle, publicKey: "q".repeat(60),
          counter: 0, deviceType: "singleDevice", backedUp: false, transports: [],
          name: "Passkey", createdAt: at, lastUsedAt: null, revokedAt: null });
        return true;
      },
    } as unknown as PasskeyCeremonies;
    const events: string[] = [], called: string[] = [];
    let now = 100_000;
    let pausedAuthorization: { entered: () => void; resume: Promise<void> } | null = null;
    let pausedEnrollment: { entered: () => void; resume: Promise<void> } | null = null;
    const gateway = remoteGateway({ socket: "/private/test.sock", token: "a".repeat(64) }, {
      origin: "https://localhost:8443", now: () => now,
      passkeys: { store, ceremonies }, strictAudit: true, auditHealthy: () => true,
      audit: (event) => { events.push(event); },
      upstream: async (_cfg, path, method, body) => {
        called.push(method + " " + path);
        if (path === "/api/v1/admin/access") {
          const form = new URLSearchParams(body);
          if (form.get("actor_id") !== "7" || form.get("proof") !== "correct")
            throw Object.assign(Error(), { status: 403 });
          return { maintenance: true, restart_required: true, expires_in: 600,
            actor: { id: "7", role: "owner", status: "active" },
            grant: "b".repeat(64), session: "c".repeat(32), csrf: "d".repeat(64),
            permissions: PERMISSION_KEYS.map(key => ({ key, granted: true,
              implemented: key !== "configuration.manage" })),
            routes: ACCESS_ROUTES.map(([verb, target, permission]) => ({
              method: verb, path: target, permission, allowed: true })),
          };
        }
        if (path === "/api/v1/management") {
          const hold = pausedAuthorization;
          if (hold) { pausedAuthorization = null; hold.entered(); await hold.resume; }
          return {
            schema: 2, maintenance: true, restart_required: true,
            areas: ["accounts", "roles", "bans", "configuration", "hub"].map(key => ({
              key, permission: key === "accounts" ? "accounts.manage" : key === "roles" ? "roles.manage" :
                key === "bans" ? "users.ban+blocks.manage" : key === "hub" ? "hub.manage" : "configuration.manage",
              implemented: key !== "configuration", allowed: true,
            })),
          };
        }
        if (path === "/api/v1/status") return snapshot;
        throw Error("unexpected private route");
      },
    });
    const call = async (name: string, method = "GET", data: unknown = {}, extra: Record<string, string> = {}) => {
      const body = method === "POST" ? JSON.stringify(data) : "";
      const headers: Record<string, string> = { host: "localhost", ...method === "POST" ? {
        origin: "https://localhost:8443", "content-type": "application/json",
        "content-length": String(Buffer.byteLength(body)),
      } : {}, ...extra };
      const req = { method, url: "/remote/v1/" + name, headers,
        socket: { remoteAddress: "127.0.0.1" },
        async *[Symbol.asyncIterator]() { if (body) yield Buffer.from(body); },
      } as unknown as IncomingMessage;
      const result = { status: 0, body: "", cookies: [] as string[] };
      const res = { writableEnded: false, destroyed: false,
        writeHead(status: number, responseHeaders: Record<string, unknown>) {
          result.status = status;
          result.cookies = responseHeaders["Set-Cookie"] as string[] ?? [];
        },
        end(text: string) { result.body = text; },
      } as unknown as ServerResponse;
      await gateway(req, res);
      return result;
    };
    const csrfHeader = (csrf: string, cookie: string) => ({ cookie,
      "x-dc24h-remote-csrf": csrf });
    const start = await call("challenge");
    const init = start.cookies[0]!.split(";")[0]!;
    const csrf = JSON.parse(start.body).csrf as string;
    assert.equal((await call("passkey/login/options", "POST", { actor_id: "7", password: "correct" },
      { cookie: init })).status, 403, "missing challenge header");
    assert.equal((await call("passkey/login/options", "POST", { actor_id: "7", password: "correct" },
      csrfHeader(csrf, init))).status, 403, "challenge consumed even when malformed");
    const challenge = await call("challenge");
    const csrf2 = JSON.parse(challenge.body).csrf as string;
    const options = await call("passkey/login/options", "POST", { actor_id: "7", password: "correct" },
      csrfHeader(csrf2, challenge.cookies[0]!.split(";")[0]!));
    assert.equal(options.status, 200);
    assert.equal(JSON.parse(options.body).challenge, "passkey-challenge");
    const pendingCookie = options.cookies[0]!.split(";")[0]!;
    assert.equal((await call("passkey/login/finish", "POST", { response: { valid: false } },
      csrfHeader(csrf2, pendingCookie))).status, 403);
    assert.equal((await call("passkey/login/finish", "POST", { response: { valid: true } },
      csrfHeader(csrf2, pendingCookie))).status, 403, "replayed finish refused");
    const next = await call("challenge");
    const csrf3 = JSON.parse(next.body).csrf as string;
    const second = await call("passkey/login/options", "POST", { actor_id: "7", password: "correct" },
      csrfHeader(csrf3, next.cookies[0]!.split(";")[0]!));
    const pending2 = second.cookies[0]!.split(";")[0]!;
    assert.equal((await call("passkey/login/finish", "POST", { response: { valid: true } },
      csrfHeader(csrf3, pending2.replace("passkey=", "passkey=bad")))).status, 403);
    const thirdStart = await call("challenge");
    const csrf4 = JSON.parse(thirdStart.body).csrf as string;
    const third = await call("passkey/login/options", "POST", { actor_id: "7", password: "correct" },
      csrfHeader(csrf4, thirdStart.cookies[0]!.split(";")[0]!));
    const approved = await call("passkey/login/finish", "POST", { response: { valid: true } },
      csrfHeader(csrf4, third.cookies[0]!.split(";")[0]!));
    assert.equal(approved.status, 200);
    assert.ok(events.includes("passkey_login_ok"));
    const sessionCookie = approved.cookies[0]!.split(";")[0]!;
    const sessionCsrf = JSON.parse(approved.body).csrf as string;
    assert.equal((await call("passkey/devices", "GET", {}, { cookie: sessionCookie })).status, 200);
    assert.equal((await call("passkey/enroll/options", "POST", {}, { cookie: sessionCookie })).status, 403);
    const enroll = await call("passkey/enroll/options", "POST", {}, csrfHeader(sessionCsrf, sessionCookie));
    assert.equal(enroll.status, 200);
    const enrolled = await call("passkey/enroll/finish", "POST", { response: { valid: true } },
      csrfHeader(sessionCsrf, sessionCookie));
    assert.equal(enrolled.status, 200);
    assert.ok(enrolled.cookies[0]?.includes("HttpOnly; Secure; SameSite=Strict"));
    const rotatedCookie = enrolled.cookies[0]!.split(";")[0]!;
    const rotatedCsrf = JSON.parse(enrolled.body).csrf as string;
    assert.notEqual(rotatedCookie, sessionCookie);
    assert.notEqual(rotatedCsrf, sessionCsrf);
    assert.equal((await call("session", "GET", {}, { cookie: sessionCookie })).status, 401,
      "enrollment must revoke the earlier session cookie");
    assert.equal((await call("session", "GET", {}, { cookie: rotatedCookie })).status, 200);
    assert.equal(store.forActor("7").length, 2);
    assert.equal((await call("passkey/enroll/finish", "POST", { response: { valid: true } },
      csrfHeader(rotatedCsrf, rotatedCookie))).status, 403);
    const rename = await call("passkey/device/rename", "POST", { id, name: "Renamed device" },
      csrfHeader(rotatedCsrf, rotatedCookie));
    assert.equal(rename.status, 200);
    const renamedCookie = rename.cookies[0]!.split(";")[0]!;
    const renamedCsrf = JSON.parse(rename.body).csrf as string;
    assert.notEqual(renamedCookie, rotatedCookie, "privileged rename must rotate the session identifier");
    assert.notEqual(renamedCsrf, rotatedCsrf, "privileged rename must rotate CSRF");
    assert.equal((await call("session", "GET", {}, { cookie: rotatedCookie })).status, 401);
    assert.equal((await call("session", "GET", {}, { cookie: renamedCookie })).status, 200);
    assert.equal((await call("passkey/device/rename", "POST", { id, name: "Stale request" },
      csrfHeader(rotatedCsrf, rotatedCookie))).status, 401);
    assert.equal(store.get("7", id)?.name, "Renamed device");
    assert.equal((await call("passkey/device/rename", "POST", { id, name: "Stale CSRF" },
      csrfHeader(rotatedCsrf, renamedCookie))).status, 403,
    "the new session must not accept the predecessor's CSRF token");
    assert.equal((await call("passkey/device/rename", "POST", { id, name: "Cross-origin" },
      { ...csrfHeader(renamedCsrf, renamedCookie), origin: "https://evil.example" })).status, 403,
    "a correct CSRF token does not override an incorrect Origin");
    const anotherHandle = store.userHandle("8"), anotherId = "x".repeat(32);
    store.add({ id: anotherId, actor: "8", userHandle: anotherHandle, publicKey: "q".repeat(60),
      counter: 0, deviceType: "multiDevice", backedUp: false, transports: [], name: "Other Owner",
      createdAt: 10, lastUsedAt: null, revokedAt: null });
    assert.equal((await call("passkey/device/rename", "POST", { id: anotherId, name: "Hijacked" },
      csrfHeader(renamedCsrf, renamedCookie))).status, 403, "cross-account rename refused");
    assert.equal((await call("passkey/device/revoke", "POST", { id: anotherId },
      csrfHeader(renamedCsrf, renamedCookie))).status, 403, "cross-account revocation refused");
    assert.equal(store.get("8", anotherId)?.name, "Other Owner");
    assert.equal((await call("passkey/device/revoke", "POST", { id },
      csrfHeader(renamedCsrf, renamedCookie))).status, 200);
    assert.equal(store.epoch("7"), 1);
    assert.equal((await call("session", "GET", {}, { cookie: renamedCookie })).status, 401);
    assert.equal(store.forActor("7").length, 1);
    assert.ok(called.includes("POST /api/v1/admin/access"));
    assert.ok(events.includes("passkey_revoke_intent") && events.includes("passkey_revoked"));

    // A cryptographic verification may finish after its one-time challenge
    // expires. The successful proof must not become a new browser session.
    const expiredStart = await call("challenge");
    const expiredCsrf = JSON.parse(expiredStart.body).csrf as string;
    const expiredOptions = await call("passkey/login/options", "POST", { actor_id: "7", password: "correct" },
      csrfHeader(expiredCsrf, expiredStart.cookies[0]!.split(";")[0]!));
    assert.equal(expiredOptions.status, 200);
    const expiredLogin = await call("passkey/login/finish", "POST", { response: { valid: true, expireBeforeReturn: true } },
      csrfHeader(expiredCsrf, expiredOptions.cookies[0]!.split(";")[0]!));
    assert.equal(expiredLogin.status, 403, "expired WebAuthn proof cannot create a session");
    assert.ok(!expiredLogin.cookies.some(value => value.startsWith("dc24h_remote=")));

    const freshStart = await call("challenge");
    const freshCsrf = JSON.parse(freshStart.body).csrf as string;
    const freshOptions = await call("passkey/login/options", "POST", { actor_id: "7", password: "correct" },
      csrfHeader(freshCsrf, freshStart.cookies[0]!.split(";")[0]!));
    assert.equal(freshOptions.status, 200);
    const freshLogin = await call("passkey/login/finish", "POST", { response: { valid: true } },
      csrfHeader(freshCsrf, freshOptions.cookies[0]!.split(";")[0]!));
    assert.equal(freshLogin.status, 200);
    const freshSession = freshLogin.cookies[0]!.split(";")[0]!;
    const freshSessionCsrf = JSON.parse(freshLogin.body).csrf as string;

    // Issue the enrollment challenge 10s after login, then let verification
    // finish 51s later: the challenge is valid, but re-verification is stale.
    now += 10_000;
    const staleEnrollment = await call("passkey/enroll/options", "POST", {},
      csrfHeader(freshSessionCsrf, freshSession));
    assert.equal(staleEnrollment.status, 200);
    assert.equal((await call("passkey/enroll/finish", "POST",
      { response: { valid: true, expireBeforePersist: true } },
      csrfHeader(freshSessionCsrf, freshSession))).status, 403,
    "expired re-verification cannot register a credential even with a valid challenge");
    assert.equal(store.forActor("7").length, 1, "stale enrollment must not persist a key");

    // A password-authorized passkey re-login must not survive logout of its
    // predecessor while the *final* C++ permission recheck is still pending.
    const raceStart = await call("challenge");
    const raceCsrf = JSON.parse(raceStart.body).csrf as string;
    const raceOptions = await call("passkey/login/options", "POST", { actor_id: "7", password: "correct" },
      csrfHeader(raceCsrf, raceStart.cookies[0]!.split(";")[0]! + "; " + freshSession));
    assert.equal(raceOptions.status, 200);
    let signalAuthorization!: () => void;
    let resumeAuthorization!: () => void;
    const enteredAuthorization = new Promise<void>(resolve => { signalAuthorization = resolve; });
    const authorizationGate = new Promise<void>(resolve => { resumeAuthorization = resolve; });
    pausedAuthorization = { entered: signalAuthorization, resume: authorizationGate };
    const lateFinish = call("passkey/login/finish", "POST", { response: { valid: true } },
      csrfHeader(raceCsrf, raceOptions.cookies[0]!.split(";")[0]! + "; " + freshSession));
    await enteredAuthorization;
    assert.equal((await call("logout", "POST", {}, csrfHeader(freshSessionCsrf, freshSession))).status, 200);
    resumeAuthorization();
    const refused = await lateFinish;
    assert.equal(refused.status, 403, "logout during the final permission check must cancel passkey login");
    assert.ok(!refused.cookies.some(value => value.startsWith("dc24h_remote=")),
      "a revoked predecessor must never mint a replacement cookie");

    // Enrollment option generation can also await an HSM/storage worker.
    // Logout during that wait must not mint a usable stale ceremony.
    const nextStart = await call("challenge");
    const nextCsrf = JSON.parse(nextStart.body).csrf as string;
    const nextOptions = await call("passkey/login/options", "POST", { actor_id: "7", password: "correct" },
      csrfHeader(nextCsrf, nextStart.cookies[0]!.split(";")[0]!));
    assert.equal(nextOptions.status, 200);
    const nextLogin = await call("passkey/login/finish", "POST", { response: { valid: true } },
      csrfHeader(nextCsrf, nextOptions.cookies[0]!.split(";")[0]!));
    assert.equal(nextLogin.status, 200);
    const nextSession = nextLogin.cookies[0]!.split(";")[0]!;
    const nextSessionCsrf = JSON.parse(nextLogin.body).csrf as string;
    let signalEnrollment!: () => void;
    let resumeEnrollment!: () => void;
    const enteredEnrollment = new Promise<void>(resolve => { signalEnrollment = resolve; });
    const enrollmentGate = new Promise<void>(resolve => { resumeEnrollment = resolve; });
    pausedEnrollment = { entered: signalEnrollment, resume: enrollmentGate };
    const lateOptions = call("passkey/enroll/options", "POST", {}, csrfHeader(nextSessionCsrf, nextSession));
    await enteredEnrollment;
    assert.equal((await call("logout", "POST", {}, csrfHeader(nextSessionCsrf, nextSession))).status, 200);
    resumeEnrollment();
    assert.equal((await lateOptions).status, 403,
      "logout during ceremony generation must not expose enrollment options");
  } finally { rmSync(dir, { recursive: true, force: true }); }
});
