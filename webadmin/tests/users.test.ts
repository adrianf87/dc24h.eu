import assert from "node:assert/strict";
import { test } from "node:test";
import { decideSession, parseHubUsers } from "../src/lib/users.ts";
import { rolePolicy } from "./roles-fixture.ts";

const user = {
  session: "7",
  sid: "ABCD",
  nickname: "a\"b\\c",
  transport: "adcs",
  role: "guest",
  public_muted: true,
  private_muted: false,
};

test("user directory keeps public fields and drops secrets", () => {
  const parsed = parseHubUsers({ schema: 1, truncated: false, users: [{ ...user, password: "UserSecret", cid: "SECRET" }], extra: "UserSecret" });
  assert.deepEqual(parsed, { schema: 1, truncated: false, users: [user] });
  assert.equal(JSON.stringify(parsed).includes("UserSecret"), false);
  assert.throws(() => parseHubUsers({ schema: 1, truncated: false, users: [{ ...user, nickname: "bad\nname" }] }));
  assert.throws(() => parseHubUsers({ schema: 2, truncated: false, users: [] }));
  assert.throws(() => parseHubUsers({ schema: 1, truncated: false, users: [{ ...user, sid: "abc" }] }));
  const shared = parseHubUsers({
    schema: 1,
    truncated: false,
    users: [
      { ...user, session: "2222", sid: "ABCD" },
      { ...user, session: "3333", sid: "2222", nickname: "Other" },
    ],
  });
  assert.equal(shared.users.length, 2);
  assert.throws(() => parseHubUsers({ schema: 1, truncated: false, users: [user, { ...user, sid: "EFGH" }] }));
});

test("session decisions follow the shared lower-role rule", () => {
  const roles = rolePolicy();
  assert.equal(decideSession("moderator", "guest", "kick", roles), "allowed");
  assert.equal(decideSession("moderator", "guest", "mute", roles), "allowed");
  assert.equal(decideSession("guest", "guest", "kick", roles), "missing_permission");
  assert.equal(decideSession("moderator", "operator", "unmute", roles), "not_lower");
  assert.equal(decideSession("vip", "guest", "mute", roles), "missing_permission");
  assert.equal(decideSession("vip", "guest", "mute", rolePolicy(["users.mute"])), "allowed");
});
