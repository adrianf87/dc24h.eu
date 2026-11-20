export interface ManagementArea {
  key: "accounts" | "roles" | "bans" | "configuration" | "hub";
  permission: "accounts.manage" | "roles.manage" | "users.ban+blocks.manage" | "configuration.manage" | "hub.manage";
  allowed: boolean;
  implemented: boolean;
}

export interface ManagementCatalog {
  schema: 2;
  maintenance: true;
  restart_required: true;
  areas: [ManagementArea, ManagementArea, ManagementArea, ManagementArea, ManagementArea];
}

const expected = [
  ["accounts", "accounts.manage", true],
  ["roles", "roles.manage", true],
  ["bans", "users.ban+blocks.manage", true],
  ["configuration", "configuration.manage", false],
  ["hub", "hub.manage", false],
] as const;

function record(value: unknown): Record<string, unknown> {
  if (!value || typeof value !== "object" || Array.isArray(value)) throw new Error("Invalid management catalog");
  return value as Record<string, unknown>;
}

export function parseManagement(value: unknown): ManagementCatalog {
  const body = record(value);
  const rows = body.areas;
  if (body.schema !== 2 || body.maintenance !== true || body.restart_required !== true || !Array.isArray(rows) || rows.length !== expected.length) {
    throw new Error("Invalid management catalog");
  }
  const areas = expected.map(([key, permission, implemented], index): ManagementArea => {
    const area = record(rows[index]);
    if (area.key !== key || area.permission !== permission || (area.allowed !== true && area.allowed !== false) || area.implemented !== implemented) {
      throw new Error("Invalid management catalog");
    }
    return {key, permission, allowed: area.allowed, implemented};
  });
  const [accounts, roles, bans, configuration, hub] = areas;
  if (!accounts || !roles || !bans || !configuration || !hub) throw new Error("Invalid management catalog");
  return {schema: 2, maintenance: true, restart_required: true, areas: [accounts, roles, bans, configuration, hub]};
}
