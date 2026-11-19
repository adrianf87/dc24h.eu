import { ROLE_KEYS } from "./roles.ts";
import type { PermissionKey, RoleKey, RolesSnapshot } from "./roles.ts";

export interface HubUser {
  session: string;
  sid: string;
  nickname: string;
  transport: "adc" | "adcs";
  role: RoleKey;
  public_muted: boolean;
  private_muted: boolean;
}

export interface HubUsersDirectory {
  schema: 1;
  truncated: boolean;
  users: HubUser[];
}

export type SessionDecision = "allowed" | "missing_permission" | "not_lower";

const sessionId = /^(0|[1-9]\d{0,19})$/;
const sidText = /^[A-Z2-7]{4}$/;
const uint64Max = 18446744073709551615n;

function record(value: unknown): Record<string, unknown> {
  if (!value || typeof value !== "object" || Array.isArray(value)) throw new Error("Invalid user directory");
  return value as Record<string, unknown>;
}

function utf8Length(value: string): number {
  return new TextEncoder().encode(value).length;
}

function nickname(value: unknown): string {
  if (typeof value !== "string" || utf8Length(value) === 0 || utf8Length(value) > 64) throw new Error("Invalid user directory");
  for (const char of value) {
    const code = char.codePointAt(0) ?? 0;
    if (code < 0x20 || code === 0x7f) throw new Error("Invalid user directory");
  }
  return value;
}

function session(value: unknown): string {
  if (typeof value !== "string" || !sessionId.test(value) || BigInt(value) > uint64Max) throw new Error("Invalid user directory");
  return value;
}

export function parseHubUsers(value: unknown): HubUsersDirectory {
  const root = record(value);
  if (root.schema !== 1 || typeof root.truncated !== "boolean" || !Array.isArray(root.users) || root.users.length > 64) {
    throw new Error("Invalid user directory");
  }
  const sessions = new Set<string>();
  const sids = new Set<string>();
  const users = root.users.map((item) => {
    const row = record(item);
    const user: HubUser = {
      session: session(row.session),
      sid: typeof row.sid === "string" && sidText.test(row.sid) ? row.sid : "",
      nickname: nickname(row.nickname),
      transport: row.transport === "adc" || row.transport === "adcs" ? row.transport : "adc",
      role: (ROLE_KEYS as readonly string[]).includes(String(row.role)) ? row.role as RoleKey : "guest",
      public_muted: row.public_muted === true,
      private_muted: row.private_muted === true,
    };
    if (!user.sid || row.transport !== user.transport || row.role !== user.role || typeof row.public_muted !== "boolean" || typeof row.private_muted !== "boolean") {
      throw new Error("Invalid user directory");
    }
    if (sessions.has(user.session) || sids.has(user.sid)) throw new Error("Invalid user directory");
    sessions.add(user.session);
    sids.add(user.sid);
    return user;
  });
  return { schema: 1, truncated: root.truncated, users };
}

// Same lower-role rule as security::moderate for a session target. This preview
// cannot see whether the selected role owns the row, so self-target stays unchecked.
export function decideSession(actor: RoleKey, target: RoleKey, operation: "kick" | "mute" | "unmute", roles: RolesSnapshot): SessionDecision {
  const permission: PermissionKey = operation === "kick" ? "users.kick" : "users.mute";
  const actorRow = roles.catalog.find((item) => item.key === actor);
  const targetRow = roles.catalog.find((item) => item.key === target);
  if (!actorRow || !targetRow || !actorRow.permissions.includes(permission)) return "missing_permission";
  if (actorRow.rank <= targetRow.rank) return "not_lower";
  return "allowed";
}
