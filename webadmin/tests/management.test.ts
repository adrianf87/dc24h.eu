import assert from "node:assert/strict";
import { test } from "node:test";
import { parseManagement } from "../src/lib/management.ts";

const catalog = {
  schema: 2,
  maintenance: true,
  restart_required: true,
  areas: [
    { key: "accounts", permission: "accounts.manage", allowed: false, implemented: true },
    { key: "roles", permission: "roles.manage", allowed: false, implemented: true },
    { key: "bans", permission: "users.ban+blocks.manage", allowed: true, implemented: true },
    { key: "configuration", permission: "configuration.manage", allowed: true, implemented: false },
    { key: "hub", permission: "hub.manage", allowed: false, implemented: false },
  ],
};

test("management catalog keeps five areas and drops secrets", () => {
  const parsed = parseManagement({ ...catalog, grant: "ab".repeat(32), actor: { id: "9" }, areas: catalog.areas.map((area) => ({ ...area, proof: "MgmtSecret" })) });
  assert.deepEqual(parsed, catalog);
  assert.equal(JSON.stringify(parsed).includes("MgmtSecret"), false);
  assert.equal(JSON.stringify(parsed).includes("\"id\""), false);
  assert.equal(parsed.areas[3].implemented, false);
  assert.equal(parsed.areas[4].implemented, false);
  assert.throws(() => parseManagement({ ...catalog, schema: 1 }));
  assert.throws(() => parseManagement({ ...catalog, restart_required: false }));
  assert.throws(() => parseManagement({ ...catalog, areas: catalog.areas.slice(0, 3) }));
  assert.throws(() => parseManagement({ ...catalog, areas: [...catalog.areas].reverse() }));
  assert.throws(() => parseManagement({ ...catalog, areas: catalog.areas.map((area) => ({ ...area, allowed: "true" })) }));
  assert.throws(() => parseManagement({
    ...catalog,
    areas: catalog.areas.map((area) => area.key === "configuration" ? { ...area, implemented: true } : area),
  }));
  assert.throws(() => parseManagement({
    ...catalog,
    areas: catalog.areas.map((area) => area.key === "hub" ? { ...area, implemented: true } : area),
  }));
});
