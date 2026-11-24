import assert from "node:assert/strict";
import { test } from "node:test";
import { aggregateObservability } from "../server/remote-observability.ts";
import type { RemoteStatusProjection } from "../src/lib/status.ts";

const sample = (running: boolean, rejects: number, verified = true): RemoteStatusProjection => ({
  ...(verified ? { observation: { schema: 1, source_age_ms: 20 } } : {}),
  server: { version: "beta-0.6.16", running, uptime_seconds: 30 },
  network: { active_connections: 3, accepted_connections: 7, security_rejected: rejects },
  users: { online: 2, peak_online: 3 }, chat: { received: 4, rejected: 0 },
});

test("redacted incident timeline only exposes bounded fixed enums after verified samples", () => {
  let at = 10_000;
  const monitor = aggregateObservability(() => at);
  monitor.observe(sample(true, 0, false));
  assert.deepEqual(monitor.read(), { schema: 1, state: "unknown", events: [], alerts: [] });
  monitor.observe(sample(true, 0));
  at += 1000; monitor.observe(sample(false, 0));
  assert.deepEqual(monitor.read().events, [{ kind: "hub_stopped", age_seconds: 0 }]);
  for (let i = 0; i < 30; i++) { at += 1000; monitor.observe(sample(i % 2 === 0, 0)); }
  assert.equal(monitor.read().events.length, 12);
  for (const event of monitor.read().events) {
    assert.deepEqual(Object.keys(event).sort(), ["age_seconds", "kind"]);
  }
  assert.ok(!JSON.stringify(monitor.read()).includes("127.0.0.1"));
  at += 600_001;
  assert.deepEqual(monitor.read().events, [], "an old timeline cannot look like current status");
});

test("transport failure alerts are thresholded, cooldown-limited, and reset on clock reversal", () => {
  let at = 100_000;
  const monitor = aggregateObservability(() => at);
  monitor.observe(sample(true, 0));
  at += 1000; monitor.observe(sample(true, 4));
  assert.equal(monitor.read().alerts.length, 0);
  at += 1000; monitor.observe(sample(true, 9));
  assert.equal(monitor.read().alerts.length, 1);
  at += 1000; monitor.observe(sample(true, 100));
  assert.equal(monitor.read().alerts.length, 1, "cooldown prevents an alert storm");
  at += 61_000; monitor.observe(sample(true, 110));
  assert.equal(monitor.read().alerts.length, 2);
  at -= 80_000;
  assert.equal(monitor.read().state, "unknown");
  assert.deepEqual(monitor.read().alerts, []);
});
