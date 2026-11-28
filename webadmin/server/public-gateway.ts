// Dedicated Stage IV password + verified-email-code gateway. Never mount the
// legacy localhost remote gateway or expose the C++ management API here.
import { randomBytes, timingSafeEqual } from "node:crypto";
import { isIP } from "node:net";
import { isSafeMailbox } from "./public-msmtp.ts";
import { opaque, stage4SessionDigest, validToken } from "./stage4-store.ts";
import type { Stage4Store } from "./stage4-store.ts";
import type { IncomingMessage, ServerResponse } from "node:http";
import { PublicUpstreamError } from "./public-backend.ts";
import type { HubRead, PublicBackend, PublicBackendGrant } from "./public-backend.ts";
import { readAdminInput } from "./public-admin.ts";
import { createPublicEmailCodeGate, PUBLIC_WEBADMIN_ORIGIN } from "./public-email-code.ts";
import type { RemoteRateLimitStore } from "./remote-rate-limit-store.ts";
import type { RemoteEvent } from "./remote-gateway.ts";

const prefix = "/stage4/v1/";
const internalHost = "127.0.0.1:4278";
const initialCookie = "__Host-dc24h_stage4_email_init";
const pendingCookie = "__Host-dc24h_stage4_email_pending";
const sessionCookie = "__Host-dc24h_stage4_session";
const ttl = { initial: 60_000, pending: 300_000, idle: 120_000, absolute: 600_000 };
const routes = new Map<string, "GET" | "POST">([
  ["challenge", "GET"], ["login/send", "POST"], ["login/verify", "POST"],
  ["session", "GET"], ["summary", "GET"], ["logout", "POST"],
  ["sessions/revoke", "POST"],
]);
type Session = {
  key: string; actor: string; csrf: string; grant: PublicBackendGrant;
  created: number; used: number;
};
type Pending = { csrf: string; issued: number; expires: number };
type PublicRateLimiter = Pick<RemoteRateLimitStore, "healthy" | "hit">;
export type PublicGatewayDependencies = {
  backend: Pick<PublicBackend, "verifyNamedPassword" | "verifyPassword" | "reauthorize" | "summary">;
  email: { healthy(): boolean; forActor(actor: string): string | null };
  sendCode(email: string, code: string): Promise<boolean>;
  limiter: PublicRateLimiter;
  auditHealthy(): boolean;
  audit(event: RemoteEvent): void;
  now?: () => number;
};

const secret = () => randomBytes(32).toString("hex");
const sameSecret = (actual: unknown, expected: string) =>
  typeof actual === "string" && /^[0-9a-f]{64}$/.test(actual) &&
  timingSafeEqual(Buffer.from(actual, "hex"), Buffer.from(expected, "hex"));
const cookie = (name: string, value: string, age: number) =>
  `${name}=${value}; Path=/; Max-Age=${age}; Secure; HttpOnly; SameSite=Strict`;
const clear = (name: string) => cookie(name, "", 0);
const readCookie = (req: IncomingMessage, name: string): string => {
  const raw = req.headers.cookie;
  if (typeof raw !== "string" || raw.length > 2048) return "";
  const matching = raw.split(";").map(item => item.trim()).filter(item => item.startsWith(name + "="));
  return matching.length === 1 ? matching[0]!.slice(name.length + 1) : "";
};
const exactFields = (object: Record<string, unknown>, names: string[]) =>
  Object.keys(object).sort().join(",") === names.sort().join(",");
const validActor = (value: unknown): value is string =>
  typeof value === "string" && /^[1-9]\d{0,18}$/.test(value) && BigInt(value) <= 9223372036854775807n;

