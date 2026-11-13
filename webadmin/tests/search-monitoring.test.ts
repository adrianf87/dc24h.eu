import assert from "node:assert/strict";
import test from "node:test";
import { observeSearch, searchIntervals, SEARCH_HISTORY_LIMIT } from "../src/lib/search-monitoring.ts";
import { SEARCH_COUNTERS } from "../src/lib/search-status.ts";
import { parseSnapshot } from "../src/lib/status.ts";
import { snapshot, searchSnapshot } from "./fixtures.ts";

function data(changes: Record<string, number> = {}, uptime = 100) {
  return parseSnapshot({ ...snapshot, server: { ...snapshot.server, uptime_seconds: uptime },
    search: { ...searchSnapshot, statistics: { ...searchSnapshot.statistics, ...changes } } });
}
test("first observation is a baseline; intervals use monotonic time and independent deltas", () => {
  const first = observeSearch([], data(), 1000);
  assert.deepEqual(searchIntervals(first), []);
  const samples = observeSearch(first, data({ queries_received: 11, results_received: 31,
    queries_rejected: 8, queries_rate_limited: 9, results_rejected: 18,
    results_rate_limited: 19, results_recipient_limited: 20, query_queued: 12, result_queued: 27 }), 6000);
  assert.deepEqual(searchIntervals(samples), [{ at: 6000, seconds: 5, queries: 2, results: 4, refusals: 5, queued: 3 }]);
  const idle = observeSearch(samples, data({ ...samples[1].counters }), 11000);
  assert.deepEqual(searchIntervals(idle).at(-1), { at: 11000, seconds: 5, queries: 0, results: 0, refusals: 0, queued: 0 });
});
test("all cumulative counter regressions reset history, while gauge changes do not", () => {
  const first = observeSearch([], data(), 1000);
  for (const key of SEARCH_COUNTERS) {
    const reset = observeSearch(first, data({ [key]: 0 }), 6000);
    assert.equal(reset.length, 1, key);
    assert.deepEqual(searchIntervals(reset), []);
  }
  assert.equal(observeSearch(first, data({ tracked_searches: 0 }), 6000).length, 2);
});
test("uptime and version resets, missing extension and invalid clocks do not bridge windows", () => {
  const first = observeSearch([], data(), 1000);
  assert.equal(observeSearch(first, data({}, 1), 6000).length, 1);
  assert.equal(observeSearch(first, { ...data(), server: { ...data().server, version: "alpha-0.5.99" } }, 6000).length, 1);
  assert.deepEqual(observeSearch(first, parseSnapshot(snapshot), 6000), []);
  for (const at of [NaN, Infinity, -1]) assert.deepEqual(observeSearch(first, data(), at), []);
  for (const at of [999, 1000]) assert.equal(observeSearch(first, data(), at).length, 1);
});
test("rapid refreshes keep the baseline; long polling gaps begin a new one", () => {
  const first = observeSearch([], data(), 1000);
  assert.deepEqual(observeSearch(first, data({ queries_received: 6 }), 1999), first);
  assert.equal(searchIntervals(observeSearch(first, data({ queries_received: 6 }), 2000))[0].queries, 5);
  assert.equal(observeSearch(first, data(), 16000).length, 2);
  assert.equal(observeSearch(first, data(), 16001).length, 1);
});
test("history is bounded, owned and contains only allowlisted aggregates", () => {
  const value = data();
  let samples = observeSearch([], value, 0);
  value.search!.statistics.queries_received = 100;
  assert.equal(samples[0].counters.queries_received, 1);
  for (let n = 1; n <= 80; n++) samples = observeSearch(samples, data({ queries_received: n + 1 }), n * 1000);
  assert.equal(samples.length, SEARCH_HISTORY_LIMIT);
  assert.equal(searchIntervals(samples).length, SEARCH_HISTORY_LIMIT - 1);
  assert.equal(samples[0].at, 51000);
  assert.deepEqual(Object.keys(samples[0]).sort(), ["at", "counters", "uptime", "version"]);
  assert.deepEqual(Object.keys(samples[0].counters), [...SEARCH_COUNTERS]);
  assert.ok(!JSON.stringify(samples).includes("configuration"));
});
test("safe integer boundary uses deltas without subtracting rounded cumulative totals", () => {
  const first = observeSearch([], data({ queries_received: Number.MAX_SAFE_INTEGER - 5 }), 1000);
  assert.equal(searchIntervals(observeSearch(first, data({ queries_received: Number.MAX_SAFE_INTEGER }), 6000))[0].queries, 1);
});
