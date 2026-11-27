// Dedicated, loopback-only Stage IV HTTP adapter. This is deliberately NOT
// remoteGateway(), the local WebAdmin API, or a general C++ API proxy.
import { randomBytes, timingSafeEqual } from "node:crypto";
import type { IncomingMessage, ServerResponse } from "node:http";
import type { AuthenticationResponseJSON, RegistrationResponseJSON } from "@simplewebauthn/server";
import type { PublicBackend, PublicBackendGrant } from "./public-backend.ts";
import { createPublicPasswordPasskeyGate, PUBLIC_WEBADMIN_ORIGIN } from "./public-two-factor.ts";
import type { PasskeyCeremonies } from "./remote-webauthn.ts";
import type { PasskeyStore } from "./remote-passkeys.ts";
import type { RemoteRateLimitStore } from "./remote-rate-limit-store.ts";
import type { RemoteEvent } from "./remote-gateway.ts";

const prefix = "/stage4/v1/";
const internalHost = "127.0.0.1:4278";
const initialCookie = "__Host-dc24h_stage4_init";
const pendingCookie = "__Host-dc24h_stage4_pending";
const sessionCookie = "__Host-dc24h_stage4_session";
const ttl = { initial: 60_000, pending: 60_000, idle: 120_000, absolute: 600_000 };
const allowed = new Map<string, "GET" | "POST">([
  ["challenge", "GET"], ["login/options", "POST"], ["login/finish", "POST"],
  ["session", "GET"], ["summary", "GET"], ["logout", "POST"],
  ["devices", "GET"], ["devices/revoke", "POST"],
  ["devices/enroll/options", "POST"], ["devices/enroll/finish", "POST"],
  ["sessions/revoke", "POST"],
]);

type Session = {
  key: string; csrf: string; grant: PublicBackendGrant; actor: string;
  created: number; used: number; epoch: number;
};
type Timed<T> = T & { issued: number; expires: number };
type Pending = Timed<{ csrf: string }>;
type Enrollment = Timed<{ challenge: string; session: Session }>;
type PublicRateLimiter = Pick<RemoteRateLimitStore, "healthy" | "hit">;

export type PublicGatewayDependencies = {
  backend: PublicBackend;
  passkeys: { store: Pick<PasskeyStore, "healthy" | "epoch" | "forActor" | "revoke">;
    ceremonies: PasskeyCeremonies };
  limiter: PublicRateLimiter;
  // Synchronous, durable audit; a missing/unhealthy journal denies access.
  auditHealthy: () => boolean;
  audit: (event: RemoteEvent) => void;
  now?: () => number;
};

const random = () => randomBytes(32).toString("hex");
const matching = (actual: unknown, expected: string): boolean =>
  typeof actual === "string" && /^[0-9a-f]{64}$/.test(actual) &&
  timingSafeEqual(Buffer.from(actual, "hex"), Buffer.from(expected, "hex"));

function cookies(req: IncomingMessage, name: string): string {
  const raw = req.headers.cookie;
  if (typeof raw !== "string" || raw.length > 2048) return "";
  const matches = raw.split(";").map(s => s.trim()).filter(s => s.startsWith(name + "="));
  return matches.length === 1 ? matches[0]!.slice(name.length + 1) : "";
}
const cookie = (name: string, value: string, age: number) =>
  `${name}=${value}; Path=/; Max-Age=${age}; Secure; HttpOnly; SameSite=Strict`;
const clear = (name: string) => cookie(name, "", 0);
const jsonObject = (input: unknown): Record<string, unknown> => {
  if (!input || typeof input !== "object" || Array.isArray(input)) throw Error("invalid_json");
  return input as Record<string, unknown>;
};
const fieldsEqual = (fields: Record<string, unknown>, keys: string[]) =>
  Object.keys(fields).sort().join(",") === keys.sort().join(",");
const validActor = (actor: unknown): actor is string =>
  typeof actor === "string" && /^[1-9]\d{0,18}$/.test(actor) &&
  BigInt(actor) <= 9223372036854775807n;