async function body(req: IncomingMessage): Promise<Record<string, unknown>> {
  const length = req.headers["content-length"];
  if (req.headers["transfer-encoding"] || !/^[1-9]\d{0,3}$/.test(String(length)) ||
      Number(length) > 2048 || req.headers["content-type"] !== "application/json") throw Error("bad_body");
  const chunks: Buffer[] = [];
  let bytes = 0;
  try {
    for await (const part of req) {
      const data = part as Buffer;
      chunks.push(data); bytes += data.length;
      if (bytes > 2048) throw Error("bad_body");
    }
    if (bytes !== Number(length)) throw Error("bad_body");
    const full = Buffer.concat(chunks);
    try {
      const parsed: unknown = JSON.parse(full.toString("utf8"));
      if (!parsed || typeof parsed !== "object" || Array.isArray(parsed)) throw Error("bad_body");
      return parsed as Record<string, unknown>;
    } finally { full.fill(0); }
  } finally { for (const part of chunks) part.fill(0); }
}

export function publicGateway(deps: PublicGatewayDependencies) {
  const clock = deps.now ?? Date.now;
  const sessions = new Map<string, Session>();
  const challenges = new Map<string, Pending>();
  const pending = new Map<string, Pending>();
  const prune = (at: number) => {
    for (const [id, s] of sessions) if (at < s.created || at < s.used ||
      s.created + ttl.absolute <= at || s.used + ttl.idle <= at) sessions.delete(id);
    for (const [id, c] of challenges) if (at < c.issued || at >= c.expires) challenges.delete(id);
    for (const [id, c] of pending) if (at < c.issued || at >= c.expires) pending.delete(id);
  };
  const revokeActor = (actor: string) => {
    for (const [id, s] of sessions) if (s.actor === actor) sessions.delete(id);
  };
  const gate = createPublicEmailCodeGate<PublicBackendGrant, Session>({
    verifyPassword: deps.backend.verifyPassword,
    verifiedEmail: actor => deps.email.forActor(actor),
    sendCode: deps.sendCode,
    reauthorize: deps.backend.reauthorize,
    issueSession: async grant => {
      if (!deps.email.healthy()) return null;
      const now = clock();
      const session: Session = { key: secret(), csrf: secret(), actor: grant.actor,
        grant, created: now, used: now };
      if (sessions.size >= 8) sessions.delete(sessions.keys().next().value!);
      sessions.set(session.key, session);
      return session;
    },
    now: clock,
  });
  const reply = (res: ServerResponse, status: number, data: unknown, cookies: string[] = []) => {
    if (res.writableEnded || res.destroyed) return;
    res.writeHead(status, {
      "Content-Type": "application/json; charset=utf-8", "Cache-Control": "no-store",
      "Pragma": "no-cache", "X-Content-Type-Options": "nosniff",
      "Content-Security-Policy": "default-src 'none'; frame-ancestors 'none'",
      "X-Frame-Options": "DENY", "Referrer-Policy": "no-referrer",
      "Cross-Origin-Resource-Policy": "same-origin",
      ...(status === 429 ? { "Retry-After": "60" } : {}),
      ...(cookies.length ? { "Set-Cookie": cookies } : {}),
    });
    res.end(JSON.stringify(data));
  };
  const trusted = (req: IncomingMessage, post: boolean) => {
    if (!["127.0.0.1", "::ffff:127.0.0.1"].includes(req.socket.remoteAddress ?? "") ||
        req.headers.host !== internalHost ||
        (req.headers.origin && req.headers.origin !== PUBLIC_WEBADMIN_ORIGIN) ||
        (post && req.headers.origin !== PUBLIC_WEBADMIN_ORIGIN) ||
        (req.headers["sec-fetch-site"] &&
          !["same-origin", "none"].includes(String(req.headers["sec-fetch-site"])))) return false;
    return !Object.keys(req.headers).some(name =>
      ["authorization", "proxy", "proxy-authorization", "forwarded", "x-real-ip",
        "x-original-url", "x-rewrite-url", "x-dc24h-admin-grant",
        "x-dc24h-session", "x-dc24h-csrf"].includes(name) || name.startsWith("x-forwarded-"));
  };
  const active = (req: IncomingMessage, at: number): Session | null => {
    const key = readCookie(req, sessionCookie);
    if (!/^[0-9a-f]{64}$/.test(key)) return null;
    const session = sessions.get(key);
    if (!session || at < session.created || at < session.used ||
        session.created + ttl.absolute <= at || session.used + ttl.idle <= at) {
      sessions.delete(key); return null;
    }
    return session;
  };
  const csrf = (req: IncomingMessage, expected: string) =>
    sameSecret(req.headers["x-dc24h-stage4-csrf"], expected);

  return async (req: IncomingMessage, res: ServerResponse) => {
    const raw = req.url ?? "";
    const endpoint = raw.startsWith(prefix) ? raw.slice(prefix.length) : "";
    const method = routes.get(endpoint);
    if (!method || req.method !== method) { reply(res, 404, { error: "not_found" }); return; }
    if (!trusted(req, method === "POST")) { reply(res, 403, { error: "forbidden" }); return; }
    if (method === "GET" && (req.headers["transfer-encoding"] ||
        (req.headers["content-length"] && req.headers["content-length"] !== "0"))) {
      reply(res, 400, { error: "bad_request" }); return;
    }
    const now = clock(); prune(now);
    try {
      if (!deps.auditHealthy() || !deps.limiter.healthy() || !deps.email.healthy())
        throw Error("security_unavailable");
    } catch {
      sessions.clear(); challenges.clear(); pending.clear();
      reply(res, 503, { error: "unavailable" }, [clear(sessionCookie)]); return;
    }
    const limited = (bucket: string, limit: number) => {
      try { return deps.limiter.hit(bucket, limit, now); }
      catch { throw Error("limiter_unavailable"); }
    };
    try {
      if (endpoint === "challenge") {
        if (limited("stage4:challenge", 20) || challenges.size >= 64) {
          reply(res, 429, { error: "rate_limited" }); return;
        }
        const id = secret(), token = secret();
        challenges.set(id, { csrf: token, issued: now, expires: now + ttl.initial });
        reply(res, 200, { schema: 1, csrf: token }, [cookie(initialCookie, id, 60)]);
        return;
      }
      if (endpoint === "login/send") {
        sessions.delete(readCookie(req, sessionCookie));
        const init = readCookie(req, initialCookie);
        const original = challenges.get(init);
        challenges.delete(init);
        const oldCookies = [clear(initialCookie), clear(sessionCookie)];
        let data: Record<string, unknown>;
        try { data = await body(req); }
        catch { reply(res, 400, { error: "bad_request" }, oldCookies); return; }
        if (!original || clock() < original.issued || clock() >= original.expires ||
            !csrf(req, original.csrf) || !exactFields(data, ["actor_id", "password"]) ||
            !validActor(data.actor_id) || typeof data.password !== "string" ||
            !data.password || data.password.length > 1024) {
          deps.audit("login_refused");
          reply(res, 403, { error: "login_refused" }, oldCookies); return;
        }
        if (limited("stage4:global", 12) || limited("stage4:actor:" + data.actor_id, 4) ||
            pending.size >= 32) {
          reply(res, 429, { error: "rate_limited" }, oldCookies); return;
        }
        const sent = await gate.begin(data.actor_id, data.password);
        if (!sent || clock() >= original.expires) {
          if (sent) gate.cancel(sent.ticket);
          deps.audit("login_refused");
          reply(res, 403, { error: "login_refused" }, oldCookies); return;
        }
        const issued = clock(), pendingCsrf = secret();
        pending.set(sent.ticket, { csrf: pendingCsrf, issued, expires: issued + ttl.pending });
        reply(res, 200, { schema: 1, sent: true, csrf: pendingCsrf,
          expires_in: Math.min(sent.expires_in, 300) },
          [...oldCookies, cookie(pendingCookie, sent.ticket, 300)]);
        return;
      }
      if (endpoint === "login/verify") {
        const ticket = readCookie(req, pendingCookie);
        const attempt = pending.get(ticket);
        let data: Record<string, unknown>;
        try { data = await body(req); }
        catch { reply(res, 400, { error: "bad_request" }); return; }
        if (!attempt || clock() < attempt.issued || clock() >= attempt.expires ||
            !csrf(req, attempt.csrf) || !exactFields(data, ["code"]) ||
            typeof data.code !== "string" || !/^[0-9]{8}$/.test(data.code)) {
          deps.audit("mfa_refused"); reply(res, 403, { error: "code_refused" }); return;
        }
        if (limited("stage4:verify", 60)) {
          reply(res, 429, { error: "rate_limited" }); return;
        }
        const session = await gate.finish(ticket, data.code);
        if (!session || clock() >= attempt.expires ||
            sessions.get(session.key) !== session || !await deps.backend.reauthorize(session.grant)) {
          if (session) sessions.delete(session.key);
          deps.audit("mfa_refused"); reply(res, 403, { error: "code_refused" }); return;
        }
        pending.delete(ticket);
        try { deps.audit("mfa_ok"); deps.audit("login_ok"); }
        catch { sessions.delete(session.key); throw Error("audit_unavailable"); }
        reply(res, 200, { schema: 1, authenticated: true, role: session.grant.role,
          csrf: session.csrf, expires_in: 600 },
          [clear(pendingCookie), cookie(sessionCookie, session.key, 600)]);
        return;
      }
      const session = active(req, clock());
      if (!session) { reply(res, 401, { error: "unauthorized" }, [clear(sessionCookie)]); return; }
      if (!await deps.backend.reauthorize(session.grant) || sessions.get(session.key) !== session) {
        sessions.delete(session.key); deps.audit("session_revoked");
        reply(res, 403, { error: "forbidden" }, [clear(sessionCookie)]); return;
      }
      if (method === "POST" && !csrf(req, session.csrf)) {
        reply(res, 403, { error: "csrf_required" }); return;
      }
      if (endpoint === "logout" || endpoint === "sessions/revoke") {
        const data = await body(req);
        if (!exactFields(data, [])) { reply(res, 400, { error: "bad_request" }); return; }
        if (endpoint === "sessions/revoke") {
          if (clock() - session.created > 60_000) {
            reply(res, 403, { error: "fresh_login_required" }); return;
          }
          revokeActor(session.actor); deps.audit("session_revoked");
        } else { sessions.delete(session.key); deps.audit("logout"); }
        reply(res, 200, { schema: 1, authenticated: false }, [clear(sessionCookie)]);
        return;
      }
      if (endpoint === "summary") {
        const summary = await deps.backend.summary(session.grant);
        const at = clock();
        if (sessions.get(session.key) !== session || at < session.created || at < session.used ||
            session.created + ttl.absolute <= at || session.used + ttl.idle <= at ||
            !await deps.backend.reauthorize(session.grant)) throw Error("session_revoked");
        session.used = clock();
        reply(res, 200, { schema: 1, summary }); return;
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

const registrationRoutes = new Map([...routes,
  ["register/send", "POST"], ["register/verify", "POST"], ["password/change", "POST"],
  ["identity-summary", "GET"], ["admin", "POST"]]);
const hubReads = new Map<string, HubRead>([
  ["hub/health", "health"], ["hub/ready", "ready"],
  ["hub/status", "status"], ["hub/users", "users"],
]);
for (const path of hubReads.keys()) registrationRoutes.set(path, "GET");
const usernameOK = (value: unknown): value is string =>
  typeof value === "string" && /^[A-Za-z0-9][A-Za-z0-9_.-]{0,63}$/.test(value);
const passwordOK = (value: unknown): value is string =>
  typeof value === "string" && [...value].length >= 15 && Buffer.byteLength(value) <= 1024 &&
  !/[\u0000-\u001f\u007f\ud800-\udfff]/u.test(value);
const strongPassword = (value: unknown): value is string =>
  passwordOK(value) && new Set(value).size >= 8 &&
  !/^(?:password|administrator|changeme|qwerty|1234)[0-9!@#$._-]*$/i.test(value);

type DatabaseGateway = {
  db: Stage4Store;
  backend: Pick<PublicBackend, "prove" | "prepare" | "change" | "health" |
    "identitySummary" | "hubRead" | "adminOperation">;
  sendCode(email: string, code: string): Promise<boolean>;
  now?: () => number;
};

export function databasePublicGateway({ db, backend, sendCode, now = Date.now }: DatabaseGateway) {
  const initials = new Map<string, number>();
  let registrationSends = 0;
  const reply = (res: ServerResponse, status: number, data: object, cookies: string[] = []) => {
    if (res.writableEnded || res.destroyed) return;
    res.writeHead(status, {
      "Content-Type": "application/json; charset=utf-8", "Cache-Control": "no-store",
      "X-Content-Type-Options": "nosniff", "X-Frame-Options": "DENY",
      "Referrer-Policy": "no-referrer", "Cross-Origin-Resource-Policy": "same-origin",
      "Content-Security-Policy": "default-src 'none'; frame-ancestors 'none'",
      ...(status === 429 ? { "Retry-After": "60" } : {}),
      ...(cookies.length ? { "Set-Cookie": cookies } : {}),
    });
    res.end(JSON.stringify(data));
  };
  const client = (req: IncomingMessage, method: string) => {
    if (req.socket.remoteAddress !== "127.0.0.1" || req.headers.host !== internalHost ||
        (req.headers.origin !== undefined && req.headers.origin !== PUBLIC_WEBADMIN_ORIGIN) ||
        (method === "POST" && req.headers.origin !== PUBLIC_WEBADMIN_ORIGIN) ||
        (req.headers["sec-fetch-site"] !== undefined &&
          !["same-origin", "none"].includes(String(req.headers["sec-fetch-site"]))) ||
        Object.keys(req.headers).some(name => name.startsWith("x-forwarded-") ||
          ["forwarded", "x-real-ip", "authorization", "x-dc24h-admin-grant",
           "x-dc24h-session", "x-dc24h-csrf"].includes(name))) return null;
    const ip = req.headers["x-dc24h-stage4-ip"];
    return typeof ip === "string" && isIP(ip) ? ip : null;
  };
  const minutes = 60_000;
  const limited = (endpoint: string, ip: string, global: number, perIp: number,
    extra: [string, number, number][] = []) => db.allowed([
      [endpoint + ":all", global, minutes], [endpoint + ":ip:" + ip, perIp, minutes], ...extra,
    ]);
  const outputSession = (session: { token: string; identity: { role: string } }) => ({
    schema: 1, authenticated: true, role: session.identity.role, csrf: db.csrf("session", session.token),
  });
  const deliver = async (ticket: string, email: string, code: string) => {
    const ok = await sendCode(email, code).catch(() => false);
    return db.delivery(ticket, ok);
  };

  return async (req: IncomingMessage, res: ServerResponse) => {
    const endpoint = req.url?.startsWith(prefix) ? req.url.slice(prefix.length) : "";
    const method = registrationRoutes.get(endpoint);
    if (!method || req.method !== method) return reply(res, 404, { error: "not_found" });
    const ip = client(req, method);
    if (!ip) return reply(res, 403, { error: "forbidden" });
    if (method === "GET" && (req.headers["transfer-encoding"] ||
        (req.headers["content-length"] && req.headers["content-length"] !== "0")))
      return reply(res, 400, { error: "bad_request" });
    const supplied = req.headers["x-dc24h-stage4-csrf"];
    const rateLimited = () => reply(res, 429, { error: "rate_limited" });
    try {
      if (endpoint === "challenge") {
        const t = now();
        for (const [key, expiry] of initials) if (expiry <= t) initials.delete(key);
        if (!await limited(endpoint, ip, 120, 20) || initials.size >= 128) return rateLimited();
        const token = opaque();
        initials.set(token, t + minutes);
        return reply(res, 200, { schema: 1, csrf: db.csrf("init", token) }, [cookie(initialCookie, token, 60)]);
      }
      if (endpoint === "login/send" || endpoint === "register/send") {
        const init = readCookie(req, initialCookie);
        const expiry = initials.get(init) ?? 0;
        if (expiry <= now() || !db.isCsrf("init", init, supplied))
          return reply(res, 403, { error: "csrf_required" });
        initials.delete(init);
        const registration = endpoint === "register/send";
        const data = await body(req).catch(() => null);
        const fields = registration ? ["username", "password", "email"] : ["username", "password"];
        if (!data || !exactFields(data, fields) || !usernameOK(data.username) ||
            (registration ? !strongPassword(data.password) || !isSafeMailbox(data.email) : !passwordOK(data.password)))
          return reply(res, 400, { error: "bad_request" });
        const nameKey = data.username.toLowerCase();
        const email = registration ? (data.email as string).toLowerCase() : "";
        const extras: [string, number, number][] = [[endpoint + ":name:" + nameKey, 4, minutes]];
        if (registration) extras.push([endpoint + ":email:" + email, 2, 5 * minutes]);
        if (!await limited(endpoint, ip, 16, 6, extras)) return rateLimited();
        if (registration) {
          if (registrationSends >= 4) return rateLimited();
          const verifier = await backend.prepare(data.username, data.password as string);
          const attempt = verifier && email !== "admin@dc24h.eu" && nameKey !== "administrator" ?
            await db.beginRegistration(data.username, email, verifier) : null;
          const token = attempt?.ticket ?? opaque();
          // The HTTP result is identical for duplicate names and email addresses.
          // A challenge cannot verify until msmtp has accepted the message.
          if (attempt) {
            registrationSends++;
            void deliver(attempt.ticket, email, attempt.code).catch(() => {}).finally(() => registrationSends--);
          }
          return reply(res, 200, { schema: 1, csrf: db.csrf("register", token), expires_in: 300 },
            [clear(initialCookie), cookie(pendingCookie, token, 300)]);
        }
        const identity = await backend.prove(data.username, data.password as string);
        const attempt = identity && await db.beginLogin(identity);
        if (!attempt || !await deliver(attempt.ticket, attempt.email, attempt.code))
          return reply(res, 403, { error: "login_refused" }, [clear(initialCookie), clear(pendingCookie)]);
        return reply(res, 200, { schema: 1, sent: true, csrf: db.csrf("login", attempt.ticket),
          expires_in: Math.ceil((attempt.expires - now()) / 1000) },
          [clear(initialCookie), cookie(pendingCookie, attempt.ticket, 300)]);
      }
      if (endpoint === "login/verify" || endpoint === "register/verify") {
        const registration = endpoint === "register/verify";
        const ticket = readCookie(req, pendingCookie);
        if (!validToken(ticket) || !db.isCsrf(registration ? "register" : "login", ticket, supplied))
          return reply(res, 403, { error: "csrf_required" });
        const data = await body(req).catch(() => null);
        if (!data || !exactFields(data, ["code"]) || typeof data.code !== "string" || !/^\d{8}$/.test(data.code))
          return reply(res, 400, { error: "bad_request" });
        if (!await limited(endpoint, ip, 120, 25)) return rateLimited();
        const approved = await db.verify(ticket, data.code, registration);
        if (!approved) return reply(res, 403, { error: "code_refused" });
        if ("registered" in approved)
          return reply(res, 200, { schema: 1, registered: true }, [clear(pendingCookie)]);
        return reply(res, 200, outputSession(approved), [clear(pendingCookie), cookie(sessionCookie, approved.token, 600)]);
      }

      const token = readCookie(req, sessionCookie);
      const active = validToken(token) && await db.session(token);
      if (!active) return reply(res, 401, { error: "unauthorized" }, [clear(sessionCookie)]);
      const hubRead = hubReads.get(endpoint);
      if (endpoint === "identity-summary" || endpoint === "admin" || hubRead) {
        if (!["owner", "administrator"].includes(active.identity.role))
          return reply(res, 403, { error: "forbidden" });
        const admin = endpoint === "admin";
        if (admin && !db.isCsrf("session", token, supplied)) return reply(res, 403, { error: "csrf_required" });
        const input = admin ? readAdminInput(await body(req).catch(() => ({}))) : null;
        if (admin && !input) return reply(res, 400, { error: "bad_request" });
        if (!await limited(endpoint, ip, admin ? 120 : 300, admin ? 20 : 30,
          [[endpoint + ":actor:" + active.identity.id, admin ? 20 : 30, minutes]])) return rateLimited();
        let result: unknown;
        try {
          if (input) {
            result = input.parse(await backend.adminOperation(input.operation, input.values, stage4SessionDigest(token)));
          } else result = hubRead ? await backend.hubRead(hubRead) : await backend.identitySummary();
        } catch (error) {
          if (error instanceof PublicUpstreamError) {
            if (error.status === 401) return reply(res, 401, { error: "unauthorized" }, [clear(sessionCookie)]);
            if (error.status === 429) return rateLimited();
            if ([400, 403, 404, 409].includes(error.status)) return reply(res, error.status, { error: "operation_refused" });
            if (hubRead === "ready" && error.status === 503) return reply(res, 503, { error: "not_ready_or_unavailable" });
          }
          return reply(res, 503, { error: input && "proof" in input.values && input.operation !== "audit.list" ?
            "outcome_unknown" : "unavailable" });
        }
        // Recheck after the asynchronous C++ request: password/role changes or
        // revocation during the read must invalidate the result.
        const verified = await db.session(token);
        if (!verified || verified.identity.id !== active.identity.id ||
            verified.identity.revision !== active.identity.revision ||
            verified.identity.role !== active.identity.role)
          return reply(res, 401, { error: "unauthorized" }, [clear(sessionCookie)]);
        await db.touch(token);
        return reply(res, 200, { schema: 1, data: result });
      }
      if (method === "POST" && !db.isCsrf("session", token, supplied))
        return reply(res, 403, { error: "csrf_required" });
      if (endpoint === "summary") {
        if (!["owner", "administrator"].includes(active.identity.role))
          return reply(res, 403, { error: "forbidden" });
        const available = await backend.health();
        if (!await db.session(token)) return reply(res, 401, { error: "unauthorized" }, [clear(sessionCookie)]);
        await db.touch(token);
        return reply(res, 200, { schema: 1, summary: { account_service: available ? "available" : "unavailable" } });
      }
      if (endpoint === "session") {
        await db.touch(token);
        return reply(res, 200, outputSession(active));
      }
      const data = await body(req).catch(() => null);
      if (endpoint === "password/change") {
        if (!data || !exactFields(data, ["current", "password"]) || !passwordOK(data.current) ||
            !strongPassword(data.password)) return reply(res, 400, { error: "bad_request" });
        if (!await limited(endpoint, ip, 8, 4, [["change:account:" + active.identity.id, 3, minutes]]))
          return rateLimited();
        if (!await db.session(token) || !await backend.change(active.identity, data.current, data.password))
          return reply(res, 403, { error: "password_refused" });
        await db.revoke(token, active.identity.id, "password_change");
      } else {
        if (!data || !exactFields(data, [])) return reply(res, 400, { error: "bad_request" });
        if (endpoint === "sessions/revoke" && now() - active.issued >= minutes)
          return reply(res, 403, { error: "fresh_login_required" });
        await db.revoke(token, active.identity.id, endpoint === "logout" ? "logout" : "revoke_sessions");
      }
      return reply(res, 200, { schema: 1, authenticated: false }, [clear(sessionCookie), clear(pendingCookie)]);
    } catch {
      return reply(res, 503, { error: "unavailable" });
    }
  };
}
