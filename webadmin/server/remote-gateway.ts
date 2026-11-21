// A separate, loopback-only session gateway. Never mount this handler in Vite
// or in the public-facing WebAdmin adapter.
import { randomBytes, timingSafeEqual } from "node:crypto";
import { request } from "node:http";
import type { IncomingMessage, ServerResponse } from "node:http";
import { parseAccess } from "../src/lib/access.ts";
import { parseManagement } from "../src/lib/management.ts";
import { parseSnapshot } from "../src/lib/status.ts";

type Credentials = { socket: string; token: string };
export type RemoteEvent = "login_ok" | "login_refused" | "logout" | "session_revoked" | "route_refused" | "api_unavailable";
export interface RemoteOptions {
  origin: string;
  now?: () => number;
  audit?: (event: RemoteEvent) => void;
}
type Session = {
  actor: string;
  grant: string;
  apiSession: string;
  apiCsrf: string;
  csrf: string;
  created: number;
  used: number;
};
type Challenge = { token: string; expires: number };
type Budget = { count: number; starts: number };

const prefix = "/remote/v1/";
const cookiePath = "/remote/v1";
const maxSessions = 4;
const idleTime = 2 * 60_000;
const absoluteTime = 10 * 60_000;
const challengeTime = 2 * 60_000;
const maxBody = 2048;
const safePath = new Set(["challenge", "login", "session", "summary", "logout"]);
const object = (data: unknown): Record<string, unknown> => {
  if (!data || typeof data !== "object" || Array.isArray(data)) throw new Error("Invalid response");
  return data as Record<string, unknown>;
};
const sameSecret = (a: string, b: string): boolean =>
  /^[0-9a-f]{64}$/.test(a) && /^[0-9a-f]{64}$/.test(b) &&
  timingSafeEqual(Buffer.from(a, "hex"), Buffer.from(b, "hex"));
const loopback = (peer: string | undefined): boolean =>
  peer === "127.0.0.1" || peer === "::1" || peer === "::ffff:127.0.0.1";
const untrustedProxyHeader = (req: IncomingMessage): boolean =>
  Object.keys(req.headers).some((name) =>
    name === "authorization" || name === "proxy" || name === "proxy-authorization" ||
    name === "forwarded" || name === "x-real-ip" || name === "x-original-url" ||
    name === "x-rewrite-url" || name.startsWith("x-forwarded-"));
const cookie = (req: IncomingMessage, key: string): string => {
  const raw = req.headers.cookie;
  if (typeof raw !== "string" || raw.length > 1024) return "";
  const found = raw.split(";").map((part) => part.trim()).filter((part) => part.startsWith(key + "="));
  if (found.length !== 1) return "";
  return found[0]!.slice(key.length + 1);
};
const setCookie = (key: string, value: string, age: number): string =>
  key + "=" + value + "; Path=" + cookiePath + "; HttpOnly; Secure; SameSite=Strict; Max-Age=" + age;

// Fixed upstream paths: a grant never becomes a general-purpose HTTP proxy.
async function upstream(config: Credentials, path: "/api/v1/admin/access" | "/api/v1/management" | "/api/v1/status",
  method: "GET" | "POST", body = "", session?: Session): Promise<unknown> {
  return new Promise((resolve, reject) => {
    const headers: Record<string, string> = {
      Host: "localhost", Authorization: "Bearer " + config.token,
      Accept: "application/json", Connection: "close",
    };
    if (session) {
      headers["X-DC24H-Admin-Grant"] = session.grant;
      headers["X-DC24H-Session"] = session.apiSession;
      headers["X-DC24H-Csrf"] = session.apiCsrf;
    }
    if (method === "POST") {
      headers["Content-Type"] = "application/x-www-form-urlencoded";
      headers["Content-Length"] = String(Buffer.byteLength(body));
    }
    const call = request({ socketPath: config.socket, path, method, agent: false, headers }, (answer) => {
      const chunks: Buffer[] = [];
      let bytes = 0;
      answer.on("data", (part: Buffer) => {
        bytes += part.length;
        if (bytes > 65536) { call.destroy(new Error("upstream_size")); return; }
        chunks.push(part);
      });
      answer.on("end", () => {
        try {
          if (!String(answer.headers["content-type"] ?? "").startsWith("application/json")) throw Error();
          const value: unknown = JSON.parse(Buffer.concat(chunks).toString("utf8"));
          if (answer.statusCode !== 200) {
            const error = new Error("upstream_status");
            (error as Error & { status: number }).status = answer.statusCode ?? 503;
            reject(error);
          } else resolve(value);
        } catch { reject(new Error("upstream_invalid")); }
        for (const chunk of chunks) chunk.fill(0);
      });
    });
    call.setTimeout(4000, () => call.destroy(new Error("upstream_timeout")));
    call.on("error", reject);
    call.end(body);
  });
}