async function receive(req: IncomingMessage, limit = 32768): Promise<Record<string, unknown>> {
  const length = req.headers["content-length"];
  if (req.headers["transfer-encoding"] || !/^[1-9]\d{0,4}$/.test(String(length)) ||
      Number(length) > limit || req.headers["content-type"] !== "application/json") throw Error("bad_body");
  const parts: Buffer[] = [];
  let count = 0;
  try {
    for await (const part of req) {
      const chunk = part as Buffer;
      parts.push(chunk);
      count += chunk.length;
      if (count > limit) throw Error("bad_body");
    }
    if (count !== Number(length)) throw Error("bad_body");
    const body = Buffer.concat(parts);
    try { return jsonObject(JSON.parse(body.toString("utf8")) as unknown); }
    finally { body.fill(0); }
  } finally { for (const part of parts) part.fill(0); }
}

export function publicGateway(dependencies: PublicGatewayDependencies) {
  const clock = dependencies.now ?? Date.now;
  const sessions = new Map<string, Session>();
  const challenges = new Map<string, Timed<{ csrf: string }>>();
  const pending = new Map<string, Pending>();
  const enrollments = new Map<string, Enrollment>();
  const prune = (at: number) => {
    for (const [key, entry] of sessions)
      if (at < entry.created || at < entry.used ||
          entry.created + ttl.absolute <= at || entry.used + ttl.idle <= at) sessions.delete(key);
    for (const [key, entry] of challenges) if (at < entry.issued || at >= entry.expires) challenges.delete(key);
    for (const [key, entry] of pending) if (at < entry.issued || at >= entry.expires) pending.delete(key);
    for (const [key, entry] of enrollments) if (at < entry.issued || at >= entry.expires) enrollments.delete(key);
  };
  const revokeActor = (actor: string) => {
    for (const [key, value] of sessions) if (value.actor === actor) sessions.delete(key);
  };
  const event = (value: RemoteEvent) => { dependencies.audit(value); };
  const gate = createPublicPasswordPasskeyGate<AuthenticationResponseJSON, PublicBackendGrant, Session,
    Awaited<ReturnType<PasskeyCeremonies["beginLogin"]>> & { challenge: string }>({
    verifyPassword: dependencies.backend.verifyPassword,
    passkeys: {
      beginLogin: async actor => dependencies.passkeys.ceremonies.beginLogin(actor),
      finishLogin: (actor, challenge, assertion, at) =>
        dependencies.passkeys.ceremonies.finishLogin(actor, challenge, assertion, at),
    },
    reauthorize: dependencies.backend.reauthorize,
    issueSession: async grant => {
      if (!dependencies.passkeys.store.healthy()) return null;
      const at = clock();
      const session: Session = {
        key: random(), csrf: random(), actor: grant.actor, grant,
        created: at, used: at, epoch: dependencies.passkeys.store.epoch(grant.actor),
      };
      if (sessions.size >= 8) sessions.delete(sessions.keys().next().value!);
      sessions.set(session.key, session);
      return session;
    },
    now: clock,
  });
  const reply = (res: ServerResponse, code: number, payload: unknown, setCookies: string[] = []) => {
    if (res.writableEnded || res.destroyed) return;
    res.writeHead(code, {
      "Content-Type": "application/json; charset=utf-8", "Cache-Control": "no-store",
      "Pragma": "no-cache", "X-Content-Type-Options": "nosniff",
      "Content-Security-Policy": "default-src 'none'; frame-ancestors 'none'",
      "X-Frame-Options": "DENY", "Referrer-Policy": "no-referrer",
      "Cross-Origin-Resource-Policy": "same-origin",
      ...(code === 429 ? { "Retry-After": "60" } : {}),
      ...(setCookies.length ? { "Set-Cookie": setCookies } : {}),
    });
    res.end(JSON.stringify(payload));
  };
  const checkCsrf = (req: IncomingMessage, value: string) => matching(req.headers["x-dc24h-stage4-csrf"], value);
  const current = (req: IncomingMessage, at: number): Session | null => {
    const key = cookies(req, sessionCookie);
    if (!/^[0-9a-f]{64}$/.test(key)) return null;
    const session = sessions.get(key);
    if (!session || at < session.created || at < session.used ||
        session.created + ttl.absolute <= at || session.used + ttl.idle <= at ||
        session.epoch !== dependencies.passkeys.store.epoch(session.actor)) {
      sessions.delete(key); return null;
    }
    return session;
  };
  const trusted = (req: IncomingMessage, post: boolean) => {
    const peer = req.socket.remoteAddress;
    if (peer !== "127.0.0.1" && peer !== "::ffff:127.0.0.1") return false;
    if (req.headers.host !== internalHost ||
        (req.headers.origin && req.headers.origin !== PUBLIC_WEBADMIN_ORIGIN) ||
        (post && req.headers.origin !== PUBLIC_WEBADMIN_ORIGIN) ||
        (req.headers["sec-fetch-site"] && !["same-origin", "none"].includes(String(req.headers["sec-fetch-site"])))) return false;
    return !Object.keys(req.headers).some(name =>
      ["authorization", "proxy", "proxy-authorization", "forwarded", "x-real-ip",
        "x-original-url", "x-rewrite-url", "x-dc24h-admin-grant",
        "x-dc24h-session", "x-dc24h-csrf"].includes(name) || name.startsWith("x-forwarded-"));
  };

  return async (req: IncomingMessage, res: ServerResponse) => {
    const path = req.url ?? "";
    const name = path.startsWith(prefix) ? path.slice(prefix.length) : "";
    const method = allowed.get(name);
    if (!method || req.method !== method) { reply(res, 404, { error: "not_found" }); return; }
    const post = method === "POST";
    if (!trusted(req, post)) { reply(res, 403, { error: "forbidden" }); return; }
    if (!post && (req.headers["transfer-encoding"] ||
        (req.headers["content-length"] && req.headers["content-length"] !== "0"))) {
      reply(res, 400, { error: "bad_request" }); return;
    }
    const now = clock();
    prune(now);
    try {
      if (!dependencies.auditHealthy() || !dependencies.limiter.healthy() ||
          !dependencies.passkeys.store.healthy()) throw Error("security_unavailable");
    } catch {
      sessions.clear(); challenges.clear(); pending.clear(); enrollments.clear();
      reply(res, 503, { error: "unavailable" }, [clear(sessionCookie)]); return;
    }
    const limited = (bucket: string, threshold: number) => {
      try { return dependencies.limiter.hit(bucket, threshold, now); }
      catch { throw Error("limiter_unavailable"); }
    };

    try {
      if (name === "challenge") {
        if (limited("stage4:challenge", 20) || challenges.size >= 64) {
          reply(res, 429, { error: "rate_limited" }); return;
        }
        const key = random(), csrf = random();
        challenges.set(key, { csrf, issued: now, expires: now + ttl.initial });
        reply(res, 200, { schema: 1, csrf }, [cookie(initialCookie, key, 60)]);
        return;
      }
      if (name === "login/options") {
        // Starting a replacement login invalidates the old browser session,
        // including its server-side authority, before accepting new factors.
        sessions.delete(cookies(req, sessionCookie));
        const init = cookies(req, initialCookie);
        const initial = challenges.get(init);
        challenges.delete(init);
        const baseCookie = [clear(initialCookie), clear(sessionCookie)];
        let fields: Record<string, unknown>;
        try { fields = await receive(req, 2048); }
        catch { reply(res, 400, { error: "bad_request" }, baseCookie); return; }
        if (!initial || clock() < initial.issued || clock() >= initial.expires ||
            !checkCsrf(req, initial.csrf) || !fieldsEqual(fields, ["actor_id", "password"]) ||
            !validActor(fields.actor_id) || typeof fields.password !== "string" ||
            !fields.password || fields.password.length > 1024) {
          event("login_refused"); reply(res, 403, { error: "login_refused" }, baseCookie); return;
        }
        if (limited("stage4:global", 12) || limited("stage4:actor:" + fields.actor_id, 4) ||
            pending.size >= 32) {
          reply(res, 429, { error: "rate_limited" }, baseCookie); return;
        }
        const started = await gate.begin(fields.actor_id, fields.password);
        if (!started || clock() >= initial.expires) {
          if (started) gate.cancel(started.ticket);
          event("login_refused"); reply(res, 403, { error: "login_refused" }, baseCookie); return;
        }
        const csrf = random(), issued = clock();
        pending.set(started.ticket, { csrf, issued, expires: issued + ttl.pending });
        reply(res, 200, { schema: 1, options: started.options, csrf },
          [...baseCookie, cookie(pendingCookie, started.ticket, 60)]);
        return;
      }
      if (name === "login/finish") {
        const ticket = cookies(req, pendingCookie);
        const entry = pending.get(ticket);
        pending.delete(ticket); // consume before reading the body or awaiting crypto
        const clearPending = [clear(pendingCookie)];
        let fields: Record<string, unknown>;
        try { fields = await receive(req); }
        catch { gate.cancel(ticket); reply(res, 400, { error: "bad_request" }, clearPending); return; }
        if (!entry || clock() < entry.issued || clock() >= entry.expires ||
            !checkCsrf(req, entry.csrf) || !fieldsEqual(fields, ["response"]) ||
            !fields.response || typeof fields.response !== "object" || Array.isArray(fields.response)) {
          gate.cancel(ticket); event("passkey_login_refused");
          reply(res, 403, { error: "login_refused" }, clearPending); return;
        }
        const session = await gate.finish(ticket, fields.response as AuthenticationResponseJSON);
        if (!session || clock() < entry.issued || clock() >= entry.expires ||
            !sessions.has(session.key) || !await dependencies.backend.reauthorize(session.grant)) {
          if (session) sessions.delete(session.key);
          event("passkey_login_refused"); reply(res, 403, { error: "login_refused" }, clearPending); return;
        }
        try { event("passkey_login_ok"); event("login_ok"); }
        catch { sessions.delete(session.key); throw Error("audit_unavailable"); }
        reply(res, 200, { schema: 1, authenticated: true, role: session.grant.role,
          csrf: session.csrf, expires_in: 600 },
        [...clearPending, cookie(sessionCookie, session.key, 600)]);
        return;
      }
      const session = current(req, clock());
      if (!session) { reply(res, 401, { error: "unauthorized" }, [clear(sessionCookie)]); return; }
      if (!await dependencies.backend.reauthorize(session.grant) ||
          sessions.get(session.key) !== session || session.epoch !== dependencies.passkeys.store.epoch(session.actor)) {
        sessions.delete(session.key); event("session_revoked");
        reply(res, 403, { error: "forbidden" }, [clear(sessionCookie)]); return;
      }
      if (post && !checkCsrf(req, session.csrf)) {
        reply(res, 403, { error: "csrf_required" }); return;
      }
      if (name === "logout" || name === "sessions/revoke") {
        const body = await receive(req, 2048);
        if (!fieldsEqual(body, [])) { reply(res, 400, { error: "bad_request" }); return; }
        if (name === "sessions/revoke") {
          if (clock() - session.created > 60_000) { reply(res, 403, { error: "fresh_login_required" }); return; }
          revokeActor(session.actor);
          event("session_revoked");
        } else { sessions.delete(session.key); event("logout"); }
        reply(res, 200, { schema: 1, authenticated: false }, [clear(sessionCookie)]);
        return;
      }
      if (name === "devices" || name === "devices/revoke" || name.startsWith("devices/enroll/")) {
        const store = dependencies.passkeys.store;
        if (name === "devices") {
          reply(res, 200, { schema: 1, devices: store.forActor(session.actor).map(c => ({
            id: c.id, name: c.name, device_type: c.deviceType, backed_up: c.backedUp,
            created_at: c.createdAt, last_used_at: c.lastUsedAt,
          })) });
          return;
        }
        if (clock() - session.created > 60_000) {
          reply(res, 403, { error: "fresh_login_required" }); return;
        }
        if (limited("stage4:device:" + session.actor, 10)) {
          reply(res, 429, { error: "rate_limited" }); return;
        }
        if (name === "devices/revoke") {
          const body = await receive(req, 2048);
          if (!fieldsEqual(body, ["id"]) || typeof body.id !== "string" ||
              !store.forActor(session.actor).some(c => c.id === body.id) ||
              !await dependencies.backend.reauthorize(session.grant)) {
            reply(res, 403, { error: "device_refused" }); return;
          }
          event("passkey_revoke_intent");
          if (!store.revoke(session.actor, body.id, clock())) throw Error("device_revoke_failed");
          revokeActor(session.actor);
          event("passkey_revoked");
          reply(res, 200, { schema: 1, revoked: true }, [clear(sessionCookie)]);
          return;
        }
        if (name === "devices/enroll/options") {
          const body = await receive(req, 2048);
          if (!fieldsEqual(body, []) || enrollments.has(session.key) || enrollments.size >= 32) {
            reply(res, 403, { error: "enrollment_refused" }); return;
          }
          const options = await dependencies.passkeys.ceremonies.beginEnrollment(session.actor);
          if (!await dependencies.backend.reauthorize(session.grant) || sessions.get(session.key) !== session)
            throw Error("enrollment_revoked");
          const issued = clock();
          enrollments.set(session.key, { challenge: options.challenge, session,
            issued, expires: issued + ttl.pending });
          reply(res, 200, { schema: 1, options });
          return;
        }
        if (name === "devices/enroll/finish") {
          const enrollment = enrollments.get(session.key);
          enrollments.delete(session.key);
          const body = await receive(req);
          if (!enrollment || clock() < enrollment.issued || clock() >= enrollment.expires ||
              enrollment.session !== session || !fieldsEqual(body, ["response"]) ||
              !body.response || typeof body.response !== "object") {
            reply(res, 403, { error: "enrollment_refused" }); return;
          }
          event("passkey_enroll_intent");
          const done = await dependencies.passkeys.ceremonies.finishEnrollment(
            session.actor, enrollment.challenge, body.response as RegistrationResponseJSON, clock(),
            async () => sessions.get(session.key) === session && clock() < enrollment.expires &&
              session.epoch === store.epoch(session.actor) &&
              await dependencies.backend.reauthorize(session.grant));
          // Revoke all sessions even when persistence was followed by an audit
          // failure; this avoids leaving pre-enrollment authority alive.
          revokeActor(session.actor);
          if (!done) { reply(res, 403, { error: "enrollment_refused" }, [clear(sessionCookie)]); return; }
          event("passkey_enrolled");
          reply(res, 200, { schema: 1, enrolled: true }, [clear(sessionCookie)]);
          return;
        }
      }
      // Async data sampling always gets a second fresh backend grant check.
      if (name === "summary") {
        const data = await dependencies.backend.summary(session.grant);
        const completedAt = clock();
        if (sessions.get(session.key) !== session || completedAt < session.created ||
            completedAt < session.used || session.created + ttl.absolute <= completedAt ||
            session.used + ttl.idle <= completedAt ||
            !await dependencies.backend.reauthorize(session.grant) ||
            session.epoch !== dependencies.passkeys.store.epoch(session.actor)) throw Error("session_revoked");
        session.used = clock();
        reply(res, 200, { schema: 1, summary: data });
        return;
      }
      session.used = clock();
      reply(res, 200, { schema: 1, authenticated: true, role: session.grant.role,
        csrf: session.csrf, expires_in: Math.max(0, Math.floor(Math.min(
          session.created + ttl.absolute - session.used, ttl.idle) / 1000)) });
    } catch {
      sessions.clear();
      reply(res, 503, { error: "unavailable" }, [clear(sessionCookie)]);
    }
  };
}
