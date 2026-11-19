import {ACCOUNT_ROLES, PERMISSION_KEYS} from "./roles.ts";
import type {AccountRole, PermissionKey} from "./roles.ts";

const id = (value: unknown): value is string =>
  typeof value === "string" && /^[1-9]\d{0,18}$/.test(value) && BigInt(value) <= 9223372036854775807n;

export const ACCESS_ROUTES = [
  ["GET", "/api/v1/admin", ""],
  ["GET", "/api/v1/accounts", "accounts.manage"],
  ["POST", "/api/v1/accounts", "accounts.manage"],
  ["POST", "/api/v1/accounts/{id}/activate", "accounts.manage"],
  ["POST", "/api/v1/accounts/{id}/disable", "accounts.manage"],
  ["POST", "/api/v1/accounts/{id}/password", "accounts.password.change"],
  ["POST", "/api/v1/accounts/{id}/reset", "accounts.manage"],
  ["POST", "/api/v1/accounts/{id}/role", "roles.manage"],
  ["GET", "/api/v1/bans", "users.ban+blocks.manage"],
  ["POST", "/api/v1/bans", "users.ban+blocks.manage"],
  ["POST", "/api/v1/bans/{id}/revoke", "users.ban+blocks.manage"],
  ["POST", "/api/v1/audit", "audit.read"],
] as const;

export interface AccessPermission {key: PermissionKey; granted: boolean; implemented: boolean}
export interface AccessRoute {method: string; path: string; permission: string; allowed: boolean}
export interface AccessSnapshot {
  maintenance: true;
  restart_required: true;
  actor: {id: string; role: AccountRole; status: "active"};
  expires_in: 600;
  grant: string;
  session: string;
  csrf: string;
  permissions: AccessPermission[];
  routes: AccessRoute[];
}

function record(value: unknown): Record<string, unknown> {
  if (!value || typeof value !== "object" || Array.isArray(value)) throw new Error("Invalid administrative access");
  return value as Record<string, unknown>;
}

export function parseAccess(value: unknown): AccessSnapshot {
  const body = record(value);
  if (body.maintenance !== true || body.restart_required !== true || body.expires_in !== 600) throw new Error("Invalid administrative access");
  const actor = record(body.actor);
  if (!id(actor.id) || typeof actor.role !== "string" || !(ACCOUNT_ROLES as readonly string[]).includes(actor.role) || actor.status !== "active") {
    throw new Error("Invalid administrative access");
  }
  if (typeof body.grant !== "string" || !/^[0-9a-f]{64}$/.test(body.grant)) throw new Error("Invalid administrative access");
  if (typeof body.session !== "string" || !/^[0-9a-f]{32}$/.test(body.session)) throw new Error("Invalid administrative access");
  if (typeof body.csrf !== "string" || !/^[0-9a-f]{64}$/.test(body.csrf) || body.csrf === body.grant) throw new Error("Invalid administrative access");
  if (!Array.isArray(body.permissions) || body.permissions.length !== PERMISSION_KEYS.length || !Array.isArray(body.routes) || body.routes.length !== ACCESS_ROUTES.length) {
    throw new Error("Invalid administrative access");
  }
  const permissions = body.permissions.map((entry, index) => {
    const row = record(entry);
    const key = PERMISSION_KEYS[index];
    if (row.key !== key || typeof row.granted !== "boolean" || typeof row.implemented !== "boolean") throw new Error("Invalid administrative access");
    if ((key === "configuration.manage" || key === "hub.manage") && row.implemented !== false) throw new Error("Invalid administrative access");
    return {key, granted: row.granted, implemented: row.implemented};
  });
  const routes = body.routes.map((entry, index) => {
    const row = record(entry);
    const expected = ACCESS_ROUTES[index];
    if (!expected || row.method !== expected[0] || row.path !== expected[1] || row.permission !== expected[2] || typeof row.allowed !== "boolean") {
      throw new Error("Invalid administrative access");
    }
    return {method: expected[0], path: expected[1], permission: expected[2], allowed: row.allowed};
  });
  return {
    maintenance: true,
    restart_required: true,
    actor: {id: actor.id, role: actor.role as AccountRole, status: "active"},
    expires_in: 600,
    grant: body.grant,
    session: body.session,
    csrf: body.csrf,
    permissions,
    routes,
  };
}
