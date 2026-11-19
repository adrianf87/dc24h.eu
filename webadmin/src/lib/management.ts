export interface ManagementArea {
  key: "accounts" | "roles" | "bans";
  permission: "accounts.manage" | "roles.manage" | "users.ban+blocks.manage";
  allowed: boolean;
}

export interface ManagementCatalog {
  schema: 1;
  maintenance: true;
  restart_required: true;
  areas: [ManagementArea, ManagementArea, ManagementArea];
}

const expected = [
  ["accounts", "accounts.manage"],
  ["roles", "roles.manage"],
  ["bans", "users.ban+blocks.manage"],
] as const;

function record(value: unknown): Record<string, unknown> {
  if (!value || typeof value !== "object" || Array.isArray(value)) throw new Error("Invalid management catalog");
  return value as Record<string, unknown>;
}

export function parseManagement(value: unknown): ManagementCatalog {
  const body = record(value);
  const rows = body.areas;
  if (body.schema !== 1 || body.maintenance !== true || body.restart_required !== true || !Array.isArray(rows) || rows.length !== expected.length) {
    throw new Error("Invalid management catalog");
  }
  const areas = expected.map(([key, permission], index): ManagementArea => {
    const area = record(rows[index]);
    if (area.key !== key || area.permission !== permission || typeof area.allowed !== "boolean") {
      throw new Error("Invalid management catalog");
    }
    return {key, permission, allowed: area.allowed};
  });
  const [accounts, roles, bans] = areas;
  if (!accounts || !roles || !bans) throw new Error("Invalid management catalog");
  return {schema: 1, maintenance: true, restart_required: true, areas: [accounts, roles, bans]};
}
