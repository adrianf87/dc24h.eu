import { test } from "node:test";
import assert from "node:assert/strict";
import { MODERATION_RULES, parseModeration } from "../src/lib/moderation.ts";
import { parseConfiguration } from "../src/lib/configuration.ts";
import { configuration } from "./configuration-fixture.ts";

test("moderation rules are exact and optional on older APIs", () => {
  assert.deepEqual(parseModeration(MODERATION_RULES.map((rule) => ({ ...rule, actor: "secret" }))), [...MODERATION_RULES]);
  assert.equal(parseConfiguration(configuration).moderation, undefined);
  assert.deepEqual(parseConfiguration({ ...configuration, moderation: MODERATION_RULES }).moderation, [...MODERATION_RULES]);
  const weakened = MODERATION_RULES.map((rule, index) => index === 0 ? { ...rule, forbid_self: false } : rule);
  assert.throws(() => parseModeration(weakened));
  assert.throws(() => parseModeration(MODERATION_RULES.slice(1)));
  assert.throws(() => parseConfiguration({ ...configuration, moderation: weakened }));
});
