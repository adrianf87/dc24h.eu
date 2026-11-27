// Fixed-path bridge to the privileged C++ Unix-socket API. There is no
// user-controlled forwarding, general-purpose proxy or browser service token.
import { request } from "node:http";
import { parseAccess } from "../src/lib/access.ts";
import { parseManagement } from "../src/lib/management.ts";
import { parseRemoteSummary } from "../src/lib/status.ts";
import { isJsonContentType } from "./json-content-type.ts";
import type { PublicAdminRole, PublicPasswordGrant } from "./public-two-factor.ts";

export type PublicBackendGrant = PublicPasswordGrant & {
  grant: string;
  session: string;
  csrf: string;
};
export type PublicBackendCredentials = { socket: string; token: string };

type APIPath = "/api/v1/admin/access" | "/api/v1/management" | "/api/v1/status";

export function publicBackend(config: PublicBackendCredentials) {
  const call = (path: APIPath, method: "GET" | "POST", body = "", grant?: PublicBackendGrant): Promise<unknown> =>
    new Promise((resolve, reject) => {
      const headers: Record<string, string> = {
        Host: "localhost", Authorization: "Bearer " + config.token,
        Accept: "application/json", Connection: "close",
      };
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
        agent: false, signal: AbortSignal.timeout(4000) }, res => {
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
            if (res.statusCode !== 200 || !isJsonContentType(res.headers["content-type"]))
              throw Error("upstream_denied");
            combined = Buffer.concat(chunks);
            resolve(JSON.parse(combined.toString("utf8")) as unknown);
          } catch { reject(Error("upstream_unavailable")); }
          finally { combined?.fill(0); wipe(); }
        });
      });
      upstream.setTimeout(4000, () => upstream.destroy(Error("upstream_timeout")));
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
  return {
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
  };
}

export type PublicBackend = ReturnType<typeof publicBackend>;
