import assert from "node:assert/strict";
import {test} from "node:test";
import {adminHeaders, clearAdminGrant, setAdminAccess} from "../src/lib/adminAccess.ts";
import {ACCESS_ROUTES, parseAccess} from "../src/lib/access.ts";
import {PERMISSION_KEYS} from "../src/lib/roles.ts";

function body(patch: Record<string, unknown> = {}) {
  return {
    maintenance: true,
    restart_required: true,
    expires_in: 600,
    actor: {id: "7", role: "administrator", status: "active"},
    grant: "ab".repeat(32),
    session: "cd".repeat(16),
    csrf: "ef".repeat(32),
    permissions: PERMISSION_KEYS.map(key => ({key, granted: key !== "hub.manage", implemented: key !== "configuration.manage" && key !== "hub.manage"})),
    routes: ACCESS_ROUTES.map(([method, path, permission]) => ({method, path, permission, allowed: method !== "POST" || permission !== "accounts.manage"})),
    token: "hidden",
    ...patch,
  };
}

test("administrative access keeps the grant and drops transport secrets", () => {
  const parsed = parseAccess(body());
  assert.equal(parsed.grant, "ab".repeat(32));
  assert.equal(parsed.session, "cd".repeat(16));
  assert.equal(parsed.csrf, "ef".repeat(32));
  assert.equal(parsed.expires_in, 600);
  assert.equal(parsed.permissions[14].key, "configuration.manage");
  assert.equal(parsed.permissions[14].implemented, false);
  assert.equal(parsed.permissions[15].key, "hub.manage");
  assert.equal(parsed.permissions[15].granted, false);
  assert.equal(parsed.routes.length, 12);
  assert.equal("token" in parsed, false);
  assert.throws(() => parseAccess(body({grant: "AB".repeat(32)})));
  assert.throws(() => parseAccess(body({session: "CD".repeat(16)})));
  assert.throws(() => parseAccess(body({csrf: body().grant})));
  assert.throws(() => parseAccess(body({csrf: "zz"})));
  assert.throws(() => parseAccess(body({expires_in: 60})));
  const implemented = body();
  implemented.permissions[15].implemented = true;
  assert.throws(() => parseAccess(implemented));
  assert.throws(() => parseAccess(body({routes: body().routes.slice(1)})));
});

test("session secrets stay in page memory and are sent with the grant", () => {
  clearAdminGrant();
  assert.deepEqual(adminHeaders(), {});
  setAdminAccess({grant: "ab".repeat(32), session: "cd".repeat(16), csrf: "ef".repeat(32)});
  assert.deepEqual(adminHeaders(), {
    "X-DC24H-Admin-Grant": "ab".repeat(32),
    "X-DC24H-Session": "cd".repeat(16),
    "X-DC24H-Csrf": "ef".repeat(32),
  });
  clearAdminGrant();
  assert.deepEqual(adminHeaders(), {});
  setAdminAccess({grant: "ab".repeat(32), session: "CD".repeat(16), csrf: "ef".repeat(32)});
  assert.deepEqual(adminHeaders(), {});
});
