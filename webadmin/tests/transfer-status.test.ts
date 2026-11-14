import assert from "node:assert/strict";
import { test } from "node:test";
import { parseSnapshot } from "../src/lib/status.ts";
import { TRANSFER_COUNTERS, parseTransfers } from "../src/lib/transfer-status.ts";
import { snapshot } from "./fixtures.ts";
const statistics = Object.fromEntries(TRANSFER_COUNTERS.map((key, i) => [key, i + 1]));
test("transfer extension preserves every safe counter and strips request data", () => {
  const value: { statistics: Record<string, unknown>; address: string } = { statistics: { ...statistics, token: "PRIVATE", sid: "AAAA" }, address: "PRIVATE" };
  const parsed = parseSnapshot({ ...snapshot, transfers: value, secret: "PRIVATE" });
  assert.deepEqual(parsed.transfers, { statistics });
  assert.ok(!JSON.stringify(parsed).includes("PRIVATE"));
  value.statistics.ctm_received = 999;
  assert.equal(parsed.transfers?.statistics.ctm_received, 1);
  assert.equal(parseSnapshot(snapshot).transfers, undefined);
});
test("transfer extension rejects absent, fractional, negative and unsafe counters", () => {
  for (const key of TRANSFER_COUNTERS) {
    for (const bad of [undefined, null, -1, .5, "0", NaN, Infinity, Number.MAX_SAFE_INTEGER + 1]) {
      assert.throws(() => parseTransfers({ statistics: { ...statistics, [key]: bad } }));
    }
  }
  for (const bad of [null, [], {}, { statistics: [] }]) assert.throws(() => parseTransfers(bad));
  const boundary = Object.fromEntries(TRANSFER_COUNTERS.map(key => [key, Number.MAX_SAFE_INTEGER]));
  assert.deepEqual(parseTransfers({ statistics: boundary }), { statistics: boundary });
});
