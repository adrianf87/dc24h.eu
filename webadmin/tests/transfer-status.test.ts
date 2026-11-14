import assert from "node:assert/strict";
import { test } from "node:test";
import { parseSnapshot } from "../src/lib/status.ts";
import { TRANSFER_COUNTERS, parseTransfers } from "../src/lib/transfer-status.ts";
import { snapshot } from "./fixtures.ts";
import { transferConfiguration } from "./fixtures.ts";
import { TRANSFER_FLAGS, TRANSFER_LIMITS, parseTransferConfiguration } from "../src/lib/transfer-status.ts";
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

test("effective transfer settings own allowlisted fields and preserve optional older APIs", () => {
  const input = { ...transferConfiguration, allowed_protocols: ["ADC/1.0", "ADCS/1.0", 'Future"\\Protocol/1.0'], secret: "PRIVATE" };
  const config = parseTransfers({ statistics, configuration: input }).configuration!;
  assert.deepEqual(Object.keys(config), [...TRANSFER_FLAGS, ...TRANSFER_LIMITS, "allowed_protocols"]);
  input.allowed_protocols[0] = "mutated";
  assert.deepEqual(config.allowed_protocols, ["ADC/1.0", "ADCS/1.0", 'Future"\\Protocol/1.0']);
  assert.ok(!JSON.stringify(config).includes("PRIVATE"));
  assert.equal(parseTransfers({ statistics }).configuration, undefined);
  assert.deepEqual(parseTransferConfiguration(transferConfiguration), transferConfiguration);
  for (const invalid of [null, [], {}, false]) assert.throws(() => parseTransfers({ statistics, configuration: invalid }));
});
test("every transfer flag and quota validates its own strict bounds", () => {
  for (const key of TRANSFER_FLAGS) for (const bad of [undefined, null, 0, 1, "on", "true"])
    assert.throws(() => parseTransferConfiguration({ ...transferConfiguration, [key]: bad }));
  for (const key of TRANSFER_LIMITS) {
    const min = key.endsWith("byte_burst") ? 16384 : 1, max = key.includes("byte") ? 16777216 : 1000000;
    for (const value of [min, max]) assert.equal(parseTransferConfiguration({ ...transferConfiguration, [key]: value })[key], value);
    for (const bad of [undefined, null, min - 1, max + 1, .5, -1, "1", Infinity, NaN, Number.MAX_SAFE_INTEGER + 1])
      assert.throws(() => parseTransferConfiguration({ ...transferConfiguration, [key]: bad }));
  }
});
test("transfer protocol settings use exact bounded names, any or an explicit empty list", () => {
  for (const protocols of ["any", [], ["ADC/1.0", "adc/1.0"], Array.from({ length: 16 }, (_, i) => "x".repeat(30) + String(i).padStart(2, "0"))])
    assert.deepEqual(parseTransferConfiguration({ ...transferConfiguration, allowed_protocols: protocols }).allowed_protocols, protocols);
  for (const protocols of [undefined, null, "none", "ADC/1.0", [""], ["any"], ["none"], ["ADC/1.0", "ADC/1.0"],
    ["ADC/1.0,ADCS/1.0"], [" ADC/1.0"], ["żółw/1.0"], ["x".repeat(33)], [123], Array.from({ length: 17 }, (_, i) => "P/" + i)])
    assert.throws(() => parseTransferConfiguration({ ...transferConfiguration, allowed_protocols: protocols }));
});
