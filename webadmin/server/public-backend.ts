// Fixed-path bridge to the privileged C++ Unix-socket API. There is no
// user-controlled forwarding, general-purpose proxy or browser service token.
import { request } from "node:http";
import { parseAccess } from "../src/lib/access.ts";
import { parseManagement } from "../src/lib/management.ts";
import { parseRemoteSummary, parseSnapshot } from "../src/lib/status.ts";
import { parseHubUsers } from "../src/lib/users.ts";
import { parseTlsRenewal } from "../src/lib/tls-renewal.ts";
import type { TlsRenewalOperation } from "../src/lib/tls-renewal.ts";
import { isJsonContentType } from "./json-content-type.ts";
import type { Identity } from "./stage4-store.ts";
import type { PublicAdminRole, PublicPasswordGrant } from "./public-email-code.ts";
import type { PublicAdminOperation } from "./public-admin.ts";

export type PublicBackendGrant = PublicPasswordGrant & {
  grant: string;
  session: string;
  csrf: string;
};
export type PublicBackendCredentials = { socket: string; token: string };

export type HubRead = "health" | "ready" | "status" | "users";
type APIPath = "/api/v1/admin/access" | "/api/v1/management" | "/api/v1/status" |
  "/api/v1/stage4/identity-summary" | "/api/v1/stage4/admin" |
  "/api/v1/stage4/tls/renewal" | `/api/v1/stage4/hub/${HubRead}`;

export class PublicUpstreamError extends Error {
  constructor(readonly status: number, readonly code?: unknown) { super("upstream_denied"); }
}

type IdentitySummary = { schema: 1; source: "stage4-account-service";
  accounts: { total: number; active: number; owners: number; administrators: number; registered: number };
  verified_emails: number };

export function parseIdentitySummary(value: unknown): IdentitySummary {
  if (!value || typeof value !== "object" || Array.isArray(value)) throw Error("invalid_identity_summary");
  const result = value as Record<string, unknown>;
  const stats = result.accounts;
  if (result.schema !== 1 || result.source !== "stage4-account-service" ||
      !stats || typeof stats !== "object" || Array.isArray(stats)) throw Error("invalid_identity_summary");
  const counts = stats as Record<string, unknown>;
  const count = (entry: unknown) => {
    if (typeof entry !== "number" || !Number.isSafeInteger(entry) || entry < 0 || entry > 1_000_000_000)
      throw Error("invalid_identity_summary");
    return entry;
  };
  const total = count(counts.total), active = count(counts.active);
  const owners = count(counts.owners), administrators = count(counts.administrators);
  const registered = count(counts.registered), verified_emails = count(result.verified_emails);
  if (active > total || owners + administrators + registered > total || verified_emails > total)
    throw Error("invalid_identity_summary");
  return { schema: 1, source: "stage4-account-service",
    accounts: { total, active, owners, administrators, registered }, verified_emails };
}

