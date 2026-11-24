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
  at += 30_000; monitor.observe(sample(true, 100));
  at += 30_000; monitor.observe(sample(true, 110));
  assert.equal(monitor.read().alerts.length, 2);
  at -= 80_000;
  assert.equal(monitor.read().state, "unknown");
  assert.deepEqual(monitor.read().alerts, []);
});

test("authentication and rate-limit aggregates stay de-identified, thresholded and independent", () => {
  let at = 10_000;
  const monitor = aggregateObservability(() => at);
  for (let i = 0; i < 10; i++) monitor.observeAudit("login_refused");
  assert.deepEqual(monitor.read().alerts, [], "unverified status never exposes an incident");
  monitor.observe(sample(true, 0));
  for (let i = 0; i < 4; i++) monitor.observeAudit("login_refused");
  assert.deepEqual(monitor.read().alerts, []);
  monitor.observeAudit("mfa_refused");
  assert.deepEqual(monitor.read().alerts, [{ kind: "login_refusals", age_seconds: 0 }]);
  for (let i = 0; i < 20; i++) monitor.observeAudit("passkey_login_refused");
  assert.equal(monitor.read().alerts.length, 1, "one alert per cooldown");
  for (let i = 0; i < 5; i++) monitor.observeAudit("rate_limited");
  assert.deepEqual(monitor.read().alerts.map(a => a.kind), ["login_refusals", "rate_limit_refusals"]);
  assert.deepEqual(Object.keys(monitor.read().events.at(-1)!).sort(), ["age_seconds", "kind"]);
  assert.ok(!JSON.stringify(monitor.read()).includes("actor"));
  at += 30_000; monitor.observe(sample(true, 0));
  at += 31_000; monitor.observe(sample(true, 0));
  for (let i = 0; i < 5; i++) monitor.observeAudit("rate_limited");
  assert.equal(monitor.read().alerts.filter(a => a.kind === "rate_limit_refusals").length, 2);
  monitor.auditLost();
  assert.deepEqual(monitor.read(), { schema: 1, state: "unknown", events: [], alerts: [] });
});

test("verified timeline expires after 45 seconds without new source samples", () => {
  let at = 1_000;
  const monitor = aggregateObservability(() => at);
  monitor.observe(sample(true, 1));
  at += 1_000; monitor.observe(sample(false, 1));
  assert.equal(monitor.read().state, "observed");
  at += 45_001;
  assert.deepEqual(monitor.read(), { schema: 1, state: "unknown", events: [], alerts: [] });
  for (let n = 0; n < 20; n++) monitor.observeAudit("login_refused");
  assert.deepEqual(monitor.read().alerts, [], "stale audit signals cannot fabricate freshness");
  monitor.observe(sample(true, 2));
  assert.deepEqual(monitor.read().events, [], "first new generation sample is only a baseline");
});

test("legacy downgrade, malformed sample, unavailable source and counter reset drop old alerts", () => {
  let at = 30_000;
  const monitor = aggregateObservability(() => at);
  monitor.observe(sample(true, 1));
  at += 1_000; monitor.observe(sample(true, 10));
  assert.equal(monitor.read().alerts.length, 1);

  monitor.observe(sample(true, 10, false));
  assert.deepEqual(monitor.read(), { schema: 1, state: "unknown", events: [], alerts: [] });

  monitor.observe(sample(true, 10));
  at += 1_000; monitor.observe(sample(true, 20));
  assert.equal(monitor.read().alerts.length, 1);
  monitor.observe(sample(false, 0));
  assert.deepEqual(monitor.read().events, [], "reset counters must not join generations");

  monitor.observe({ ...sample(true, 10), observation: { schema: 1, source_age_ms: 10_001 } });
  assert.equal(monitor.read().state, "unknown");
  monitor.observe(sample(true, 10));
  monitor.sourceUnavailable();
  assert.equal(monitor.read().state, "unknown");
});

test("hub uptime reversal and monotonic reversal cannot fabricate a transition", () => {
  let at = 80_000;
  const monitor = aggregateObservability(() => at);
  monitor.observe({ ...sample(true, 2), server: { ...sample(true, 2).server, uptime_seconds: 100 } });
  at += 1_000;
  monitor.observe({ ...sample(false, 2), server: { ...sample(false, 2).server, uptime_seconds: 1 } });
  assert.deepEqual(monitor.read().events, [], "new hub uptime generation is not a confirmed stop");
  at -= 2_000;
  monitor.observeAudit("login_refused");
  assert.equal(monitor.read().state, "unknown");
});

test("real performance.now fractional milliseconds are valid monotonic receipts", () => {
  let at = 100.125;
  const monitor = aggregateObservability(() => at);
  monitor.observe(sample(true, 0));
  at += 5.375;
  monitor.observe(sample(false, 0));
  assert.deepEqual(monitor.read().events, [{ kind: "hub_stopped", age_seconds: 0 }]);
});
