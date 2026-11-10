import assert from "node:assert/strict";
import { test } from "node:test";
import { parseSnapshot, duration } from "../src/lib/status.ts";
import { snapshot } from "./fixtures.ts";
test("accepts the C++ status contract and strips unrecognized fields", () => {
  assert.deepEqual(
    parseSnapshot({
      ...snapshot,
      secret: "never-forward",
      server: { ...snapshot.server, private_path: "never-forward" },
      users: { ...snapshot.users, cid: "never-forward" },
      network: { ...snapshot.network, peer_ip: "never-forward" },
    }),
    snapshot,
  );
  assert.equal(duration(3661), "1h 1m");
  assert.equal(duration(86400), "1d 0h");
  assert.equal(duration(0), "0s");
});
test("rejects incompatible schema, absent counters and imprecise or unsafe numbers", () => {
  for (const input of [
    null,
    [],
    {},
    { ...snapshot, api_version: "v2" },
    { ...snapshot, server: { ...snapshot.server, version: "<script>" } },
    { ...snapshot, database: { enabled: 1 } },
    ...[
      -1,
      NaN,
      Infinity,
      1.5,
      Number.MAX_SAFE_INTEGER + 1,
      "7",
      undefined,
    ].map((active_connections) => ({
      ...snapshot,
      network: { ...snapshot.network, active_connections },
    })),
  ]) {
    assert.throws(() => parseSnapshot(input));
  }
});

test("validates every new statistics counter without fabricating absent values", () => {
  for (const group of ["network", "users"] as const) {
    for (const field of Object.keys(snapshot[group])) {
      for (const value of [-1, 1.5, Number.MAX_SAFE_INTEGER + 1, undefined, "0"]) {
        assert.throws(() => parseSnapshot({
          ...snapshot, [group]: { ...snapshot[group], [field]: value },
        }));
      }
    }
  }
  assert.throws(() => parseSnapshot({ ...snapshot, users: undefined }));
});