export function remoteGateway(config: Credentials | null, options: RemoteOptions) {
  if (!/^https:\/\/[A-Za-z0-9.-]+(?::[1-9]\d{1,4})?$/.test(options.origin)) throw new Error("HTTPS origin required");
  const clock = options.now ?? Date.now;
  const audit = options.audit ?? (() => {});
  const sessions = new Map<string, Session>();
  const challenges = new Map<string, Challenge>();
  const budgets = new Map<string, Budget>();

  const log = (event: RemoteEvent) => { audit(event); };
  const prune = (now: number) => {
    for (const [id, value] of sessions) {
      if (value.created + absoluteTime <= now || value.used + idleTime <= now) sessions.delete(id);
    }
    for (const [id, value] of challenges) if (value.expires <= now) challenges.delete(id);
    for (const [id, value] of budgets) if (value.starts + 60_000 <= now) budgets.delete(id);
  };
  const limited = (key: string, limit: number, now: number) => {
    const entry = budgets.get(key);
    if (!entry || entry.starts + 60_000 <= now) { budgets.set(key, { count: 1, starts: now }); return false; }
    entry.count++;
    return entry.count > limit;
  };
  const reply = (res: ServerResponse, status: number, data: unknown, cookies: string[] = []) => {
    if (res.writableEnded || res.destroyed) return;
    res.writeHead(status, {
      "Content-Type": "application/json; charset=utf-8", "Cache-Control": "no-store",
      "Pragma": "no-cache", "X-Content-Type-Options": "nosniff",
      "Content-Security-Policy": "default-src 'none'; frame-ancestors 'none'",
      "X-Frame-Options": "DENY", "Referrer-Policy": "no-referrer",
      ...(status === 429 ? { "Retry-After": "60" } : {}),
      ...(cookies.length ? { "Set-Cookie": cookies } : {}),
    });
    res.end(JSON.stringify(data));
  };
  const clearSession = () => [setCookie("dc24h_remote", "", 0)];
  const active = (req: IncomingMessage, now: number): [string, Session] | null => {
    const id = cookie(req, "dc24h_remote");
    if (!/^[0-9a-f]{64}$/.test(id)) return null;
    const session = sessions.get(id);
    if (!session || session.created + absoluteTime <= now || session.used + idleTime <= now) {
      sessions.delete(id);
      return null;
    }
    return [id, session];
  };
  const authorized = async (id: string, value: Session): Promise<"ok" | "revoked" | "unavailable"> => {
    if (!config) return "unavailable";
    try {
      const catalog = parseManagement(await upstream(config, "/api/v1/management", "GET", "", value));
      if (catalog.areas[0].allowed && catalog.areas[1].allowed) return "ok";
      sessions.delete(id);
      log("session_revoked");
      return "revoked";
    } catch (error) {
      const status = (error as { status?: number }).status;
      if (status === 401 || status === 403 || status === 404) {
        sessions.delete(id);
        log("session_revoked");
        return "revoked";
      }
      log("api_unavailable");
      return "unavailable";
    }
  };
  const readBody = async (req: IncomingMessage): Promise<unknown> => {
    const length = req.headers["content-length"];
    if (req.headers["transfer-encoding"] || !/^[1-9]\d{0,3}$/.test(String(length)) ||
        Number(length) > maxBody || req.headers["content-type"] !== "application/json") throw new Error("bad_body");
    const chunks: Buffer[] = [];
    let size = 0;
    try {
      for await (const part of req) {
        const chunk = part as Buffer;
        chunks.push(chunk);
        size += chunk.length;
        if (size > maxBody) throw new Error("bad_body");
      }
      if (size !== Number(length)) throw new Error("bad_body");
      return JSON.parse(Buffer.concat(chunks).toString("utf8")) as unknown;
    } finally { for (const chunk of chunks) chunk.fill(0); }
  };
  return async (req: IncomingMessage, res: ServerResponse) => {
    const path = req.url ?? "";
    const method = req.method ?? "";
    const now = clock();
    prune(now);
    const name = path.startsWith(prefix) ? path.slice(prefix.length) : "";
    const post = name === "login" || name === "logout";
    const valid = safePath.has(name) && (post ? method === "POST" : method === "GET");
    // No query strings, normalized paths, wildcard methods or forwarding.
    if (!valid) { log("route_refused"); reply(res, 404, { error: "not_found" }); return; }
    const host = req.headers.host ?? "";
    if (!loopback(req.socket.remoteAddress) ||
      !/^(127\.0\.0\.1|localhost|\[::1\])(:\d{1,5})?$/.test(host) ||
      (req.headers.origin && req.headers.origin !== options.origin) ||
      (req.headers["sec-fetch-site"] && !["same-origin", "none"].includes(String(req.headers["sec-fetch-site"]))) ||
      req.headers["x-dc24h-admin-grant"] || req.headers["x-dc24h-session"] ||
      req.headers["x-dc24h-csrf"] || untrustedProxyHeader(req) ||
      (post && req.headers.origin !== options.origin)) {
      log("route_refused"); reply(res, 403, { error: "forbidden" }); return;
    }
    if (!post && (req.headers["transfer-encoding"] ||
      (req.headers["content-length"] && req.headers["content-length"] !== "0"))) {
      reply(res, 400, { error: "bad_request" }); return;
    }
    if (name === "challenge") {
      if (limited("challenge", 20, now) || challenges.size >= 128) {
        reply(res, 429, { error: "rate_limited" }); return;
      }
      const id = randomBytes(32).toString("hex");
      const token = randomBytes(32).toString("hex");
      challenges.set(id, { token, expires: now + challengeTime });
      reply(res, 200, { schema: 1, csrf: token }, [setCookie("dc24h_remote_init", id, 120)]);
      return;
    }
    if (name === "login") {
      // A challenge is one-use even if request framing or JSON is invalid.
      const init = cookie(req, "dc24h_remote_init");
      const challenge = challenges.get(init);
      challenges.delete(init);
      let fields: Record<string, unknown>;
      try { fields = object(await readBody(req)); }
      catch { reply(res, 400, { error: "bad_request" }, [setCookie("dc24h_remote_init", "", 0)]); return; }
      const actor = fields.actor_id;
      const proof = fields.password;
      const csrf = req.headers["x-dc24h-remote-csrf"];
      if (typeof actor !== "string" || !/^[1-9]\d{0,18}$/.test(actor) ||
        BigInt(actor) > 9223372036854775807n ||
        typeof proof !== "string" || !proof.length || proof.length > 1024 ||
        Object.keys(fields).length !== 2 ||
        typeof csrf !== "string" || !challenge || challenge.expires <= now ||
        !sameSecret(challenge.token, csrf)) {
        log("login_refused"); reply(res, 403, { error: "login_refused" }, [setCookie("dc24h_remote_init", "", 0)]); return;
      }
      if (limited("login:global", 12, now) || limited("login:" + actor, 4, now)) {
        log("login_refused"); reply(res, 429, { error: "rate_limited" }); return;
      }
      if (!config) { reply(res, 503, { error: "unavailable" }); return; }
      try {
        const form = "actor_id=" + encodeURIComponent(actor) + "&proof=" + encodeURIComponent(proof);
        const issued = parseAccess(await upstream(config, "/api/v1/admin/access", "POST", form));
        if (!["administrator", "owner"].includes(issued.actor.role)) throw new Error("role_refused");
        // A subsequent GET rechecks the role against the actual database.
        const session: Session = {
          actor, grant: issued.grant, apiSession: issued.session, apiCsrf: issued.csrf,
          csrf: randomBytes(32).toString("hex"), created: now, used: now,
        };
        const key = randomBytes(32).toString("hex");
        const check = await authorized(key, session);
        if (check === "unavailable") {
          const fault = new Error("upstream_unavailable") as Error & { status: number };
          fault.status = 503;
          throw fault;
        }
        if (check !== "ok") throw new Error("authorization_refused");
        if (sessions.size >= maxSessions) sessions.delete(sessions.keys().next().value!);
        sessions.set(key, session);
        log("login_ok");
        reply(res, 200, { schema: 1, authenticated: true, mode: "read-only", csrf: session.csrf, expires_in: 600 },
          [setCookie("dc24h_remote", key, 600), setCookie("dc24h_remote_init", "", 0)]);
      } catch (error) {
        log("login_refused");
        const status = (error as { status?: number }).status;
        const outage = typeof status === "number" && status >= 500;
        reply(res, status === 429 ? 429 : outage ? 503 : 403,
          { error: status === 429 ? "rate_limited" : outage ? "unavailable" : "login_refused" },
          [setCookie("dc24h_remote_init", "", 0)]);
      }
      return;
    }
    const entry = active(req, now);
    if (!entry) { reply(res, 401, { error: "unauthorized" }, clearSession()); return; }
    const [key, session] = entry;
    if (name === "logout") {
      if (typeof req.headers["x-dc24h-remote-csrf"] !== "string" ||
          !sameSecret(session.csrf, req.headers["x-dc24h-remote-csrf"])) {
        reply(res, 403, { error: "csrf_required" }); return;
      }
      try { await readBody(req); } catch { reply(res, 400, { error: "bad_request" }); return; }
      sessions.delete(key);
      log("logout");
      reply(res, 200, { schema: 1, authenticated: false }, clearSession());
      return;
    }
    const check = await authorized(key, session);
    if (check === "revoked") { reply(res, 403, { error: "forbidden" }, clearSession()); return; }
    if (check !== "ok") { reply(res, 503, { error: "unavailable" }); return; }
    session.used = now;
    if (name === "session") {
      reply(res, 200, {
        schema: 1, authenticated: true, mode: "read-only", csrf: session.csrf,
        expires_in: Math.max(0, Math.floor(Math.min(session.created + absoluteTime - now, idleTime) / 1000)),
      });
      return;
    }
    try {
      if (!config) throw new Error("unavailable");
      const data = parseSnapshot(await upstream(config, "/api/v1/status", "GET"));
      // Intentionally no user list, identities, IPs, INI, audit, grants or
      // process counters beyond the explicitly reviewed aggregate fields.
      reply(res, 200, { schema: 1, mode: "read-only", server: {
        version: data.server.version, running: data.server.core_running,
        uptime_seconds: data.server.uptime_seconds,
      }, network: {
        active_connections: data.network.active_connections, accepted_connections: data.network.accepted_connections,
      }, users: { online: data.users.online, peak_online: data.users.peak_online },
      chat: { received: data.chat.received, rejected: data.chat.rejected },
      ...(data.search ? { search: { queries: data.search.statistics.queries_received,
        results: data.search.statistics.results_received } } : {}),
      ...(data.transfers ? { transfers: { ctm: data.transfers.statistics.ctm_received,
        rcm: data.transfers.statistics.rcm_received } } : {}),
      });
    } catch {
      log("api_unavailable"); reply(res, 503, { error: "unavailable" });
    }
  };
}
