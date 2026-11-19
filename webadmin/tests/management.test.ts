import assert from "node:assert/strict";
import { test } from "node:test";
import { parseManagement } from "../src/lib/management.ts";

const catalog = {
  schema: 1,
  maintenance: true,
  restart_required: true,
  areas: [
    { key: "accounts", permission: "accounts.manage", allowed: false },
    { key: "roles", permission: "roles.manage", allowed: false },
    { key: "bans", permission: "users.ban+blocks.manage", allowed: true },
  ],
};

test("management catalog keeps the three areas and drops secrets", () => {
  const parsed = parseManagement({ ...catalog, grant: "ab".repeat(32), actor: { id: "9" }, areas: catalog.areas.map((area) => ({ ...area, proof: "MgmtSecret" })) });
  assert.deepEqual(parsed, catalog);
  assert.equal(JSON.stringify(parsed).includes("MgmtSecret"), false);
  assert.equal(JSON.stringify(parsed).includes("\"id\""), false);
  assert.throws(() => parseManagement({ ...catalog, schema: 2 }));
  assert.throws(() => parseManagement({ ...catalog, restart_required: false }));
  assert.throws(() => parseManagement({ ...catalog, areas: catalog.areas.slice(0, 2) }));
  assert.throws(() => parseManagement({ ...catalog, areas: [...catalog.areas].reverse() }));
  assert.throws(() => parseManagement({ ...catalog, areas: catalog.areas.map((area) => ({ ...area, allowed: "true" })) }));
});