export function publicBackend(config: PublicBackendCredentials) {
  const call = (path: APIPath, method: "GET" | "POST", body = "", grant?: PublicBackendGrant): Promise<unknown> =>
    new Promise((resolve, reject) => {
      const timeout = path === "/api/v1/stage4/tls/renewal" ? 22_000 : 4000;
      const headers: Record<string, string> = {
        Host: "localhost", Accept: "application/json", Connection: "close",
      };
      if (config.token) headers.Authorization = "Bearer " + config.token;
      if (grant) {
        headers["X-DC24H-Admin-Grant"] = grant.grant;
        headers["X-DC24H-Session"] = grant.session;
        headers["X-DC24H-Csrf"] = grant.csrf;
      }
      if (method === "POST") {
        headers["Content-Type"] = "application/x-www-form-urlencoded";
        headers["Content-Length"] = String(Buffer.byteLength(body));
      }
      const upstream = request({ socketPath: config.socket, path, method, headers,
        agent: false, signal: AbortSignal.timeout(timeout) }, res => {
        const chunks: Buffer[] = [];
        let size = 0;
        const wipe = () => { for (const chunk of chunks) chunk.fill(0); chunks.length = 0; };
        res.once("close", wipe);
        res.on("data", (part: Buffer) => {
          size += part.length;
          if (size > 65536) { upstream.destroy(Error("invalid_upstream")); return; }
          chunks.push(part);
        });
        res.on("end", () => {
          let combined: Buffer | undefined;
          try {
            if (!isJsonContentType(res.headers["content-type"])) throw Error("upstream_unavailable");
            combined = Buffer.concat(chunks);
            const data: unknown = JSON.parse(combined.toString("utf8"));
            if (res.statusCode !== 200) {
              const code = data && typeof data === "object" && "error" in data ? data.error : undefined;
              reject(new PublicUpstreamError(res.statusCode ?? 503, code));
            } else resolve(data);
          } catch { reject(Error("upstream_unavailable")); }
          finally { combined?.fill(0); wipe(); }
        });
      });
      upstream.setTimeout(timeout, () => upstream.destroy(Error("upstream_timeout")));
      upstream.on("error", reject);
      upstream.end(body);
    });

  const reauthorize = async (grant: PublicBackendGrant): Promise<boolean> => {
    try {
      const catalog = parseManagement(await call("/api/v1/management", "GET", "", grant));
      // A valid C++ grant is rechecked on every authenticated operation.
      return catalog.areas[0].allowed && catalog.areas[1].allowed;
    } catch { return false; }
  };
  const accountOperation = (values: Record<string, string>) =>
    call("/api/v1/admin/access", "POST", new URLSearchParams(values).toString());

  return {
    async prove(accountName: string, password: string): Promise<Identity | null> {
      try {
        const { account } = await accountOperation({ action: "prove", account_name: accountName, proof: password }) as
          { account: Identity };
        const decimal = (value: unknown) => typeof value === "string" && /^[1-9][0-9]{0,18}$/.test(value);
        return account && decimal(account.id) && decimal(account.revision) &&
          ["active", "disabled"].includes(account.status) ? account : null;
      } catch { return null; }
    },
    async prepare(accountName: string, password: string): Promise<string | null> {
      try {
        const { verifier } = await accountOperation({ action: "prepare", account_name: accountName, proof: password }) as
          { verifier: string };
        return typeof verifier === "string" &&
          /^\$dc24h-scrypt\$v=1,ln=17,r=8,p=1\$[0-9a-f]{64}\$[0-9a-f]{64}$/.test(verifier) ? verifier : null;
      } catch { return null; }
    },
    async change(identity: Identity, current: string, password: string): Promise<boolean> {
      try {
        const { changed } = await accountOperation({ action: "change", actor_id: identity.id,
          revision: identity.revision, proof: current, password }) as { changed: boolean };
        return changed === true;
      } catch { return false; }
    },
    async health(): Promise<boolean> {
      try { await call("/api/v1/status", "GET"); return true; }
      catch { return false; }
    },
    async verifyNamedPassword(accountName: string, password: string): Promise<PublicBackendGrant | null> {
      if (!accountName || Buffer.byteLength(accountName, "utf8") > 64 || !password || password.length > 1024) return null;
      try {
        const form = "account_name=" + encodeURIComponent(accountName) + "&proof=" + encodeURIComponent(password);
        const access = parseAccess(await call("/api/v1/admin/access", "POST", form));
        if (!["owner", "administrator"].includes(access.actor.role)) return null;
        const grant: PublicBackendGrant = {
          actor: access.actor.id, role: access.actor.role as PublicAdminRole,
          grant: access.grant, session: access.session, csrf: access.csrf,
        };
        return await reauthorize(grant) ? grant : null;
      } catch { return null; }
    },
    async verifyPassword(actor: string, password: string): Promise<PublicBackendGrant | null> {
      try {
        const form = "actor_id=" + encodeURIComponent(actor) + "&proof=" + encodeURIComponent(password);
        const access = parseAccess(await call("/api/v1/admin/access", "POST", form));
        if (access.actor.id !== actor || !["owner", "administrator"].includes(access.actor.role)) return null;
        const grant: PublicBackendGrant = {
          actor, role: access.actor.role as PublicAdminRole,
          grant: access.grant, session: access.session, csrf: access.csrf,
        };
        return await reauthorize(grant) ? grant : null;
      } catch { return null; }
    },
    reauthorize,
    async summary(grant: PublicBackendGrant) {
      const data = await call("/api/v1/status", "GET", "", grant);
      return parseRemoteSummary(data);
    },
    // Never present the auth process's synthetic /status as live hub metrics.
    // This route is implemented by the actual Stage IV C++ account service.
    async identitySummary() {
      return parseIdentitySummary(await call("/api/v1/stage4/identity-summary", "GET"));
    },
    async adminOperation(operation: PublicAdminOperation, values: Record<string, string>, sessionDigest: string) {
      return call("/api/v1/stage4/admin", "POST",
        new URLSearchParams({ ...values, operation, session_digest: sessionDigest }).toString());
    },
    async tlsRenewal(operation: TlsRenewalOperation, sessionDigest: string, proof?: string) {
      return parseTlsRenewal(await call("/api/v1/stage4/tls/renewal", "POST",
        new URLSearchParams({ operation, session_digest: sessionDigest, ...(proof === undefined ? {} : { proof }) }).toString()));
    },
    async hubRead(endpoint: HubRead) {
      const data = await call(`/api/v1/stage4/hub/${endpoint}`, "GET");
      if (endpoint === "status") {
        const status = parseSnapshot(data);
        if (status.configuration && status.configuration.mode !== "hub") throw Error("invalid_hub_status");
        return status;
      }
      if (endpoint === "users") {
        const { truncated, users } = parseHubUsers(data);
        return { truncated, users: users.map(({ nickname, role, transport, public_muted, private_muted }) =>
          ({ nickname, role, transport, public_muted, private_muted })) };
      }
      const status = endpoint === "health" ? "ok" : "ready";
      if (!data || typeof data !== "object" || !("status" in data) || data.status !== status)
        throw Error("invalid_hub_probe");
      return { status };
    },
  };
}

export type PublicBackend = ReturnType<typeof publicBackend>;
