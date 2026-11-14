import assert from "node:assert/strict";
import test from "node:test";
import { observeTransfers, transferIntervals, TRANSFER_HISTORY_LIMIT } from "../src/lib/transfer-monitoring.ts";
import { TRANSFER_COUNTERS } from "../src/lib/transfer-status.ts";
import { parseSnapshot } from "../src/lib/status.ts";
import { snapshot } from "./fixtures.ts";

const statistics = Object.fromEntries(TRANSFER_COUNTERS.map((key, index) => [key, index + 1]));
function data(changes: Record<string, number> = {}, uptime = 100) {
  return parseSnapshot({ ...snapshot, server: { ...snapshot.server, uptime_seconds: uptime },
    transfers: { statistics: { ...statistics, ...changes } } });
}
test("transfer rates count attempts and copies separately, with idle intervals and no outcome-sum assumption", () => {
  const first = observeTransfers([], data(), 1000);
  assert.deepEqual(transferIntervals(first), []);
  const samples = observeTransfers(first, data({ ctm_received: 11, rcm_received: 31,
    ctm_rejected: 8, ctm_rate_limited: 9, ctm_recipient_limited: 10,
    rcm_rejected: 18, rcm_rate_limited: 19, rcm_recipient_limited: 20,
    ctm_queued: 12, rcm_queued: 27, ctm_errors: 999, ctm_refused: 888,
    peer_connection_failed: 777 }), 6000);
  assert.deepEqual(transferIntervals(samples), [{ at: 6000, seconds: 5, ctm: 2, rcm: 4, refusals: 6, queued: 3 }]);
  const idle = observeTransfers(samples, data(samples[1].counters), 11000);
  assert.deepEqual(transferIntervals(idle).at(-1), { at: 11000, seconds: 5, ctm: 0, rcm: 0, refusals: 0, queued: 0 });
});
test("a decrease in any transfer counter, including peer reports, starts a fresh baseline", () => {
  const first = observeTransfers([], data(), 1000);
  for (const key of TRANSFER_COUNTERS) {
    const reset = observeTransfers(first, data({ [key]: 0 }), 6000);
    assert.equal(reset.length, 1, key);
    assert.deepEqual(transferIntervals(reset), []);
  }
});
test("transfer monitoring resets on version, uptime, missing extension or invalid clock", () => {
  const first = observeTransfers([], data(), 1000);
  assert.equal(observeTransfers(first, data({}, 1), 6000).length, 1);
  assert.equal(observeTransfers(first, { ...data(), server: { ...data().server, version: "another-version" } }, 6000).length, 1);
  assert.deepEqual(observeTransfers(first, parseSnapshot(snapshot), 6000), []);
  for (const at of [NaN, Infinity, -1]) assert.deepEqual(observeTransfers(first, data(), at), []);
  assert.equal(observeTransfers(first, data(), 999).length, 1);
});
test("transfer sampling preserves rapid-refresh increments and enforces one/fifteen-second boundaries", () => {
  const first = observeTransfers([], data(), 1000);
  for (const at of [1000, 1999]) assert.deepEqual(observeTransfers(first, data({ ctm_received: 6 }), at), first);
  assert.equal(transferIntervals(observeTransfers(first, data({ ctm_received: 6 }), 2000))[0].ctm, 5);
  assert.equal(observeTransfers(first, data(), 16000).length, 2);
  assert.equal(observeTransfers(first, data(), 16001).length, 1);
});
test("transfer history owns only the 22 counters and remains bounded", () => {
  const value = data();
  let samples = observeTransfers([], value, 0);
  value.transfers!.statistics.ctm_received = 100;
  assert.equal(samples[0].counters.ctm_received, 1);
  for (let index = 1; index <= 80; index++) samples = observeTransfers(samples, data({ ctm_received: index + 1 }), index * 1000);
  assert.equal(samples.length, TRANSFER_HISTORY_LIMIT);
  assert.equal(transferIntervals(samples).length, TRANSFER_HISTORY_LIMIT - 1);
  assert.equal(samples[0].at, 51000);
  assert.deepEqual(Object.keys(samples[0]).sort(), ["at", "counters", "uptime", "version"]);
  assert.deepEqual(Object.keys(samples[0].counters), [...TRANSFER_COUNTERS]);
});
test("transfer rates subtract each safe integer before combining deltas", () => {
  const near = Object.fromEntries(TRANSFER_COUNTERS.map(key => [key, Number.MAX_SAFE_INTEGER - 5]));
  const high = Object.fromEntries(TRANSFER_COUNTERS.map(key => [key, Number.MAX_SAFE_INTEGER]));
  const samples = observeTransfers(observeTransfers([], data(near), 1000), data(high), 6000);
  assert.deepEqual(transferIntervals(samples), [{ at: 6000, seconds: 5, ctm: 1, rcm: 1, refusals: 6, queued: 2 }]);
});
